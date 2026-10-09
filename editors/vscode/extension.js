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

    const helpLine = block.lines.find((line) => /^\s*=\s*help:\s*/.test(line));
    const help = helpLine ? helpLine.replace(/^\s*=\s*help:\s*/, "").trim() : "";
    const message = help ? block.message + "\nHelp: " + help : block.message;
    const diagnostic = new vscode.Diagnostic(new vscode.Range(start, end), message, severity);
    diagnostic.source = "YLang";
    if (block.code) diagnostic.code = block.code;
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
      } else if (error && !parsed.length && error.code !== 1) {
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

module.exports = { activate, deactivate };
