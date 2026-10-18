package main

import (
	"archive/zip"
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"regexp"
	"runtime"
	"sort"
	"strings"
	"time"
)

const (
	managerVersion   = "0.1.0"
	manifestFilename = "ylang.json"
	lockFilename     = "ylpm.lock"
	modulesDirectory = "yl_modules"
	maxArchiveBytes  = 64 << 20
	maxExpandedBytes = 256 << 20
	maxEntries       = 10000
)

var (
	namePattern = regexp.MustCompile("^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$")
	partPattern = regexp.MustCompile("^[A-Za-z0-9_.-]+$")
	tagPattern  = regexp.MustCompile("^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")
	httpClient  = &http.Client{Timeout: 60 * time.Second}
)

type Manifest struct {
	Name         string            `json:"name"`
	Version      string            `json:"version"`
	Entry        string            `json:"entry"`
	Dependencies map[string]string `json:"dependencies"`
}

type Lockfile struct {
	LockVersion int                      `json:"lockVersion"`
	Packages    map[string]LockedPackage `json:"packages"`
}

type LockedPackage struct {
	Source string `json:"source"`
	URL    string `json:"url"`
	SHA256 string `json:"sha256"`
}

func main() {
	if err := run(os.Args[1:], os.Stdout, os.Stderr); err != nil {
		fmt.Fprintln(os.Stderr, "ylpm:", err)
		os.Exit(1)
	}
}

func run(args []string, stdout, stderr io.Writer) error {
	if len(args) == 0 {
		printUsage(stderr)
		return errors.New("missing command")
	}
	root, err := os.Getwd()
	if err != nil {
		return fmt.Errorf("get current directory: %w", err)
	}
	switch args[0] {
	case "help", "--help", "-h":
		printUsage(stdout)
		return nil
	case "version", "--version", "-V":
		fmt.Fprintf(stdout, "ylpm %s\n", managerVersion)
		return nil
	case "init":
		if len(args) > 2 {
			return errors.New("usage: ylpm init [project-name]")
		}
		name := filepath.Base(root)
		if len(args) == 2 {
			name = args[1]
		}
		return initProject(root, name, stdout)
	case "add":
		if len(args) != 3 {
			return errors.New("usage: ylpm add <package-name> github:owner/repo@tag")
		}
		return addDependency(root, args[1], args[2], stdout)
	case "install":
		if len(args) != 1 {
			return errors.New("usage: ylpm install")
		}
		manifest, err := readManifest(root)
		if err != nil {
			return err
		}
		return installDependencies(context.Background(), root, manifest, stdout)
	case "remove":
		if len(args) != 2 {
			return errors.New("usage: ylpm remove <package-name>")
		}
		return removeDependency(root, args[1], stdout)
	case "list":
		if len(args) != 1 {
			return errors.New("usage: ylpm list")
		}
		return listDependencies(root, stdout)
	case "build":
		output, err := parseOutputFlag(args[1:])
		if err != nil {
			return err
		}
		manifest, err := readManifest(root)
		if err != nil {
			return err
		}
		_, err = buildProject(root, manifest, output, stdout, stderr)
		return err
	case "run":
		manifest, err := readManifest(root)
		if err != nil {
			return err
		}
		output, err := buildProject(root, manifest, "", stdout, stderr)
		if err != nil {
			return err
		}
		return runExecutable(root, output, args[1:], stdout, stderr)
	default:
		printUsage(stderr)
		return fmt.Errorf("unknown command %q", args[0])
	}
}

func printUsage(w io.Writer) {
	fmt.Fprintln(w, "ylpm — YLang package manager")
	fmt.Fprintln(w, "")
	fmt.Fprintln(w, "Usage:")
	fmt.Fprintln(w, "  ylpm init [project-name]")
	fmt.Fprintln(w, "  ylpm add <package-name> github:owner/repo@tag")
	fmt.Fprintln(w, "  ylpm install")
	fmt.Fprintln(w, "  ylpm remove <package-name>")
	fmt.Fprintln(w, "  ylpm list")
	fmt.Fprintln(w, "  ylpm build [--output path]")
	fmt.Fprintln(w, "  ylpm run [program-arguments...]")
	fmt.Fprintln(w, "  ylpm version")
	fmt.Fprintln(w, "")
	fmt.Fprintln(w, "Packages are pinned in ylpm.lock with a SHA-256 archive digest.")
	fmt.Fprintln(w, "Initial package sources use GitHub tags; no central registry is provided yet.")
}

