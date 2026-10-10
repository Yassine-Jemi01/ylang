if vim.g.loaded_ylang_editor_diagnostics then
  return
end
vim.g.loaded_ylang_editor_diagnostics = true

vim.filetype.add({ extension = { yl = "ylang" } })

local diagnostics = require("ylang.diagnostics")
diagnostics.setup()

local group = vim.api.nvim_create_augroup("YLangEditorDiagnostics", { clear = true })

vim.api.nvim_create_autocmd("FileType", {
  group = group,
  pattern = "ylang",
  callback = function(event)
    diagnostics.schedule(event.buf, 100)
  end,
})

vim.api.nvim_create_autocmd({ "TextChanged", "TextChangedI" }, {
  group = group,
  callback = function(event)
    if vim.bo[event.buf].filetype == "ylang" and vim.g.ylang_check_on_change ~= false then
      diagnostics.schedule(event.buf, tonumber(vim.g.ylang_check_debounce_ms) or 400)
    end
  end,
})

vim.api.nvim_create_autocmd("BufWritePost", {
  group = group,
  callback = function(event)
    if vim.bo[event.buf].filetype == "ylang" and vim.g.ylang_check_on_save ~= false then
      diagnostics.schedule(event.buf, 0)
    end
  end,
})

vim.api.nvim_create_autocmd({ "BufWipeout", "BufDelete" }, {
  group = group,
  callback = function(event)
    diagnostics.clear(event.buf)
  end,
})

vim.api.nvim_create_user_command("YLangCheck", function()
  diagnostics.check(0)
end, { desc = "Check the current YLang buffer with the compiler" })

vim.api.nvim_create_user_command("YLangDiagnostics", function()
  vim.diagnostic.open_float(0, { scope = "line", border = "rounded", source = "always" })
end, { desc = "Show YLang diagnostics and compiler hints for the current line" })
