local M = {}

local namespace = vim.api.nvim_create_namespace("ylang")
local jobs = {}
local generations = {}
local notified_missing = false

local SHORT_MESSAGES = {
  E1001 = "Invalid character",
  E1002 = "Syntax error",
  E1003 = "Expected a value",
  E1004 = "Invalid assignment",
  E1005 = "Put this code inside a function",
  E1010 = "Invalid f-string",
  E1011 = "Extra '}' in f-string",
  E1012 = "Missing '}' in f-string",
  E1013 = "Empty f-string placeholder",
  E1014 = "Invalid f-string expression",
  E1015 = "print() needs a value",
  E1016 = "Unknown standard-library namespace",
  E2001 = "Type mismatch",
  E2002 = "Unknown type",
  E2003 = "const needs a value",
  E2004 = "Name already declared",
  E2005 = "void cannot be used here",
  E2010 = "Integer is too large",
  E2011 = "Invalid float value",
  E2012 = "char must be one byte",
  E2013 = "Cannot use void in an f-string",
  E2014 = "No calls or assignments inside f-strings",
  E2017 = "const arrays are not supported",
  E2018 = "Array needs an initializer",
  E2019 = "Array literal needs an array declaration",
  E2020 = "Unknown variable",
  E2021 = "Variable may be uninitialized",
  E2022 = "Cannot change a const variable",
  E2023 = "Assignment cannot be used as a value",
  E2024 = "Array literal cannot be empty",
  E2025 = "Array elements must have the same type",
  E2026 = "Only named fixed-size arrays can be indexed",
  E2027 = "Indexing requires an array",
  E2028 = "Array index must be an integer",
  E2029 = "Array must be indexed before use",
  E2030 = "Minus needs a number",
  E2031 = "'not' needs true or false",
  E2032 = "'and' and 'or' need true/false values",
  E2033 = "'%' needs integers",
  E2034 = "Use matching number types",
  E2035 = "Compare matching types",
  E2036 = "Compare values of the same type",
  E2037 = "Whole-array assignment is not supported",
  E2038 = "Array needs an array literal initializer",
  E2040 = "Unknown function",
  E2041 = "Wrong number of arguments",
  E2042 = "Argument type mismatch",
  E2043 = "Invalid function declaration or conversion",
  E2044 = "This function requires string arguments",
  E2045 = "Invalid conversion argument type",
  E2050 = "Cannot print a void value",
  E2051 = "if condition must be true or false",
  E2052 = "'break' or 'continue' must be inside a loop",
  E2053 = "Return value missing",
  E2054 = "void function cannot return a value",
  E2055 = "Return type mismatch",
  E2056 = "Global value must be a constant",
  E2060 = "Missing main() function",
  E2061 = "Invalid main() function",
  E2062 = "Some path is missing a return",
}

local function parse_header(line)
  local severity, code, message = line:match("^(error)%[([^%]]+)%]:%s*(.*)$")
  if severity then
    return severity, code, message
  end

  severity, code, message = line:match("^(warning)%[([^%]]+)%]:%s*(.*)$")
  if severity then
    return severity, code, message
  end

  severity, message = line:match("^(error):%s*(.*)$")
  if severity then
    return severity, nil, message
  end

  severity, message = line:match("^(warning):%s*(.*)$")
  if severity then
    return severity, nil, message
  end

  return nil
end

local function short_message(code, original)
  if code and SHORT_MESSAGES[code] then
    return SHORT_MESSAGES[code]
  end

  local message = (original or "YLang error"):gsub("%s+", " "):gsub("^%s+", ""):gsub("%s+$", "")
  if #message <= 58 then
    return message:gsub("%.$", "")
  end
  return message:sub(1, 55):gsub("%s+$", "") .. "..."
end

local function parse_diagnostics(lines)
  local result = {}
  local i = 1

  while i <= #lines do
    local severity_name, code, original = parse_header(lines[i] or "")
    if not severity_name then
      i = i + 1
    else
      local start_index = i
      local j = i + 1
      while j <= #lines and not parse_header(lines[j] or "") do
        j = j + 1
      end

      local source_line, source_col, hint
      local width = 1
      for k = start_index + 1, j - 1 do
        local line = lines[k] or ""
        local _, line_number, column_number =
          line:match("^%s*%-%->%s+(.+):(%d+):(%d+)%s*$")
        if line_number and column_number and not source_line then
          source_line = tonumber(line_number)
          source_col = tonumber(column_number)
        end

        local help = line:match("^%s*=%s*help:%s*(.*)$")
        if help and help ~= "" then
          hint = help
        end

        local pipe = line:find("|", 1, true)
        if pipe then
          local caret = line:find("^", pipe + 1, true)
          if caret then
            local carets = line:sub(caret):match("^(%^+)")
            if carets and #carets > 0 then
              width = #carets
            end
          end
        end
      end

      if source_line and source_col and source_line >= 1 and source_col >= 1 then
        local message = short_message(code, original)
        if hint then
          message = message .. "\nHint: " .. hint
        end
        result[#result + 1] = {
          lnum = source_line - 1,
          col = source_col - 1,
          end_lnum = source_line - 1,
          end_col = source_col - 1 + width,
          severity = severity_name == "warning"
              and vim.diagnostic.severity.WARN
              or vim.diagnostic.severity.ERROR,
          source = "YLang",
          code = code,
          message = message,
          user_data = {
            original_message = original,
            hint = hint,
          },
        }
      end
      i = j
    end
  end

  return result