func initProject(root, name string, stdout io.Writer) error {
	if !namePattern.MatchString(name) {
		return errors.New("project name must begin with a letter or number and contain only letters, numbers, '_' or '-'")
	}
	manifestPath := filepath.Join(root, manifestFilename)
	if _, err := os.Stat(manifestPath); err == nil {
		return fmt.Errorf("%s already exists", manifestFilename)
	} else if !errors.Is(err, os.ErrNotExist) {
		return fmt.Errorf("check manifest: %w", err)
	}
	manifest := Manifest{Name: name, Version: "0.1.0", Entry: "main.yl", Dependencies: map[string]string{}}
	if err := writeJSON(manifestPath, manifest); err != nil {
		return fmt.Errorf("write manifest: %w", err)
	}
	mainPath := filepath.Join(root, manifest.Entry)
	if _, err := os.Stat(mainPath); errors.Is(err, os.ErrNotExist) {
		source := "function main() -> int {\n    print(\"Hello from YLang\");\n    return 0;\n}\n"
		if err := os.WriteFile(mainPath, []byte(source), 0o644); err != nil {
			return fmt.Errorf("write starter source: %w", err)
		}
	} else if err != nil {
		return fmt.Errorf("check starter source: %w", err)
	}
	fmt.Fprintf(stdout, "Created %s and %s\n", manifestFilename, manifest.Entry)
	return nil
}

func readManifest(root string) (Manifest, error) {
	data, err := os.ReadFile(filepath.Join(root, manifestFilename))
	if err != nil {
		return Manifest{}, fmt.Errorf("read %s (run 'ylpm init' first): %w", manifestFilename, err)
	}
	var manifest Manifest
	if err := json.Unmarshal(data, &manifest); err != nil {
		return Manifest{}, fmt.Errorf("parse %s: %w", manifestFilename, err)
	}
	if !namePattern.MatchString(manifest.Name) {
		return Manifest{}, errors.New("manifest has an invalid project name")
	}
	if manifest.Entry == "" {
		manifest.Entry = "main.yl"
	}
	if filepath.IsAbs(manifest.Entry) {
		return Manifest{}, errors.New("manifest entry must be relative")
	}
	cleanEntry := filepath.Clean(manifest.Entry)
	if cleanEntry == "." || cleanEntry == ".." || strings.HasPrefix(cleanEntry, ".."+string(os.PathSeparator)) {
		return Manifest{}, errors.New("manifest entry must stay inside the project directory")
	}
	if manifest.Dependencies == nil {
		manifest.Dependencies = map[string]string{}
	}
	for name, source := range manifest.Dependencies {
		if !namePattern.MatchString(name) {
			return Manifest{}, fmt.Errorf("invalid dependency name %q", name)
		}
		if _, _, _, err := parseGitHubSource(source); err != nil {
			return Manifest{}, fmt.Errorf("dependency %q: %w", name, err)
		}
	}
	return manifest, nil
}

func readLockfile(root string) (Lockfile, error) {
	data, err := os.ReadFile(filepath.Join(root, lockFilename))
	if errors.Is(err, os.ErrNotExist) {
		return Lockfile{LockVersion: 1, Packages: map[string]LockedPackage{}}, nil
	}
	if err != nil {
		return Lockfile{}, fmt.Errorf("read %s: %w", lockFilename, err)
	}
	var lock Lockfile
	if err := json.Unmarshal(data, &lock); err != nil {
		return Lockfile{}, fmt.Errorf("parse %s: %w", lockFilename, err)
	}
	if lock.LockVersion != 1 {
		return Lockfile{}, fmt.Errorf("unsupported lockfile version %d", lock.LockVersion)
	}
	if lock.Packages == nil {
		lock.Packages = map[string]LockedPackage{}
	}
	return lock, nil
}

func writeJSON(filename string, value any) error {
	data, err := json.MarshalIndent(value, "", "  ")
	if err != nil {
		return err
	}
	data = append(data, '\n')
	return os.WriteFile(filename, data, 0o644)
}

