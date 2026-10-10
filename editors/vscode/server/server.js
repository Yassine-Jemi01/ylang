"use strict";
const cp = require("node:child_process");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const { fileURLToPath } = require("node:url");
const {
  createConnection, ProposedFeatures, TextDocuments, Diagnostic,
  DiagnosticSeverity, TextDocumentSyncKind, CompletionItemKind,
  InsertTextFormat, MarkupKind, MessageType
} = require("vscode-languageserver/node");
const { TextDocument } = require("vscode-languageserver-textdocument");

const connection = createConnection(ProposedFeatures.all);
const documents = new TextDocuments(TextDocument);
const active = new Map();
const timers = new Map();
const settingsCache = new Map();
let workspaceConfigSupported = false;
let compilerWarningShown = false;
let defaults = { compilerPath: "ylang", checkOnOpen: true, checkOnChange: true, checkOnSave: true, checkTimeoutMs: 10000 };

const SHORT = {
  E1001:"Invalid character", E1002:"Syntax error", E1003:"Expected a value",
  E1004:"Invalid assignment", E1005:"Code must be inside a function",
  E1010:"Invalid f-string", E1011:"Extra '}' in f-string", E1012:"Missing '}' in f-string",
  E1013:"Empty f-string placeholder", E1014:"Invalid f-string expression", E1015:"print() needs a value",
  E2001:"Type mismatch", E2002:"Unknown type", E2003:"const needs a value",
  E2004:"Name already declared", E2005:"void cannot be used here",
  E2010:"Integer is too large", E2011:"Invalid float value", E2012:"char must be one byte",
  E2013:"Cannot use void in an f-string", E2014:"No calls or assignments inside f-strings",
  E2020:"Unknown variable", E2021:"Variable may be uninitialized", E2022:"Cannot change a const variable",
  E2023:"Assignment cannot be used as a value", E2030:"Minus needs a number",
  E2031:"'not' needs true or false", E2032:"'and' and 'or' need boolean values",
  E2033:"'%' needs integers", E2034:"Use matching number types", E2035:"Compare matching types",
  E2036:"Compare values of the same type", E2040:"Unknown function", E2041:"Wrong number of arguments",
  E2042:"Argument type mismatch", E2043:"Function already exists", E2050:"Cannot print a void value",
  E2051:"if condition must be boolean", E2052:"'break' or 'continue' must be inside loop()",
  E2053:"Return value missing", E2054:"void function cannot return a value",
  E2055:"Return type mismatch", E2056:"Global value must be constant",
  E2060:"Missing main() function", E2061:"Invalid main() function", E2062:"Some path is missing a return",
  E2063:"Borrow only in a matching parameter", E2064:"Borrow type not supported", E2065:"Cannot borrow mutable global",
  E2066:"Cannot mutably borrow const", E2067:"Read conflicts with mutable borrow", E2068:"Write conflicts with borrow",
  E2070:"Use after move", E2071:"Cannot move a global owned value", E2072:"Cannot move a value into itself",
  E2073:"Array literal needs an element", E2074:"Invalid array element type", E2075:"Array elements must match",
  E2076:"Operation requires an array", E2077:"Array index must be int", E2078:"Array must be a named variable",
  E2079:"Cannot modify const array", E2080:"For-each variable type mismatch", E2081:"String array iteration unsupported", E2082:"For-each requires a named array"
};

function shortMessage(code, original) {
  if (code && Object.prototype.hasOwnProperty.call(SHORT, code)) {
    if (code === "E1002") {
      const text = String(original || "");
      if (/semicolon/i.test(text)) return "Expected ';'";
      if (/Expected '\)'/i.test(text)) return "Expected ')'";
      if (/Expected '\('/i.test(text)) return "Expected '('";
      if (/Expected '\}'/i.test(text)) return "Expected '}'";
      if (/Expected '\{'/i.test(text)) return "Expected '{'";
      if (/Expected '->'/i.test(text)) return "Expected '->'";
    }
    return SHORT[code];
  }
  const text = String(original || "YLang error").replace(/\s+/g, " ").trim();
  return text.length > 58 ? text.slice(0, 55).trimEnd() + "..." : text.replace(/\.$/, "");
}

