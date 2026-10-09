[CmdletBinding()]
param(
    [string] $CompilerPath = (Join-Path $PSScriptRoot "..\..\build\ylang.exe")
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$compiler = (Resolve-Path -LiteralPath $CompilerPath).Path
$version = (Get-Content -LiteralPath (Join-Path $repoRoot "VERSION") -Raw).Trim()
$buildDirectory = Join-Path $repoRoot "build"
$outputPath = Join-Path $buildDirectory "ylang-windows-smoke.exe"
$newline = [string][char]10
$carriageReturn = [string][char]13
$crlf = $carriageReturn + $newline

$versionOutput = (& $compiler "--version" | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $versionOutput -notmatch [regex]::Escape("YLang compiler $version")) {
    throw "Version smoke test failed: $versionOutput"
}

& $compiler "check" (Join-Path $repoRoot "examples\demo.yl")
if ($LASTEXITCODE -ne 0) { throw "YLang check smoke test failed." }

& $compiler "build" (Join-Path $repoRoot "examples\demo.yl") "-o" $outputPath
if ($LASTEXITCODE -ne 0) { throw "YLang native build smoke test failed." }
if (-not (Test-Path -LiteralPath $outputPath -PathType Leaf)) {
    throw "YLang did not create the expected Windows executable: $outputPath"
}

$actualLines = & $outputPath
if ($LASTEXITCODE -ne 0) { throw "The generated Windows executable returned exit code $LASTEXITCODE." }
$actual = (($actualLines -join $newline) -replace $carriageReturn, "").TrimEnd($newline)
$expected = [System.IO.File]::ReadAllText((Join-Path $repoRoot "tests\expected-demo.txt"))
$expected = ($expected -replace $crlf, $newline).TrimEnd($newline)
if ($actual -cne $expected) {
    throw "Generated Windows program output did not match tests/expected-demo.txt. Expected: [$expected] Actual: [$actual]"
}

Write-Host "Windows smoke tests passed (version, check, native .exe build, execution, output)."