func parseGitHubSource(source string) (owner, repo, tag string, err error) {
	payload := strings.TrimPrefix(source, "github:")
	if payload == source {
		return "", "", "", errors.New("source must use github:owner/repo@tag")
	}
	repoAndTag := strings.SplitN(payload, "@", 2)
	if len(repoAndTag) != 2 {
		return "", "", "", errors.New("GitHub source must pin a tag, e.g. github:owner/repo@v1.0.0")
	}
	parts := strings.Split(repoAndTag[0], "/")
	if len(parts) != 2 || !partPattern.MatchString(parts[0]) || !partPattern.MatchString(parts[1]) ||
		parts[1] == "." || parts[1] == ".." {
		return "", "", "", errors.New("expected GitHub source in owner/repo form")
	}
	tag = repoAndTag[1]
	if !tagPattern.MatchString(tag) || strings.Contains(tag, "..") {
		return "", "", "", errors.New("tag contains unsupported characters")
	}
	return parts[0], parts[1], tag, nil
}

func githubArchiveURL(source string) (string, error) {
	owner, repo, tag, err := parseGitHubSource(source)
	if err != nil {
		return "", err
	}
	return "https://codeload.github.com/" + owner + "/" + repo + "/zip/refs/tags/" + url.PathEscape(tag), nil
}

func addDependency(root, name, source string, stdout io.Writer) error {
	if !namePattern.MatchString(name) {
		return errors.New("invalid package name")
	}
	if _, _, _, err := parseGitHubSource(source); err != nil {
		return err
	}
	manifest, err := readManifest(root)
	if err != nil {
		return err
	}
	lock, err := readLockfile(root)
	if err != nil {
		return err
	}
	if current, ok := manifest.Dependencies[name]; ok && current == source {
		return fmt.Errorf("dependency %q is already present", name)
	}
	archiveURL, err := githubArchiveURL(source)
	if err != nil {
		return err
	}
	locked, err := fetchAndInstall(context.Background(), source, archiveURL, "", packageDirectory(root, name))
	if err != nil {
		return fmt.Errorf("install %q: %w", name, err)
	}
	manifest.Dependencies[name] = source
	lock.Packages[name] = locked
	if err := writeJSON(filepath.Join(root, lockFilename), lock); err != nil {
		return fmt.Errorf("write %s: %w", lockFilename, err)
	}
	if err := writeJSON(filepath.Join(root, manifestFilename), manifest); err != nil {
		return fmt.Errorf("write %s: %w", manifestFilename, err)
	}
	fmt.Fprintf(stdout, "Added %s (%s, sha256:%s)\n", name, source, locked.SHA256)
	return nil
}

func removeDependency(root, name string, stdout io.Writer) error {
	manifest, err := readManifest(root)
	if err != nil {
		return err
	}
	if _, ok := manifest.Dependencies[name]; !ok {
		return fmt.Errorf("dependency %q is not listed", name)
	}
	if err := os.RemoveAll(packageDirectory(root, name)); err != nil {
		return fmt.Errorf("remove installed package: %w", err)
	}
	delete(manifest.Dependencies, name)
	lock, err := readLockfile(root)
	if err != nil {
		return err
	}
	delete(lock.Packages, name)
	if err := writeJSON(filepath.Join(root, manifestFilename), manifest); err != nil {
		return err
	}
	if err := writeJSON(filepath.Join(root, lockFilename), lock); err != nil {
		return err
	}
	fmt.Fprintf(stdout, "Removed %s\n", name)
	return nil
}

func listDependencies(root string, stdout io.Writer) error {
	manifest, err := readManifest(root)
	if err != nil {
		return err
	}
	names := make([]string, 0, len(manifest.Dependencies))
	for name := range manifest.Dependencies {
		names = append(names, name)
	}
	sort.Strings(names)
	if len(names) == 0 {
		fmt.Fprintln(stdout, "No dependencies.")
		return nil
	}
	for _, name := range names {
		fmt.Fprintf(stdout, "%s  %s\n", name, manifest.Dependencies[name])
	}
	return nil
}