function normalizeSettings(value) {
  const x = value || {};
  return {
    compilerPath: typeof x.compilerPath === "string" && x.compilerPath.trim() ? x.compilerPath.trim() : "ylang",
    checkOnOpen: x.checkOnOpen !== false,
    checkOnChange: x.checkOnChange !== false,
    checkOnSave: x.checkOnSave !== false,
    checkTimeoutMs: Number.isFinite(x.checkTimeoutMs) ? Math.max(500, Math.min(120000, x.checkTimeoutMs)) : 10000
  };
}

async function getSettings(document) {
  if (settingsCache.has(document.uri)) return settingsCache.get(document.uri);
  let result = defaults;
  if (workspaceConfigSupported) {
    try {
      result = normalizeSettings(await connection.workspace.getConfiguration({ scopeUri: document.uri, section: "ylang" }));
    } catch (error) {
      connection.console.warn("Could not read YLang settings: " + String(error));
    }
  }
  settingsCache.set(document.uri, result);
  return result;
}

function expandPath(value) {
  if (value === "~") return os.homedir();
  if (value.startsWith("~/") || value.startsWith("~\\")) {
    return path.join(os.homedir(), value.slice(2));
  }
  return value;
}

function removeTemp(entry) {
  if (!entry || !entry.tempDirectory) return;
  try { fs.rmSync(entry.tempDirectory, { recursive: true, force: true }); }
  catch (error) { connection.console.warn("Could not clean temporary source: " + String(error)); }
}

function cancelCheck(uri) {
  const entry = active.get(uri);
  if (!entry) return;
  active.delete(uri);
  if (entry.child && !entry.child.killed) entry.child.kill();
  removeTemp(entry);
}

function makeTemporarySource(document) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "ylang-lsp-"));
  const sourcePath = document.uri.startsWith("file:") ? fileURLToPath(document.uri) : "";
  let name = sourcePath ? path.basename(sourcePath) : "untitled.yl";
  if (!name.toLowerCase().endsWith(".yl")) name = "untitled.yl";
  const filename = path.join(dir, name);
  fs.writeFileSync(filename, document.getText(), "utf8");
  return { tempDirectory: dir, filename };
}

function parseOutput(text, document) {
  const lines = text.split(/\r?\n/);
  const blocks = [];
  let current = null;
  for (const line of lines) {
    const header = line.match(/^(error|warning)(?:\[([^\]]+)\])?:\s*(.*)$/);
    if (header) {
      if (current) blocks.push(current);
      current = { severity: header[1], code: header[2] || undefined, message: header[3], lines: [line] };
    } else if (current) current.lines.push(line);
  }
  if (current) blocks.push(current);
  const sourceLines = document.getText().split(/\r?\n/);
  const result = [];
  for (const item of blocks) {
    const locationText = item.lines.find((line) => /^\s*-->\s+.+:\d+:\d+\s*$/.test(line));
    const match = locationText && locationText.match(/^\s*-->\s+(.+):(\d+):(\d+)\s*$/);
    if (!match) continue;
    const line = Number(match[2]);
    const column = Number(match[3]);
    if (!Number.isInteger(line) || line < 1 || line > sourceLines.length || !Number.isInteger(column) || column < 1) continue;
    const content = sourceLines[line - 1] || "";
    const caretLine = item.lines.find((entry) => /^\s*\|\s*\^+/.test(entry));
    const caret = caretLine && caretLine.match(/^\s*\|\s*(\^+)/);
    const start = Math.min(column - 1, content.length);
    const end = Math.min(start + Math.max(1, caret ? caret[1].length : 1), content.length);
    result.push(Diagnostic.create(
      { start: { line: line - 1, character: start }, end: { line: line - 1, character: end } },
      shortMessage(item.code, item.message),
      item.severity === "warning" ? DiagnosticSeverity.Warning : DiagnosticSeverity.Error,
      item.code, "YLang"
    ));
  }
  return result;
}

