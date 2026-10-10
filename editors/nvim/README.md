# YLang diagnostics for Neovim

This lightweight Neovim integration uses YLang's real ylang check command and Neovim's built-in diagnostic UI. It does not require an LSP client, null-ls, or another plugin.

## Features

- Marks syntax and semantic errors in the buffer with underlines, signs, and virtual text.
- Preserves YLang diagnostic codes and compiler hints.
- Checks the current unsaved buffer while typing, when opening a .yl file, and after saving.
- Provides :YLangCheck to check immediately and :YLangDiagnostics to show the current line's full message and hint.
- Runs the compiler without a shell and checks a temporary copy so unsaved text can be validated without touching the source file.

## Install from a YLang checkout

Add the plugin folder to Neovim's runtime path in ~/.config/nvim/init.lua:

~~~lua
vim.opt.rtp:prepend(vim.fn.expand("~/Documents/ylang-1.0/editors/nvim"))

vim.g.ylang_compiler_path = vim.fn.expand("~/Documents/ylang-1.0/build/ylang")
-- Optional settings:
vim.g.ylang_check_on_change = true
vim.g.ylang_check_on_save = true
vim.g.ylang_check_debounce_ms = 400
vim.g.ylang_check_timeout_ms = 10000
~~~

Adjust the paths if your YLang checkout is somewhere else. Restart Neovim and open a .yl file. The extension is loaded automatically for the ylang filetype.

If you use lazy.nvim, the plugin directory can also be loaded directly:

~~~lua
{
  dir = vim.fn.expand("~/Documents/ylang-1.0/editors/nvim"),
  name = "ylang-diagnostics",
  config = function()
    vim.g.ylang_compiler_path = vim.fn.expand("~/Documents/ylang-1.0/build/ylang")
  end,
}
~~~

## Use it

- :YLangCheck — run the compiler on the current buffer now.
- :YLangDiagnostics — open a floating diagnostic for the current line, including any compiler hint.
- :lopen after running :lua vim.diagnostic.setloclist() — inspect diagnostics in the location list.

You can also open Neovim's message area with :messages if the compiler cannot start or the check times out.

## Requirements

- Neovim 0.8 or newer.
- A built YLang compiler and GCC or Clang available for building YLang itself.

## Smoke test

Open a temporary file with intentionally invalid YLang, for example:

~~~ylang
function main() -> int {
    let int answer = "not an integer";
    return 0;
}
~~~

The let line should receive a diagnostic. Correct the type and the diagnostic should disappear after the next check.