func installDependencies(ctx context.Context, root string, manifest Manifest, stdout io.Writer) error {
	lock, err := readLockfile(root)
	if err != nil {
		return err
	}
	next := Lockfile{LockVersion: 1, Packages: make(map[string]LockedPackage, len(manifest.Dependencies))}
	names := make([]string, 0, len(manifest.Dependencies))
	for name := range manifest.Dependencies {
		names = append(names, name)
	}
	sort.Strings(names)
	for _, name := range names {
		source := manifest.Dependencies[name]
		archiveURL, err := githubArchiveURL(source)
		if err != nil {
			return fmt.Errorf("dependency %q: %w", name, err)
		}
		expectedSHA := ""
		if old, ok := lock.Packages[name]; ok && old.Source == source && old.SHA256 != "" {
			archiveURL, expectedSHA = old.URL, old.SHA256
		}
		locked, err := fetchAndInstall(ctx, source, archiveURL, expectedSHA, packageDirectory(root, name))
		if err != nil {
			return fmt.Errorf("install %q: %w", name, err)
		}
		next.Packages[name] = locked
		fmt.Fprintf(stdout, "Installed %s (sha256:%s)\n", name, locked.SHA256)
	}
	return writeJSON(filepath.Join(root, lockFilename), next)
}

func packageDirectory(root, name string) string {
	return filepath.Join(root, modulesDirectory, name)
}

func fetchAndInstall(ctx context.Context, source, archiveURL, expectedSHA, destination string) (LockedPackage, error) {
	parsed, err := url.Parse(archiveURL)
	if err != nil || parsed.Scheme != "https" || parsed.Host == "" {
		return LockedPackage{}, errors.New("package archive URL must use HTTPS")
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, archiveURL, nil)
	if err != nil {
		return LockedPackage{}, fmt.Errorf("create download request: %w", err)
	}
	req.Header.Set("User-Agent", "ylpm/"+managerVersion)
	resp, err := httpClient.Do(req)
	if err != nil {
		return LockedPackage{}, fmt.Errorf("download archive: %w", err)
	}
	defer resp.Body.Close()
	if resp.Request.URL.Scheme != "https" {
		return LockedPackage{}, errors.New("package download redirected away from HTTPS")
	}
	if resp.StatusCode != http.StatusOK {
		return LockedPackage{}, fmt.Errorf("download archive: HTTP %s", resp.Status)
	}
	if resp.ContentLength > maxArchiveBytes {
		return LockedPackage{}, errors.New("package archive exceeds the 64 MiB download limit")
	}
	tmp, err := os.CreateTemp("", "ylpm-archive-*.zip")
	if err != nil {
		return LockedPackage{}, fmt.Errorf("create temporary archive: %w", err)
	}
	tmpName := tmp.Name()
	defer os.Remove(tmpName)
	hasher := sha256.New()
	size, err := io.Copy(io.MultiWriter(tmp, hasher), io.LimitReader(resp.Body, maxArchiveBytes+1))
	if err != nil {
		_ = tmp.Close()
		return LockedPackage{}, fmt.Errorf("save archive: %w", err)
	}
	if size > maxArchiveBytes {
		_ = tmp.Close()
		return LockedPackage{}, errors.New("package archive exceeds the 64 MiB download limit")
	}
	if err := tmp.Close(); err != nil {
		return LockedPackage{}, fmt.Errorf("close archive: %w", err)
	}
	digest := hex.EncodeToString(hasher.Sum(nil))
	if expectedSHA != "" && !strings.EqualFold(expectedSHA, digest) {
		return LockedPackage{}, fmt.Errorf("SHA-256 mismatch: expected %s, received %s", expectedSHA, digest)
	}
	if err := installZipAtomically(tmpName, destination); err != nil {
		return LockedPackage{}, err
	}
	return LockedPackage{Source: source, URL: archiveURL, SHA256: digest}, nil
}

func installZipAtomically(archivePath, destination string) error {
	parent := filepath.Dir(destination)
	if err := os.MkdirAll(parent, 0o755); err != nil {
		return fmt.Errorf("create package parent: %w", err)
	}
	stage, err := os.MkdirTemp(parent, ".ylpm-stage-")
	if err != nil {
		return fmt.Errorf("create staging directory: %w", err)
	}
	defer os.RemoveAll(stage)
	if err := extractSourceZip(archivePath, stage); err != nil {
		return fmt.Errorf("extract package: %w", err)
	}
	backup := destination + ".ylpm-backup"
	if err := os.RemoveAll(backup); err != nil {
		return fmt.Errorf("remove stale backup: %w", err)
	}
	hadOld := false
	if _, err := os.Stat(destination); err == nil {
		if err := os.Rename(destination, backup); err != nil {
			return fmt.Errorf("move previous package aside: %w", err)
		}
		hadOld = true
	} else if !errors.Is(err, os.ErrNotExist) {
		return fmt.Errorf("inspect installed package: %w", err)
	}
	if err := os.Rename(stage, destination); err != nil {
		if hadOld {
			_ = os.Rename(backup, destination)
		}
		return fmt.Errorf("activate package: %w", err)
	}
	if hadOld {
		if err := os.RemoveAll(backup); err != nil {
			return fmt.Errorf("remove previous package backup: %w", err)
		}
	}
	return nil
}