async function validate(document) {
  if (!document || document.languageId !== "ylang") return;
  const uri = document.uri;
  const version = document.version;
  cancelCheck(uri);
  let temp;
  try { temp = makeTemporarySource(document); }
  catch (error) { connection.console.error("Cannot prepare YLang source: " + String(error)); return; }

  const options = await getSettings(document);
  const latest = documents.get(uri);
  if (!latest || latest.version !== version) {
    removeTemp(temp);
    return;
  }
  const executable = expandPath(options.compilerPath);
  const cwd = uri.startsWith("file:") ? path.dirname(fileURLToPath(uri)) : os.homedir();
  const entry = { child: null, tempDirectory: temp.tempDirectory, version };
  active.set(uri, entry);
  entry.child = cp.execFile(executable, ["check", temp.filename], {
    cwd, timeout: options.checkTimeoutMs, maxBuffer: 1024 * 1024, encoding: "utf8"
  }, (error, stdout, stderr) => {
    removeTemp(entry);
    if (active.get(uri) !== entry) return;
    active.delete(uri);
    const current = documents.get(uri);
    if (!current || current.version !== version) return;

    if (error && error.code === "ENOENT") {
      connection.sendDiagnostics({ uri, diagnostics: [] });
      if (!compilerWarningShown) {
        compilerWarningShown = true;
        connection.window.showMessage({
          type: MessageType.Error,
          message: "YLang compiler not found. Set ylang.compilerPath in VS Code settings."
        });
      }
      connection.console.error("Cannot start YLang compiler: " + executable);
      return;
    }
    compilerWarningShown = false;
    const combined = [stderr || "", stdout || ""].filter(Boolean).join("\n");
    const diagnostics = parseOutput(combined, current);
    connection.sendDiagnostics({ uri, version, diagnostics });
    if (error && error.killed) connection.console.warn("YLang check timed out: " + uri);
    else if (error && error.code !== 1 && diagnostics.length === 0) {
      connection.console.error("YLang check failed: " + error.message);
      if (combined.trim()) connection.console.error(combined.trim());
    }
  });
}

function schedule(document, delay) {
  if (!document || document.languageId !== "ylang") return;
  const uri = document.uri;
  if (timers.has(uri)) clearTimeout(timers.get(uri));
  timers.delete(uri);
  cancelCheck(uri);
  timers.set(uri, setTimeout(() => {
    timers.delete(uri);
    void validate(document);
  }, delay));
}

