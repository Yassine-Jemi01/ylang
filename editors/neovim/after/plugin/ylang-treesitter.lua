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