func extractSourceZip(archivePath, destination string) error {
	reader, err := zip.OpenReader(archivePath)
	if err != nil {
		return fmt.Errorf("open zip archive: %w", err)
	}
	defer reader.Close()
	if len(reader.File) == 0 {
		return errors.New("package archive is empty")
	}
	if len(reader.File) > maxEntries {
		return fmt.Errorf("archive contains more than %d entries", maxEntries)
	}
	root := ""
	var expanded int64
	sourceCount := 0
	for _, entry := range reader.File {
		raw := strings.TrimSuffix(entry.Name, "/")
		if raw == "" || strings.Contains(raw, "\\") || strings.HasPrefix(raw, "/") ||
			strings.Contains(raw, ":") {
			return fmt.Errorf("unsafe archive path %q", entry.Name)
		}
		for _, segment := range strings.Split(raw, "/") {
			if segment == "." || segment == ".." {
				return fmt.Errorf("unsafe archive path %q", entry.Name)
			}
		}
		clean := path.Clean(raw)
		if clean == "." || clean == ".." || strings.HasPrefix(clean, "../") {
			return fmt.Errorf("unsafe archive path %q", entry.Name)
		}
		parts := strings.Split(clean, "/")
		if root == "" {
			root = parts[0]
		}
		if parts[0] != root {
			return errors.New("archive has multiple top-level directories")
		}
		if len(parts) == 1 {
			if entry.FileInfo().IsDir() {
				continue
			}
			return errors.New("archive must contain files inside one top-level directory")
		}
		relative := path.Clean(strings.Join(parts[1:], "/"))
		if relative == "." || relative == ".." || strings.HasPrefix(relative, "../") {
			return fmt.Errorf("unsafe relative archive path %q", entry.Name)
		}
		if entry.Mode()&os.ModeSymlink != 0 {
			return fmt.Errorf("symbolic links are not allowed: %q", entry.Name)
		}
		if !entry.FileInfo().IsDir() && !entry.Mode().IsRegular() {
			return fmt.Errorf("unsupported file type: %q", entry.Name)
		}
		target := filepath.Join(destination, filepath.FromSlash(relative))
		rel, err := filepath.Rel(destination, target)
		if err != nil || rel == ".." || strings.HasPrefix(rel, ".."+string(os.PathSeparator)) || filepath.IsAbs(rel) {
			return fmt.Errorf("archive path escapes the package directory: %q", entry.Name)
		}
		if entry.FileInfo().IsDir() {
			if err := os.MkdirAll(target, 0o755); err != nil {
				return fmt.Errorf("create directory %q: %w", relative, err)
			}
			continue
		}
		if entry.UncompressedSize64 > uint64(maxExpandedBytes) ||
			expanded+int64(entry.UncompressedSize64) > maxExpandedBytes {
			return errors.New("expanded package exceeds the 256 MiB limit")
		}
		if err := os.MkdirAll(filepath.Dir(target), 0o755); err != nil {
			return fmt.Errorf("create parent for %q: %w", relative, err)
		}
		output, err := os.OpenFile(target, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0o644)
		if err != nil {
			return fmt.Errorf("create package source %q: %w", relative, err)
		}
		input, err := entry.Open()
		if err != nil {
			_ = output.Close()
			return fmt.Errorf("open archive member %q: %w", relative, err)
		}
		written, copyErr := io.Copy(output, io.LimitReader(input, maxExpandedBytes+1))
		inputErr := input.Close()
		outputErr := output.Close()
		if copyErr != nil {
			return fmt.Errorf("extract %q: %w", relative, copyErr)
		}
		if inputErr != nil {
			return fmt.Errorf("close archive member %q: %w", relative, inputErr)
		}
		if outputErr != nil {
			return fmt.Errorf("close extracted file %q: %w", relative, outputErr)
		}
		if written != int64(entry.UncompressedSize64) {
			return fmt.Errorf("size mismatch for archive member %q", relative)
		}
		expanded += written
		if strings.EqualFold(filepath.Ext(relative), ".yl") {
			sourceCount++
		}
	}
	if sourceCount == 0 {
		return errors.New("package contains no .yl source files")
	}
	return nil
}

