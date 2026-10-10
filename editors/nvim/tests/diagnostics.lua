vim.opt.rtp:prepend(vim.fn.fnamemodify(vim.fn.getcwd(), ":p") .. "/editors/nvim")
local diagnostics = require("ylang.diagnostics")

local parsed = diagnostics.parse_diagnostics({
  "error[E1002]: Expected ';' after variable declaration.",
  "  --> /tmp/example.yl:12:30",
  "   |",
  " 12 |     let string file_path = io.read_line();",
  "   |                              ^",
  "   = help: End the declaration with a semicolon.",
  "ylang: check failed with 1 error(s).",
})

assert(#parsed == 1, "expected one diagnostic")
assert(parsed[1].lnum == 11, "diagnostic line should be zero-based")
assert(parsed[1].col == 29, "diagnostic column should be zero-based")
assert(parsed[1].source == "YLang", "diagnostic should identify its source")
assert(parsed[1].code == "E1002", "diagnostic code should be preserved")
assert(parsed[1].message:find("Hint: End the declaration with a semicolon.", 1, true),
  "compiler hints should be preserved")
print("YLang Neovim diagnostics tests passed")
vim.cmd("qa!")