const completions = [
  { label:"function", kind:CompletionItemKind.Keyword, detail:"Declare a function", insertText:"function " + "$" + "{1:name}(" + "$" + "{2}) -> " + "$" + "{3:int} {\n    " + "$" + "0\n}" },
  { label:"main", kind:CompletionItemKind.Snippet, detail:"Main function", insertText:"function main() -> int {\n    " + "$" + "0\n    return 0;\n}" },
  { label:"let", kind:CompletionItemKind.Keyword, detail:"Declare a variable", insertText:"let " + "$" + "{1:int} " + "$" + "{2:name} = " + "$" + "{3:value};" },
  { label:"const", kind:CompletionItemKind.Keyword, detail:"Immutable variable modifier" },
  { label:"if", kind:CompletionItemKind.Keyword, detail:"Conditional", insertText:"if (" + "$" + "{1:condition}) {\n    " + "$" + "0\n}" },
  { label:"else", kind:CompletionItemKind.Keyword, detail:"Alternative branch" },
  { label:"loop", kind:CompletionItemKind.Keyword, detail:"Loop", insertText:"loop() {\n    " + "$" + "0\n}" },
  { label:"for", kind:CompletionItemKind.Keyword, detail:"Iterate over an array", insertText:"for (" + "$" + "{1:int} " + "$" + "{2:item} in " + "$" + "{3:values}) {\n    " + "$" + "0\n}" },
  { label:"in", kind:CompletionItemKind.Keyword, detail:"Array iteration separator" },
  { label:"break", kind:CompletionItemKind.Keyword, detail:"Exit the loop" },
  { label:"continue", kind:CompletionItemKind.Keyword, detail:"Continue loop" },
  { label:"return", kind:CompletionItemKind.Keyword, detail:"Return from function" },
  { label:"int", kind:CompletionItemKind.TypeParameter, detail:"Signed 64-bit integer" },
  { label:"float", kind:CompletionItemKind.TypeParameter, detail:"64-bit float" },
  { label:"bool", kind:CompletionItemKind.TypeParameter, detail:"Boolean" },
  { label:"char", kind:CompletionItemKind.TypeParameter, detail:"One-byte character" },
  { label:"string", kind:CompletionItemKind.TypeParameter, detail:"String" },
  { label:"void", kind:CompletionItemKind.TypeParameter, detail:"No return value" },
  { label:"true", kind:CompletionItemKind.Constant, detail:"Boolean true" },
  { label:"false", kind:CompletionItemKind.Constant, detail:"Boolean false" },
  { label:"and", kind:CompletionItemKind.Operator, detail:"Boolean AND" },
  { label:"or", kind:CompletionItemKind.Operator, detail:"Boolean OR" },
  { label:"not", kind:CompletionItemKind.Operator, detail:"Boolean NOT" },
  { label:"print", kind:CompletionItemKind.Function, detail:"Print values", insertText:"print(" + "$" + "{1:value});" },
  { label:"read_line", kind:CompletionItemKind.Function, detail:"Read one line from standard input", insertText:"read_line()" },
  { label:"len", kind:CompletionItemKind.Function, detail:"String byte length or array element count", insertText:"len(" + "$" + "{1:value})" },
  { label:"clone", kind:CompletionItemKind.Function, detail:"Copy an owned string or array", insertText:"clone(" + "$" + "{1:value})" },
  { label:"append", kind:CompletionItemKind.Function, detail:"Append a value to a mutable array", insertText:"append(" + "$" + "{1:values}, " + "$" + "{2:value});" }
];

const hovers = {
  int:"Signed 64-bit integer.", float:"64-bit floating-point value.", bool:"Boolean: true or false.",
  char:"One-byte character.", string:"Immutable string.", void:"Function return type with no value.",
  function:"Declares a function.", let:"Declares a variable.", const:"Makes a variable immutable.",
  if:"Conditional statement.", else:"Alternative branch.", loop:"Repeats until break.", for:"Iterates over each scalar element of an array.", in:"Separates the loop variable from the array.",
  break:"Exits the current loop.", continue:"Skips to the next loop iteration.",
  return:"Returns from the current function.", print:"Built-in statement: print(value);", read_line:"Reads one line from standard input; EOF is a runtime error in this initial API.", len:"Returns UTF-8 byte length for strings or element count for arrays.", clone:"Creates an independent copy of an owned string or array.", append:"Appends one type-matching value to a named mutable array.",
  true:"Boolean true.", false:"Boolean false.", and:"Boolean AND.", or:"Boolean OR.", not:"Boolean negation."
};

function wordAt(document, position) {
  const text = document.getText();
  const offset = document.offsetAt(position);
  let start = offset, end = offset;
  const part = (c) => /[A-Za-z0-9_]/.test(c || "");
  while (start > 0 && part(text[start - 1])) start--;
  while (end < text.length && part(text[end])) end++;
  return start === end ? null : { word: text.slice(start, end) };
}

function escapeRegex(value) {
  const special = "\\.^$*+?()[]{}|";
  return value.split("").map((ch) => special.includes(ch) ? "\\" + ch : ch).join("");
}

