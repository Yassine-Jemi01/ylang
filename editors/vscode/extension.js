"use strict";

const vscode = require("vscode");
const childProcess = require("node:child_process");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");

let diagnostics;
let output;
let compilerMissingNoticeShown = false;
const documentStates = new Map();

function setting(name, fallback) {
  return vscode.workspace.getConfiguration("ylang").get(name, fallback);
}

function stateFor(uri) {
  const key = uri.toString();
  let state = documentStates.get(key);
  if (!state) {
    state = { revision: 0, timer: undefined, child: undefined };
    documentStates.set(key, state);
  }
  return state;
}

function cancelPending(document) {
  const state = stateFor(document.uri);
  state.revision += 1;
  if (state.timer) {
    clearTimeout(state.timer);
    state.timer = undefined;
  }
  if (state.child && !state.child.killed) {
    state.child.kill();
  }
  state.child = undefined;
  return state.revision;
}

function compilerExecutable(configured) {
  if (configured.startsWith("~/")) {
    return path.join(os.homedir(), configured.slice(2));
  }
  return configured;
}

const SHORT_MESSAGES = {
  E1001: "Invalid character",
  E1002: "Syntax error",
  E1003: "Expected a value",
  E1004: "Invalid assignment",
  E1005: "Put this code inside a function",
  E1010: "Invalid f-string",
  E1011: "Extra '}' in f-string",
  E1012: "Missing '}' in f-string",
  E1013: "Empty f-string placeholder",
  E1014: "Invalid f-string expression",
  E1015: "print() needs a value",
  E1016: "Unknown standard-library namespace",
  E2001: "Type mismatch",
  E2002: "Unknown type",
  E2003: "const needs a value",
  E2004: "Name already declared",
  E2005: "void cannot be used here",
  E2010: "Integer is too large",
  E2011: "Invalid float value",
  E2012: "char must be one byte",
  E2013: "Cannot use void in an f-string",
  E2014: "No calls or assignments inside f-strings",
  E2017: "const arrays are not supported",
  E2018: "Array needs an initializer",
  E2019: "Array literal needs an array declaration",
  E2020: "Unknown variable",
  E2021: "Variable may be uninitialized",
  E2022: "Cannot change a const variable",
  E2023: "Assignment cannot be used as a value",
  E2024: "Array literal cannot be empty",
  E2025: "Array elements must have the same type",
  E2026: "Only named fixed-size arrays can be indexed",
  E2027: "Indexing requires an array",
  E2028: "Array index must be an integer",
  E2029: "Array must be indexed before use",
  E2030: "Minus needs a number",
  E2031: "'not' needs true or false",
  E2032: "'and' and 'or' need true/false values",
  E2033: "'%' needs integers",
  E2034: "Use matching number types",
  E2035: "Compare matching types",
  E2036: "Compare values of the same type",
  E2037: "Whole-array assignment is not supported",
  E2038: "Array needs an array literal initializer",
  E2040: "Unknown function",
  E2041: "Wrong number of arguments",
  E2042: "Argument type mismatch",
  E2043: "Check function declaration or argument count",
  E2044: "This function requires string arguments",
  E2045: "Invalid conversion argument type",
  E2050: "Cannot print a void value",
  E2051: "if condition must be true or false",
  E2052: "'break' or 'continue' must be inside loop()",
  E2053: "Return value missing",
  E2054: "void function cannot return a value",
  E2055: "Return type mismatch",
  E2056: "Global value must be a constant",
  E2060: "Missing main() function",
  E2061: "Invalid main() function",
  E2062: "Some path is missing a return"
};

