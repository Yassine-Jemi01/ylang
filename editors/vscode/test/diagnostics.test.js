"use strict";

const assert = require("node:assert/strict");
const Module = require("node:module");

class Position {
  constructor(line, character) {
    this.line = line;
    this.character = character;
  }
}
class Range {
  constructor(start, end) {
    this.start = start;
    this.end = end;
  }
}
class Diagnostic {
  constructor(range, message, severity) {
    this.range = range;
    this.message = message;
    this.severity = severity;
  }
}
class Location {
  constructor(uri, range) {
    this.uri = uri;
    this.range = range;
  }
}
class DiagnosticRelatedInformation {
  constructor(location, message) {
    this.location = location;
    this.message = message;
  }
}

const vscodeMock = {
  Position,
  Range,
  Diagnostic,
  Location,
  DiagnosticRelatedInformation,
  DiagnosticSeverity: { Error: 0, Warning: 1 }
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

const document = {
  uri: {
    scheme: "file",
    fsPath: "/workspace/main.yl",
    toString() { return this.fsPath; }
  }
};

const diagnostics = extension._parseDiagnostics([
  "error[E1002]: Expected ';' after variable declaration.",
  "  --> /tmp/ylang-check/main.yl:12:30",
  "   |",
  " 12 |     let string file_path = io.read_line();",
  "   |                              ^",
  "   = help: End the declaration with a semicolon.",
  "",
  "error[E2025]: All array elements must have the same type.",
  "  --> /tmp/ylang-check/main.yl:18:25",
  "   |",
  " 18 |     let int values[] = [1, true];",
  "   |                         ^",
  "   = help: Use one element type throughout the array literal.",
  "ylang: check failed with 2 error(s)."
].join("\n"), document);

assert.equal(diagnostics.length, 2, "both compiler errors should be parsed");
assert.equal(diagnostics[0].range.start.line, 11, "source lines should become zero-based");
assert.equal(diagnostics[0].range.start.character, 29, "source columns should become zero-based");
assert.equal(diagnostics[0].source, "YLang");
assert.equal(diagnostics[0].code, "E1002");
assert.ok(
  diagnostics[0].relatedInformation.some((item) => item.message.includes("Hint: End the declaration")),
  "compiler help text should be available as related information"
);
assert.ok(
  diagnostics[0].relatedInformation.some((item) => item.message.includes("Expected ';' after variable declaration")),
  "full compiler explanation should be retained"
);
assert.equal(diagnostics[1].code, "E2025");
assert.equal(diagnostics[1].message, "Array elements must have the same type");

const unknown = extension._parseDiagnostics([
  "error[E9999]: A new compiler diagnostic.",
  "  --> /tmp/ylang-check/main.yl:2:3",
  "   |",
  "  2 | x",
  "   |   ^"
].join("\n"), document);
assert.equal(unknown.length, 1);
assert.equal(unknown[0].message, "A new compiler diagnostic");

console.log("VS Code diagnostic parser tests passed");
