# Building and using YLang on Windows

Windows support is being developed on `dev/lsp-foundation`. The compiler is written in C and generates C17 before invoking GCC or Clang. The initial supported Windows toolchain is GCC from MSYS2 UCRT64 (MinGW-w64).

## 1. Install the toolchain

1. Install MSYS2 from [msys2.org](https://www.msys2.org/).
2. Open **MSYS2 UCRT64** from the Start menu.
3. Install GCC and Make:

   ```sh
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc make
   ```

4. Verify `gcc --version` and `make --version`.

For use from regular PowerShell or VS Code, add `C:\\msys64\\ucrt64\\bin` to your Windows user `PATH`. Adjust this path if MSYS2 was installed elsewhere.

## 2. Build YLang

In the MSYS2 UCRT64 terminal:

```sh
git clone https://github.com/Yassine-Jemi01/ylang.git
cd ylang
git switch dev/lsp-foundation
make clean all CC=gcc
make test CC=gcc
```

The compiler is created at `build/ylang.exe`. The Windows CI uses this toolchain and runs the regression suite.

## 3. Compile and run a program

```powershell
.\\build\\ylang.exe check .\\examples\\hello.yl
.\\build\\ylang.exe build .\\examples\\hello.yl -o .\\hello.exe
.\\hello.exe
```

Use an `.exe` suffix for the output name on Windows. GCC is the default native compiler.

## 4. VS Code

From the repository root, install the extension dependencies:

```powershell
cd editors\\vscode
npm install
npm run check
```

Set `ylang.compilerPath` in VS Code settings JSON to the absolute path of your compiler:

```json
"ylang.compilerPath": "C:\\\\path\\\\to\\\\ylang\\\\build\\\\ylang.exe"
```

Open a `.yl` file. Use **YLang: Check Current File** for diagnostics and **YLang: Build and Run Current File** to build and launch the program in an interactive integrated terminal.

## 5. Neovim

Install recent Neovim and ensure `gcc.exe` is on `PATH`. Close Neovim instances before replacing a parser DLL. From PowerShell at the repository root, run:

```powershell
.\\editors\\neovim\\install.ps1
```

Then open a source file:

```powershell
nvim .\\tree-sitter-ylang\\examples\\hello.yl
```

Inside Neovim, check `:set filetype?` (should say `filetype=ylang`) and run `:InspectTree`.

## Current boundaries

- The initial Windows toolchain is GCC/Clang compatible with MinGW-w64. MSVC is not supported by the generated runtime yet.
- YLang still uses its C backend; this work does not add an LLVM backend.
- Windows support is verified only when Windows CI passes.
