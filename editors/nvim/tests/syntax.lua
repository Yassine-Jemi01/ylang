vim.opt.rtp:prepend(vim.fn.fnamemodify(vim.fn.getcwd(), ":p") .. "/editors/nvim")
vim.cmd("syntax on")
vim.cmd("new")
vim.api.nvim_buf_set_lines(0, 0, -1, false, {
  "struct Point {",
  "    int x;",
  "}",
  "enum Direction { North, East }",
  'from "./math.yl" import add;',
  "function main() -> int {",
  "    let const float ratio = 2.5e-4;",
  "    match (direction) { Direction.North -> { break; } }",
  "    return 0;",
  "}",
})
vim.cmd("runtime! syntax/ylang.vim")

local function group_at(line_number, token)
  local line = vim.api.nvim_buf_get_lines(0, line_number - 1, line_number, false)[1]
  local start = assert(line:find(token, 1, true), "missing token " .. token)
  return vim.fn.synIDattr(vim.fn.synID(line_number, start, 1), "name")
end

assert(group_at(1, "struct") == "ylangDeclaration", "struct keyword should be highlighted")
assert(group_at(1, "Point") == "ylangDeclaredType", "declared struct name should be highlighted")
assert(group_at(4, "enum") == "ylangDeclaration", "enum keyword should be highlighted")
assert(group_at(4, "North") == "ylangEnumVariant", "enum variants should be highlighted")
assert(group_at(5, "from") == "ylangDeclaration", "module imports should be highlighted")
assert(group_at(6, "main") == "ylangFunctionDeclaration", "function declarations should be highlighted")
assert(group_at(7, "ratio") == "ylangVariable", "typed variable names should be highlighted")
assert(group_at(7, "2.5e-4") == "ylangNumber", "exponent floats should be highlighted")
assert(group_at(8, "match") == "ylangDeclaration", "match should be highlighted")
assert(group_at(8, "North") == "ylangMember", "qualified enum members should be highlighted")
print("YLang Neovim 2.0 syntax tests passed")
vim.cmd("qa!")
