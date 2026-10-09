[CmdletBinding()]
param(
    [ValidateSet("gcc", "clang")]
    [string] $Compiler = "gcc"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$version = (Get-Content -LiteralPath (Join-Path $repoRoot "VERSION") -Raw).Trim()
$buildDirectory = Join-Path $repoRoot "build"
$outputPath = Join-Path $buildDirectory "ylang.exe"
$includeDirectory = Join-Path $repoRoot "include"

$compilerCommand = Get-Command $Compiler -ErrorAction SilentlyContinue
if (-not $compilerCommand) {
    throw "Could not find '$Compiler'. Install GCC/Clang (MSYS2 UCRT64 GCC is supported) and add its bin directory to PATH."
}

$sources = @(
    Get-ChildItem -LiteralPath (Join-Path $repoRoot "src") -Filter "*.c" -File |
        Sort-Object Name |
        ForEach-Object { $_.FullName }
)
if ($sources.Count -eq 0) {
    throw "No C source files were found under src/."
}

New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null
$flags = @(
    "-std=c17",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Wconversion",
    "-Wshadow",
    "-Wstrict-prototypes",
    "-g3",
    "-O0",
    "-I$includeDirectory",
    ('-DYLANG_VERSION="{0}"' -f $version)
)

Write-Host "Building YLang $version for Windows with $($compilerCommand.Name)..."
& $compilerCommand.Source @flags @sources "-o" $outputPath
if ($LASTEXITCODE -ne 0) {
    throw "The C compiler failed with exit code $LASTEXITCODE."
}
if (-not (Test-Path -LiteralPath $outputPath -PathType Leaf)) {
    throw "Compiler exited successfully but did not create $outputPath."
}

Write-Host "Build succeeded: $outputPath"