func parseOutputFlag(args []string) (string, error) {
	output := ""
	for i := 0; i < len(args); i++ {
		switch args[i] {
		case "--output", "-o":
			if i+1 >= len(args) {
				return "", errors.New("--output requires a path")
			}
			output = args[i+1]
			i++
		default:
			return "", fmt.Errorf("unexpected build argument %q", args[i])
		}
	}
	return output, nil
}

func buildProject(root string, manifest Manifest, requestedOutput string, stdout, stderr io.Writer) (string, error) {
	output := requestedOutput
	if output == "" {
		name := manifest.Name
		if runtime.GOOS == "windows" {
			name += ".exe"
		}
		output = filepath.Join("build", name)
	}
	if !filepath.IsAbs(output) {
		output = filepath.Join(root, output)
	}
	if err := os.MkdirAll(filepath.Dir(output), 0o755); err != nil {
		return "", fmt.Errorf("create output directory: %w", err)
	}
	entry := manifest.Entry
	if !filepath.IsAbs(entry) {
		entry = filepath.Join(root, entry)
	}
	compiler := os.Getenv("YLANG_COMPILER")
	if compiler == "" {
		compiler = "ylang"
	}
	cmd := exec.Command(compiler, "build", entry, "-o", output)
	cmd.Dir = root
	cmd.Env = projectEnvironment(root)
	cmd.Stdout, cmd.Stderr = stdout, stderr
	if err := cmd.Run(); err != nil {
		return "", fmt.Errorf("YLang build failed: %w", err)
	}
	return output, nil
}

func runExecutable(root, output string, args []string, stdout, stderr io.Writer) error {
	absolute, err := filepath.Abs(output)
	if err != nil {
		return fmt.Errorf("resolve executable path: %w", err)
	}
	cmd := exec.Command(absolute, args...)
	cmd.Dir = root
	cmd.Env = projectEnvironment(root)
	cmd.Stdout, cmd.Stderr, cmd.Stdin = stdout, stderr, os.Stdin
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("program exited with an error: %w", err)
	}
	return nil
}

func projectEnvironment(root string) []string {
	paths := packageSourceDirectories(filepath.Join(root, modulesDirectory))
	paths = append(paths, filepath.SplitList(os.Getenv("YLANG_PATH"))...)
	unique := make([]string, 0, len(paths))
	seen := make(map[string]bool, len(paths))
	for _, item := range paths {
		if item == "" {
			continue
		}
		absolute, err := filepath.Abs(item)
		if err != nil {
			absolute = item
		}
		key := absolute
		if runtime.GOOS == "windows" {
			key = strings.ToLower(key)
		}
		if !seen[key] {
			seen[key] = true
			unique = append(unique, absolute)
		}
	}
	env := make([]string, 0, len(os.Environ())+1)
	for _, item := range os.Environ() {
		if !strings.HasPrefix(strings.ToUpper(item), "YLANG_PATH=") {
			env = append(env, item)
		}
	}
	env = append(env, "YLANG_PATH="+strings.Join(unique, string(os.PathListSeparator)))
	return env
}

func packageSourceDirectories(root string) []string {
	var result []string
	_ = filepath.WalkDir(root, func(current string, entry os.DirEntry, err error) error {
		if err != nil {
			return nil
		}
		if !entry.IsDir() {
			return nil
		}
		if current != root && strings.HasPrefix(entry.Name(), ".ylpm-") {
			return filepath.SkipDir
		}
		items, err := os.ReadDir(current)
		if err != nil {
			return nil
		}
		for _, item := range items {
			if !item.IsDir() && strings.EqualFold(filepath.Ext(item.Name()), ".yl") {
				result = append(result, current)
				break
			}
		}
		return nil
	})
	sort.Strings(result)
	return result
}
