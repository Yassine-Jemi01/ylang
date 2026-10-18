"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

const grammarPath = path.join(__dirname, "..", "syntaxes", "ylang.tmLanguage.json");
const grammar = JSON.parse(fs.readFileSync(grammarPath, "utf8"));
const repository = grammar.repository;

assert.equal(grammar.scopeName, "source.ylang", "VS Code must use the registered language scope");

const includes = new Set();
function visit(value) {
  if (!value || typeof value !== "object") return;
  if (typeof value.include === "string" && value.include.startsWith("#")) {
    includes.add(value.include.slice(1));
  }
  for (const child of Object.values(value)) visit(child);
}
visit(grammar);
for (const name of includes) {
  assert.ok(repository[name], `grammar include #${name} must be defined`);
}

for (const keyword of ["struct", "enum", "match", "from", "import", "for", "while"]) {
  assert.ok(repository.keywords.match.includes(keyword), `missing YLang 2.0 keyword: ${keyword}`);
}
assert.ok(repository["struct-declaration"], "struct declarations should have a dedicated scope");
assert.ok(repository["enum-declaration"], "enum declarations and variants should have dedicated scopes");
assert.ok(repository["field-declaration"], "struct fields should be highlighted");
assert.ok(repository["variable-declaration"], "typed variable declarations should be highlighted");
assert.ok(repository.namespace.match.includes("gui"), "built-in namespaces should be highlighted");
assert.match(repository.numbers.match, /eE/, "scientific-notation floats should be highlighted");
assert.match(repository.operators.match, /\\\\\+\\\\\+/, "postfix increment should be highlighted");
assert.match(repository.operators.match, /\\\\\+=/, "compound addition assignment should be highlighted");
assert.match(repository.operators.match, /->/, "match-arm/function-return arrows should be highlighted");
assert.ok(grammar.patterns.some((pattern) => pattern.include === "#struct-declaration"));
assert.ok(grammar.patterns.some((pattern) => pattern.include === "#enum-declaration"));

console.log("VS Code YLang 2.0 syntax grammar tests passed");
