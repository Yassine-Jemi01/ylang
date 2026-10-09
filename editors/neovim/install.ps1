Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..")).Path
$grammarDir = Join-Path $repoRoot "tree-sitter-ylang"
$parserSource = Join-Path $grammarDir "src\parser.c"
$parserInclude = Join-Path $grammarDir "src"
$querySource = Join-Path $grammarDir "queries\highlights.scm"

function Get-NeovimPath([string] $kind) {
    $value = & nvim --headless -u NONE -c "lua io.write(vim.fn.stdpath('$kind'))" -c "qa!"
    if ($LASTEXITCODE -ne 0) {
        throw "Could not query Neovim's $kind path. Make sure Neovim is installed and recent enough."
    }
    return (($value -join "")).Trim()
}

function Copy-ManagedFile([string] $source, [string] $destination) {
    if (Test-Path -LiteralPath $destination) {
        $oldText = (Get-Content -LiteralPath $destination -Raw).Replace("`r`n", "`n")
        $newText = (Get-Content -LiteralPath $source -Raw).Replace("`r`n", "`n")
        if ($oldText -cne $newText) {
            throw "Refusing to overwrite a different config file: $destination. Back it up or merge it manually, then rerun."
        }
        return
    }
    $parent = Split-Path -Parent $destination
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination
}

if (-not (Get-Command nvim -ErrorAction SilentlyContinue)) {
    throw "Neovim was not found in PATH."
}
if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    throw "GCC was not found in PATH. Install a MinGW-w64 toolchain (for example MSYS2 UCRT64) and add its bin directory to PATH."
}
if (-not (Test-Path -LiteralPath $parserSource)) {
    throw "Generated Tree-sitter parser not found. Run npm install and npm run generate in tree-sitter-ylang."
}
if (-not (Test-Path -LiteralPath $querySource)) {
    throw "YLang Tree-sitter highlights query is missing."
}

& nvim --headless -u NONE -c 'lua assert(vim.treesitter and vim.treesitter.start and vim.treesitter.language.add, "Install a recent Neovim build with Tree-sitter support.")' -c "qa!"
if ($LASTEXITCODE -ne 0) { throw "This Neovim build does not expose the built-in Tree-sitter API." }

$dataRoot = Get-NeovimPath "data"
$configRoot = Get-NeovimPath "config"
$parserOut = Join-Path $dataRoot "site\parser\ylang.dll"
$queryOut = Join-Path $configRoot "queries\ylang\highlights.scm"
$filetypeOut = Join-Path $configRoot "ftdetect\ylang.lua"
$pluginOut = Join-Path $configRoot "after\plugin\ylang-treesitter.lua"

Copy-ManagedFile (Join-Path $scriptDir "ftdetect\ylang.lua") $filetypeOut
Copy-ManagedFile (Join-Path $scriptDir "after\plugin\ylang-treesitter.lua") $pluginOut
Copy-ManagedFile $querySource $queryOut

$parserDirectory = Split-Path -Parent $parserOut
New-Item -ItemType Directory -Force -Path $parserDirectory | Out-Null
$tempParser = Join-Path $env:TEMP ("ylang-parser-" + [guid]::NewGuid().ToString("N") + ".dll")
try {
    $gccArgs = @("-std=c11", "-O2", "-shared", "-static-libgcc", "-I", $parserInclude, $parserSource, "-o", $tempParser)
    & gcc @gccArgs
    if ($LASTEXITCODE -ne 0) { throw "GCC failed to compile the YLang Tree-sitter DLL." }
    Copy-Item -LiteralPath $tempParser -Destination $parserOut -Force
}
finally {
    if (Test-Path -LiteralPath $tempParser) { Remove-Item -LiteralPath $tempParser -Force }
}

$check = "local loaded, err = vim.treesitter.language.add('ylang', { path = vim.fn.stdpath('data') .. '/site/parser/ylang.dll' }); assert(loaded, tostring(err)); assert(vim.treesitter.query.get('ylang', 'highlights'))"
& nvim --headless -u NONE -c "lua $check" -c "qa!"
if ($LASTEXITCODE -ne 0) { throw "Neovim could not load the YLang DLL or highlighting query. Close all Neovim instances and check the compiler ABI." }

Write-Host "YLang Tree-sitter support installed for Neovim."
Write-Host "Parser: $parserOut"
Write-Host "Query:  $queryOut"
Write-Host "Open a .yl file and run :InspectTree to verify parsing."
