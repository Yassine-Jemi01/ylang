package main

import (
	"archive/zip"
	"bytes"
	"encoding/json"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
)

func makeZip(t *testing.T, members map[string]string) string {
	t.Helper()
	name := filepath.Join(t.TempDir(), "package.zip")
	file, err := os.Create(name)
	if err != nil {
		t.Fatal(err)
	}
	writer := zip.NewWriter(file)
	for member, contents := range members {
		entry, err := writer.Create(member)
		if err != nil {
			t.Fatal(err)
		}
		if _, err := entry.Write([]byte(contents)); err != nil {
			t.Fatal(err)
		}
	}
	if err := writer.Close(); err != nil {
		t.Fatal(err)
	}
	if err := file.Close(); err != nil {
		t.Fatal(err)
	}
	return name
}

func TestParseGitHubSource(t *testing.T) {
	owner, repo, tag, err := parseGitHubSource("github:example/math-lib@v1.2.3")
	if err != nil {
		t.Fatal(err)
	}
	if owner != "example" || repo != "math-lib" || tag != "v1.2.3" {
		t.Fatalf("unexpected source parse: %q %q %q", owner, repo, tag)
	}
	for _, source := range []string{
		"https://github.com/example/math-lib",
		"github:example/math-lib",
		"github:../math-lib@v1",
		"github:example/repo@../../evil",
		"github:example/repo:bad@v1",
		"github:example/repo/subdir@v1",
	} {
		t.Run(source, func(t *testing.T) {
			if _, _, _, err := parseGitHubSource(source); err == nil {
				t.Fatalf("expected invalid source %q to be rejected", source)
			}
		})
	}
}

func TestGitHubArchiveURL(t *testing.T) {
	got, err := githubArchiveURL("github:example/math-lib@v1.0.0")
	if err != nil {
		t.Fatal(err)
	}
	want := "https://codeload.github.com/example/math-lib/zip/refs/tags/v1.0.0"
	if got != want {
		t.Fatalf("got %q, want %q", got, want)
	}
}

func TestInitProjectCreatesManifestAndEntry(t *testing.T) {
	root := t.TempDir()
	var output bytes.Buffer
	if err := initProject(root, "hello-world", &output); err != nil {
		t.Fatal(err)
	}
	data, err := os.ReadFile(filepath.Join(root, manifestFilename))
	if err != nil {
		t.Fatal(err)
	}
	var manifest Manifest
	if err := json.Unmarshal(data, &manifest); err != nil {
		t.Fatal(err)
	}
	if manifest.Name != "hello-world" || manifest.Entry != "main.yl" {
		t.Fatalf("unexpected manifest: %#v", manifest)
	}
	if _, err := os.Stat(filepath.Join(root, "main.yl")); err != nil {
		t.Fatal(err)
	}
	if err := initProject(root, "hello-world", &output); err == nil {
		t.Fatal("expected duplicate init to fail")
	}
}

func TestExtractSourceZipCreatesFiles(t *testing.T) {
	archive := makeZip(t, map[string]string{
		"example-lib-v1/README.md": "readme",
		"example-lib-v1/src/math.yl": "function answer() -> int { return 42; }\n",
	})
	destination := t.TempDir()
	if err := extractSourceZip(archive, destination); err != nil {
		t.Fatal(err)
	}
	data, err := os.ReadFile(filepath.Join(destination, "src", "math.yl"))
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(string(data), "answer") {
		t.Fatalf("unexpected extracted source %q", data)
	}
}

func TestExtractSourceZipRejectsTraversal(t *testing.T) {
	archive := makeZip(t, map[string]string{
		"package/../outside.yl": "must not escape",
		"package/src/main.yl": "function main() -> int { return 0; }\n",
	})
	destination := t.TempDir()
	if err := extractSourceZip(archive, destination); err == nil {
		t.Fatal("expected traversal archive to be rejected")
	}
	if _, err := os.Stat(filepath.Join(destination, "outside.yl")); !os.IsNotExist(err) {
		t.Fatalf("unexpected outside file after failed extraction: %v", err)
	}
}

func TestExtractSourceZipRejectsNonSourcePackage(t *testing.T) {
	archive := makeZip(t, map[string]string{
		"package/README.md": "documentation only",
	})
	if err := extractSourceZip(archive, t.TempDir()); err == nil {
		t.Fatal("expected a package without .yl files to be rejected")
	}
}

func TestReadManifestRejectsPathTraversal(t *testing.T) {
	root := t.TempDir()
	manifest := Manifest{
		Name: "example",
		Entry: "../outside.yl",
		Dependencies: map[string]string{},
	}
	if err := writeJSON(filepath.Join(root, manifestFilename), manifest); err != nil {
		t.Fatal(err)
	}
	if _, err := readManifest(root); err == nil {
		t.Fatal("expected traversal entry to be rejected")
	}
}

func TestProjectEnvironmentUsesPlatformPathSeparator(t *testing.T) {
	root := t.TempDir()
	module := filepath.Join(root, modulesDirectory, "lib")
	if err := os.MkdirAll(module, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(module, "main.yl"), []byte(""), 0o644); err != nil {
		t.Fatal(err)
	}
	t.Setenv("YLANG_PATH", "")
	env := projectEnvironment(root)
	var value string
	for _, item := range env {
		if strings.HasPrefix(strings.ToUpper(item), "YLANG_PATH=") {
			value = item[len("YLANG_PATH="):]
			break
		}
	}
	absolute, err := filepath.Abs(module)
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(value, absolute) {
		t.Fatalf("module directory missing from YLANG_PATH: %q", value)
	}
	if runtime.GOOS == "windows" && len(filepath.SplitList(value)) != 1 {
		t.Fatalf("expected one Windows path-list entry, got %q", value)
	}
}

func TestParseOutputFlag(t *testing.T) {
	got, err := parseOutputFlag([]string{"--output", "build/app.exe"})
	if err != nil {
		t.Fatal(err)
	}
	if got != "build/app.exe" {
		t.Fatalf("got output %q", got)
	}
	for _, args := range [][]string{{"--output"}, {"--bogus"}} {
		if _, err := parseOutputFlag(args); err == nil {
			t.Fatalf("expected error for %v", args)
		}
	}
}
