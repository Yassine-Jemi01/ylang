"use strict";

const assert = require("node:assert/strict");
const Module = require("node:module");

class CompletionItem {
  constructor(label, kind) { this.label = label; this.kind = kind; }
}
class MarkdownString {
  constructor(value) { this.value = value; }
}
class SnippetString {
  constructor(value) { this.value = value; }
}
class SignatureHelp {}
class SignatureInformation {
  constructor(label, documentation) {
    this.label = label;
    this.documentation = documentation;
    this.parameters = [];
  }
}
class ParameterInformation {
  constructor(label, documentation) {
    this.label = label;
    this.documentation = documentation;
  }
}

const vscodeMock = {
  CompletionItem,
  CompletionItemKind: { Function: 3 },
  MarkdownString,
  SnippetString,
  SignatureHelp,
  SignatureInformation,
  ParameterInformation
};

const originalLoad = Module._load;
Module._load = function (request, parent, isMain) {
  if (request === "vscode") return vscodeMock;
  return originalLoad.call(this, request, parent, isMain);
};

let extension;
try {
  extension = require("../extension");
} finally {
  Module._load = originalLoad;
}

function documentWithLine(text) {
  return { lineAt() { return { text }; } };
}

const completions = extension._provideIoCompletions(
  documentWithLine("    io."),
  { line: 0, character: 8 }
);
assert.equal(completions.length, 3, "io. should suggest all three safe I/O APIs");
assert.deepEqual(
  completions.map((item) => item.label),
  [
    "io.try_read_file(path)",
    "io.write_file_atomic(path, content)",
    "io.last_error()"
  ]
);
assert.ok(completions[0].documentation.value.includes("owned_string"));
assert.equal(completions[0].insertText.value, "io.try_read_file(\${1:path})");

const filtered = extension._provideIoCompletions(
  documentWithLine("io.write"),
  { line: 0, character: 8 }
);
assert.deepEqual(filtered.map((item) => item.label), [
  "io.write_file_atomic(path, content)"
]);

const nestedCompletions = extension._provideIoCompletions(
  documentWithLine("print(io."),
  { line: 0, character: 9 }
);
assert.equal(
  nestedCompletions.length,
  3,
  "io completions should also work inside nested expressions"
);

const writeHelp = extension._provideIoSignatureHelp(
  documentWithLine('io.write_file_atomic("tasks.json", '),
  { line: 0, character: 34 }
);
assert.ok(writeHelp instanceof SignatureHelp);
assert.equal(writeHelp.signatures[0].label, "io.write_file_atomic(path, content) → bool");
assert.equal(writeHelp.signatures[0].parameters.length, 2);
assert.equal(writeHelp.activeParameter, 1);

const readHelp = extension._provideIoSignatureHelp(
  documentWithLine("io.try_read_file("),
  { line: 0, character: 18 }
);
assert.equal(readHelp.signatures[0].parameters[0].label, "path");

const errorHelp = extension._provideIoSignatureHelp(
  documentWithLine("io.last_error("),
  { line: 0, character: 14 }
);
assert.equal(errorHelp.signatures[0].parameters.length, 0);

console.log("VS Code I/O autocomplete and signature help tests passed");