connection.onInitialize((params) => {
  workspaceConfigSupported = !!(params.capabilities && params.capabilities.workspace && params.capabilities.workspace.configuration);
  return {
    capabilities: {
      textDocumentSync: TextDocumentSyncKind.Incremental,
      completionProvider: { triggerCharacters: ["."] },
      hoverProvider: true,
      definitionProvider: true
    },
    serverInfo: { name: "ylang-language-server", version: "0.1.0" }
  };
});
connection.onInitialized(() => { for (const doc of documents.all()) void validate(doc); });
connection.onDidChangeConfiguration((change) => {
  settingsCache.clear();
  if (!workspaceConfigSupported && change.settings) defaults = normalizeSettings(change.settings.ylang || change.settings);
  for (const doc of documents.all()) schedule(doc, 0);
});
documents.onDidOpen(({ document }) => { void validate(document); });
documents.onDidChangeContent(({ document }) => schedule(document, 300));
documents.onDidSave(({ document }) => schedule(document, 0));
documents.onDidClose(({ document }) => {
  const uri = document.uri;
  if (timers.has(uri)) clearTimeout(timers.get(uri));
  timers.delete(uri);
  cancelCheck(uri);
  settingsCache.delete(uri);
  connection.sendDiagnostics({ uri, diagnostics: [] });
});
connection.onNotification("ylang/checkDocument", (params) => {
  const doc = params && params.uri ? documents.get(params.uri) : null;
  if (doc) schedule(doc, 0);
});
connection.onCompletion((params) => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return [];
  const before = doc.getText().slice(0, doc.offsetAt(params.position));
  const match = before.match(/[A-Za-z_][A-Za-z0-9_]*$/);
  const prefix = match ? match[0].toLowerCase() : "";
  return completions.filter((item) => !prefix || item.label.toLowerCase().startsWith(prefix)).map((item) => ({
    label:item.label, kind:item.kind, detail:item.detail, insertText:item.insertText || item.label,
    insertTextFormat:item.insertText ? InsertTextFormat.Snippet : InsertTextFormat.PlainText
  }));
});
connection.onHover((params) => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return null;
  const target = wordAt(doc, params.position);
  if (!target) return null;
  if (hovers[target.word]) return { contents:{kind:MarkupKind.Markdown, value:"**" + target.word + "** — " + hovers[target.word]} };
  const escaped = escapeRegex(target.word);
  const text = doc.getText();
  const fn = new RegExp("\\bfunction\\s+" + escaped + "\\s*\\(([^)]*)\\)\\s*->\\s*((?:int|float|bool|char|string|void)(?:\\[\\])?)").exec(text);
  if (fn) return { contents:{kind:MarkupKind.Markdown, value:"**function " + target.word + "(" + fn[1].trim() + ") -> " + fn[2] + "**"} };
  const variable = new RegExp("\\blet\\s+(?:const\\s+)?((?:int|float|bool|char|string|void)(?:\\[\\])?)\\s+" + escaped + "\\b").exec(text);
  if (variable) return { contents:{kind:MarkupKind.Markdown, value:"**" + target.word + ": " + variable[1] + "**"} };
  return null;
});
connection.onDefinition((params) => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return null;
  const target = wordAt(doc, params.position);
  if (!target) return null;
  const escaped = escapeRegex(target.word);
  const text = doc.getText();
  const patterns = [
    new RegExp("\\bfunction\\s+(" + escaped + ")\\s*\\("),
    new RegExp("\\blet\\s+(?:const\\s+)?(?:int|float|bool|char|string)(?:\\[\\])?\\s+(" + escaped + ")\\b")
  ];
  for (const pattern of patterns) {
    const match = pattern.exec(text);
    if (!match) continue;
    const offset = match.index + match[0].indexOf(target.word);
    return { uri:doc.uri, range:{start:doc.positionAt(offset), end:doc.positionAt(offset + target.word.length)} };
  }
  return null;
});

documents.listen(connection);
connection.listen();
