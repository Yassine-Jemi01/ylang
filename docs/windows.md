# Building and testing YLang 2 on Windows

YLang 2.0.0-dev is developed on the `v2/core` branch. The compiler is written in C and emits C17 before invoking GCC or Clang. The initial Windows toolchain is GCC from MSYS2 UCRT64 (MinGW-w64). The stable `main` branch remains YLang 1.0.0.

## 1. Install the toolchain

1. Install MSYS2 from [msys2.org](https://www.msys2.org/).
2. Open **MSYS2 UCRT64** from the Start menu.
3. Install GCC and Make:

   ```sh
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc make
   ```

4. Verify `gcc --version` and `make --version`.

For use from regular PowerShell or VS Code, add `C:\msys64\ucrt64\bin` to your Windows user PATH. Adjust this path if MSYS2 was installed elsewhere.

## 2. Build the development branch

```sh
git clone https://github.com/Yassine-Jemi01/ylang.git
cd ylang
git switch v2/core
make clean all CC=gcc
make test CC=gcc
```

The compiler is created at `build/ylang.exe`. The test suite covers existing stable examples and the v2 preview cases for arrays, move-after-use checking, loops, input, and bounds errors.

## 3. Build from PowerShell

After adding `C:\msys64\ucrt64\bin` to your Windows PATH:

```powershell
.\scripts\windows\build.ps1
.\build\ylang.exe --version
.\scripts\windows\smoke-test.ps1
```

The PowerShell script compiles the C compiler directly. The smoke test checks the version, validates a source file, builds a native Windows executable, executes it, and compares its output. The complete regression suite runs through `make test` inside MSYS2 UCRT64.

## 4. Compile a program

```powershell
.\build\ylang.exe check .\examples\demo.yl
.\build\ylang.exe build .\examples\demo.yl -o .\demo.exe
.\build\ylang.exe run .\examples\demo.yl
.\demo.exe
```

Use an `.exe` suffix for native Windows output. GCC is the default compiler.

## 5. VS Code

```powershell
cd editors\vscode
npm install
npm run check
npx --yes @vscode/vsce package
```

Set `ylang.compilerPath` in VS Code settings JSON to the absolute path of `build\ylang.exe`. The extension provides compiler-backed diagnostics, basic completion and navigation, and an interactive build/run command. Its language-server analysis is still a lightweight foundation, not a complete semantic implementation.

## 6. Neovim and Tree-sitter

From PowerShell at the repository root:

```powershell
.\editors\neovim\install.ps1
```

Tree-sitter grammar generation and corpus tests run in CI. Because the generated parser is a checked-in artifact, regenerate it when the grammar changes and verify `:InspectTree` with v2 sample files before calling the editor integration fully updated.

## Current boundaries

- The supported Windows native compiler is GCC from MSYS2 UCRT64 / MinGW-w64; MSVC is not supported yet.
- The language front-end is YLang-specific, but machine-code generation still goes through the emitted C17 backend.
- Windows support means the compiler, generated executables, tests, and editor packaging all pass CI; check the current workflow results before relying on an unmerged commit.
- This preview is not a hardened sandbox and does not claim Rust-equivalent memory safety.
