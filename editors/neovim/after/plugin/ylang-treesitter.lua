local parser_extension = vim.fn.has("win32") == 1 and "dll" or "so"
local parser_path = vim.fn.stdpath("data") .. "/site/parser/ylang." .. parser_extension
local call_ok, loaded, load_error = pcall(
  vim.treesitter.language.add,
  "ylang",
  { path = parser_path }
)

if not call_ok or not loaded then
  vim.notify(
    "YLang Tree-sitter parser could not be loaded from " .. parser_path .. ": " .. tostring(load_error or loaded),
    vim.log.levels.WARN
  )
  return
end

vim.api.nvim_create_autocmd("FileType", {
  group = vim.api.nvim_create_augroup("YLangTreeSitter", { clear = true }),
  pattern = "ylang",
  callback = function(args)
    local ok, err = pcall(vim.treesitter.start, args.buf, "ylang")
    if not ok then
      vim.notify(
        "YLang Tree-sitter highlighting failed: " .. tostring(err),
        vim.log.levels.WARN
      )
    end
  end,
})