function shortMessage(code, original) {
  if (code && SHORT_MESSAGES[code]) {
    if (code === "E1002") {
      const expected = String(original || "").match(/Expected ['"](.+?)['"]/i);
      if (expected) return "Expected " + expected[1];
    }
    return SHORT_MESSAGES[code];
  }

  const message = String(original || "YLang error").replace(/\s+/g, " ").trim();
  if (message.length <= 58) return message.replace(/\.$/, "");
  return message.slice(0, 55).trimEnd() + "...";
}

function parseDiagnostics(text, document) {
  const lines = text.split(/\r?\n/);
  const blocks = [];
  let current;

  for (const line of lines) {
    const header = line.match(/^(error|warning)(?:\[([^\]]+)\])?:\s*(.*)$/);
    if (header) {
      if (current) blocks.push(current);
      current = {
        severity: header[1],
        code: header[2] || undefined,
        message: header[3],
        lines: [line]
      };
    } else if (current) {
      current.lines.push(line);
    }
  }
  if (current) blocks.push(current);

  const found = [];
  for (const block of blocks) {
    const locationLine = block.lines.find((line) => /^\s*-->\s+.+:\d+:\d+\s*$/.test(line));
    if (!locationLine) continue;

    const location = locationLine.match(/^\s*-->\s+(.+):(\d+):(\d+)\s*$/);
    if (!location) continue;

    const sourceLine = Number(location[2]);
    const sourceColumn = Number(location[3]);
    if (!Number.isInteger(sourceLine) || sourceLine < 1 ||
        !Number.isInteger(sourceColumn) || sourceColumn < 1) {
      continue;
    }

    const caretLine = block.lines.find((line) => /^\s*\|\s*\^+/.test(line));
    const caretMatch = caretLine && caretLine.match(/^\s*\|\s*(\^+)/);
    const width = caretMatch ? caretMatch[1].length : 1;
    const start = new vscode.Position(sourceLine - 1, sourceColumn - 1);
    const end = new vscode.Position(sourceLine - 1, sourceColumn - 1 + width);
    const severity = block.severity === "warning"
      ? vscode.DiagnosticSeverity.Warning
      : vscode.DiagnosticSeverity.Error;

    const message = shortMessage(block.code, block.message);
    const diagnostic = new vscode.Diagnostic(new vscode.Range(start, end), message, severity);
    diagnostic.source = "YLang";
    if (block.code) diagnostic.code = block.code;

    // Keep inline diagnostics short, but retain the compiler's complete explanation
    // and actionable help text in the hover/related-information UI.
    const relatedInformation = [];
    if (block.message && message !== block.message.replace(/\.$/, "")) {
      relatedInformation.push(new vscode.DiagnosticRelatedInformation(
        new vscode.Location(document.uri, start),
        block.message
      ));
    }
    const helpLine = block.lines.find((line) => /^\s*=\s*help:\s*/.test(line));
    if (helpLine) {
      const hint = helpLine.replace(/^\s*=\s*help:\s*/, "").trim();
      if (hint) {
        relatedInformation.push(new vscode.DiagnosticRelatedInformation(
          new vscode.Location(document.uri, start),
          "Hint: " + hint
        ));
      }
    }
    if (relatedInformation.length) diagnostic.relatedInformation = relatedInformation;
    found.push(diagnostic);
  }

  return found;
}

function temporarySource(document) {
  const tempDirectory = fs.mkdtempSync(path.join(os.tmpdir(), "ylang-vscode-"));
  const originalName = document.uri.scheme === "file"
    ? path.basename(document.uri.fsPath)
    : "untitled.yl";
  const filename = originalName.toLowerCase().endsWith(".yl")
    ? originalName
    : "untitled.yl";
  const tempFile = path.join(tempDirectory, filename);
  fs.writeFileSync(tempFile, document.getText(), "utf8");
  return { tempDirectory, tempFile };
}

function showCompilerMissing(compiler) {
  output.appendLine("Could not start YLang compiler: " + compiler);
  output.appendLine("Set ylang.compilerPath to the full path of your ylang executable.");
  if (compilerMissingNoticeShown) return;
  compilerMissingNoticeShown = true;
  vscode.window.showErrorMessage(
    "YLang compiler was not found. Set 'ylang.compilerPath' to its full path.",
    "Open Setting"
  ).then((choice) => {
    if (choice === "Open Setting") {
      vscode.commands.executeCommand(
        "workbench.action.openSettings",
        "ylang.compilerPath"
      );
    }
  });
}

function checkDocument(document, revision) {
  if (document.languageId !== "ylang") return;
  const state = stateFor(document.uri);
  if (state.revision !== revision) return;

  let temporary;
  try {
    temporary = temporarySource(document);
  } catch (error) {
    output.appendLine("Could not create temporary source: " + String(error));
    return;
  }

  const configuredCompiler = setting("compilerPath", "ylang");
  const compiler = compilerExecutable(configuredCompiler);
  const cwd = document.uri.scheme === "file"
    ? path.dirname(document.uri.fsPath)
    : (vscode.workspace.workspaceFolders?.[0]?.uri.fsPath || os.homedir());
  const timeout = setting("checkTimeoutMs", 10000);

  output.appendLine("Checking " + document.uri.fsPath + " with " + compiler);
  const child = childProcess.execFile(
    compiler,
    ["check", temporary.tempFile],
    { cwd, timeout, maxBuffer: 1024 * 1024, encoding: "utf8" },
    (error, stdout, stderr) => {
      try {
        fs.rmSync(temporary.tempDirectory, { recursive: true, force: true });
      } catch (cleanupError) {
        output.appendLine("Could not remove temporary files: " + String(cleanupError));
      }

      if (state.child === child) state.child = undefined;
      if (state.revision !== revision) return;

      if (error && error.code === "ENOENT") {
        diagnostics.delete(document.uri);
        showCompilerMissing(compiler);
        return;
      }

      compilerMissingNoticeShown = false;
      const combined = [stderr || "", stdout || ""].filter(Boolean).join("\n");
      const parsed = parseDiagnostics(combined, document);
      diagnostics.set(document.uri, parsed);

      if (error && error.killed) {
        output.appendLine("YLang check timed out after " + timeout + " ms.");
      } else if (error && !parsed.length && error.code !== 0) {
        // Log all unparsed failures, including exit code 1, rather than silently
        // clearing diagnostics if the compiler output format ever changes.
        output.appendLine("YLang check exited unexpectedly: " + error.message);
        if (combined.trim()) output.appendLine(combined.trim());
      } else if (parsed.length) {
        output.appendLine(parsed.length + " diagnostic(s) found.");
      } else {
        output.appendLine("No errors found.");
      }
    }
  );
  state.child = child;
}

function scheduleCheck(document, delayMs = 350) {
  if (document.languageId !== "ylang") return;
  const state = stateFor(document.uri);
  const revision = cancelPending(document);
  state.timer = setTimeout(() => {
    state.timer = undefined;
    checkDocument(document, revision);
  }, delayMs);
}

function checkCurrentFile() {
  const document = vscode.window.activeTextEditor?.document;
  if (!document || document.languageId !== "ylang") {
    vscode.window.showInformationMessage("Open a YLang (.yl) file first.");
    return;
  }
  scheduleCheck(document, 0);
  output.show(true);
}

function activate(context) {
  diagnostics = vscode.languages.createDiagnosticCollection("ylang");
  output = vscode.window.createOutputChannel("YLang");
  context.subscriptions.push(diagnostics, output);

  context.subscriptions.push(
    vscode.commands.registerCommand("ylang.checkCurrentFile", checkCurrentFile),
    vscode.commands.registerCommand("ylang.showOutput", () => output.show())
  );

  context.subscriptions.push(
    vscode.workspace.onDidOpenTextDocument((document) => {
      if (setting("checkOnOpen", true)) scheduleCheck(document, 100);
    }),
    vscode.workspace.onDidChangeTextDocument((event) => {
      if (setting("checkOnChange", true)) scheduleCheck(event.document, 450);
    }),
    vscode.workspace.onDidSaveTextDocument((document) => {
      if (setting("checkOnSave", true)) scheduleCheck(document, 0);
    }),
    vscode.workspace.onDidCloseTextDocument((document) => {
      const key = document.uri.toString();
      const state = documentStates.get(key);
      if (state) {
        if (state.timer) clearTimeout(state.timer);
        if (state.child && !state.child.killed) state.child.kill();
        documentStates.delete(key);
      }
      diagnostics.delete(document.uri);
    }),
    vscode.workspace.onDidChangeConfiguration((event) => {
      if (event.affectsConfiguration("ylang.compilerPath") ||
          event.affectsConfiguration("ylang.checkOnChange") ||
          event.affectsConfiguration("ylang.checkOnOpen") ||
          event.affectsConfiguration("ylang.checkOnSave") ||
          event.affectsConfiguration("ylang.checkTimeoutMs")) {
        for (const document of vscode.workspace.textDocuments) {
          if (document.languageId === "ylang") scheduleCheck(document, 0);
        }
      }
    })
  );

  for (const document of vscode.workspace.textDocuments) {
    if (document.languageId === "ylang" && setting("checkOnOpen", true)) {
      scheduleCheck(document, 100);
    }
  }
}

function deactivate() {
  for (const state of documentStates.values()) {
    if (state.timer) clearTimeout(state.timer);
    if (state.child && !state.child.killed) state.child.kill();
  }
  documentStates.clear();
}

module.exports = { activate, deactivate, _parseDiagnostics: parseDiagnostics };