end

local function compiler_executable()
  local configured = vim.g.ylang_compiler_path
  if type(configured) == "string" and configured ~= "" then
    return vim.fn.expand(configured)
  end
  return "ylang"
end

local function notify_missing(executable)
  if notified_missing then
    return
  end
  notified_missing = true
  vim.notify(
    "YLang compiler could not be started: " .. executable
      .. "\nSet g:ylang_compiler_path to the full path of build/ylang.",
    vim.log.levels.ERROR,
    { title = "YLang diagnostics" }
  )
end

local function stop_job(bufnr)
  local job = jobs[bufnr]
  if job then
    jobs[bufnr] = nil
    pcall(vim.fn.jobstop, job)
  end
end

function M.check(bufnr)
  bufnr = bufnr or 0
  if bufnr == 0 then
    bufnr = vim.api.nvim_get_current_buf()
  end
  if not vim.api.nvim_buf_is_valid(bufnr) or vim.bo[bufnr].filetype ~= "ylang" then
    return
  end

  generations[bufnr] = (generations[bufnr] or 0) + 1
  local generation = generations[bufnr]
  stop_job(bufnr)

  local temp_path = vim.fn.tempname() .. ".yl"
  local source_lines = vim.api.nvim_buf_get_lines(bufnr, 0, -1, false)
  if vim.fn.writefile(source_lines, temp_path) ~= 0 then
    vim.notify("Could not create a temporary YLang source file.", vim.log.levels.ERROR)
    return
  end

  local compiler = compiler_executable()
  local original_path = vim.api.nvim_buf_get_name(bufnr)
  local cwd = original_path ~= ""
      and vim.fn.fnamemodify(original_path, ":p:h")
      or vim.fn.getcwd()
  local output = { stdout = {}, stderr = {} }
  local job_id

  job_id = vim.fn.jobstart({ compiler, "check", temp_path }, {
    cwd = cwd,
    stdout_buffered = true,
    stderr_buffered = true,
    on_stdout = function(_, data)
      output.stdout = data or {}
    end,
    on_stderr = function(_, data)
      output.stderr = data or {}
    end,
    on_exit = function(_, exit_code)
      vim.schedule(function()
        vim.fn.delete(temp_path)
        if jobs[bufnr] == job_id then
          jobs[bufnr] = nil
        end
        if not vim.api.nvim_buf_is_valid(bufnr) or generations[bufnr] ~= generation then
          return
        end

        local combined = {}
        vim.list_extend(combined, output.stderr)
        vim.list_extend(combined, output.stdout)
        local parsed = parse_diagnostics(combined)
        vim.diagnostic.set(namespace, bufnr, parsed, {})
        notified_missing = false

        if exit_code ~= 0 and #parsed == 0 then
          local details = table.concat(combined, "\n"):gsub("^%s+", ""):gsub("%s+$", "")
          if details == "" then
            details = "Compiler exited with status " .. tostring(exit_code) .. "."
          end
          vim.notify(details, vim.log.levels.WARN, { title = "YLang diagnostics" })
        end
      end)
    end,
  })

  if job_id <= 0 then
    vim.fn.delete(temp_path)
    notify_missing(compiler)
    return
  end

  jobs[bufnr] = job_id
  local timeout = tonumber(vim.g.ylang_check_timeout_ms) or 10000
  vim.defer_fn(function()
    if jobs[bufnr] == job_id then
      vim.fn.jobstop(job_id)
      vim.notify(
        "YLang check timed out after " .. tostring(timeout) .. " ms.",
        vim.log.levels.WARN,
        { title = "YLang diagnostics" }
      )
    end
  end, timeout)
end

function M.schedule(bufnr, delay_ms)
  bufnr = bufnr or vim.api.nvim_get_current_buf()
  if not vim.api.nvim_buf_is_valid(bufnr) or vim.bo[bufnr].filetype ~= "ylang" then
    return
  end

  generations[bufnr] = (generations[bufnr] or 0) + 1
  local generation = generations[bufnr]
  stop_job(bufnr)

  vim.defer_fn(function()
    if vim.api.nvim_buf_is_valid(bufnr)
        and generations[bufnr] == generation
        and vim.bo[bufnr].filetype == "ylang" then
      M.check(bufnr)
    end
  end, delay_ms or 350)
end

function M.clear(bufnr)
  bufnr = bufnr or vim.api.nvim_get_current_buf()
  generations[bufnr] = (generations[bufnr] or 0) + 1
  stop_job(bufnr)
  if vim.api.nvim_buf_is_valid(bufnr) then
    vim.diagnostic.reset(namespace, bufnr)
  end
end

function M.parse_diagnostics(lines)
  return parse_diagnostics(lines)
end

function M.setup()
  vim.diagnostic.config({
    underline = true,
    signs = true,
    virtual_text = {
      prefix = "●",
      format = function(diagnostic)
        return diagnostic.message:match("^[^\n]*") or diagnostic.message
      end,
    },
    severity_sort = true,
    update_in_insert = false,
    float = { border = "rounded", source = "always" },
  }, namespace)
end

return M
