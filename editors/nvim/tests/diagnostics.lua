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

local v2 = diagnostics.parse_diagnostics({
  "error[E1022]: Imported source file could not be resolved or read.",
  "  --> /tmp/example.yl:2:1",
  "   |",
  '  2 | from "./math.yl" import add;',
  "   | ^",
  "error[E2063]: This match is not exhaustive.",
  "  --> /tmp/example.yl:8:5",
  "   |",
  "  8 | match (direction) {",
  "   |     ^",
  "error[E2077]: This struct has no field with that name.",
  "  --> /tmp/example.yl:10:15",
  "   |",
  " 10 | print(point.z);",
  "   |               ^",
})
assert(#v2 == 3, "expected YLang 2.0 diagnostics")
assert(v2[1].message:find("Imported file not found", 1, true), "import hint should be summarized")
assert(v2[2].message:find("match is not exhaustive", 1, true), "enum match diagnostics should be summarized")
assert(v2[3].message:find("Unknown struct field", 1, true), "struct diagnostics should be summarized")
print("YLang Neovim diagnostics tests passed")
vim.cmd("qa!")
