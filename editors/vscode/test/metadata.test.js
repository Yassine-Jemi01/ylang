"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

const root = path.join(__dirname, "..");
const manifest = JSON.parse(fs.readFileSync(path.join(root, "package.json"), "utf8"));

assert.equal(manifest.publisher, "yassine-jemi01");
assert.equal(manifest.version, "0.6.0");
assert.ok(manifest.engines.vscode, "the VS Code engine range must be declared");
assert.ok(manifest.main && fs.existsSync(path.join(root, manifest.main)), "the extension entry point must exist");

const language = manifest.contributes.languages.find((item) => item.id === "ylang");
assert.ok(language, "the ylang language must be contributed");
assert.ok(language.extensions.includes(".yl"), "YLang files must be registered as .yl");
assert.equal(language.configuration, "./language-configuration.json");

const commands = new Map((manifest.contributes.commands || []).map((item) => [item.command, item.title]));
assert.equal(commands.get("ylang.checkCurrentFile"), "YLang: Check Current File");
assert.equal(commands.get("ylang.showOutput"), "YLang: Show Output");
assert.equal(commands.get("ylang.debugCurrentFile"), "YLang: Debug Current File");

const extensionConfigPath = path.join(root, "extension.js");
const extensionSource = fs.readFileSync(extensionConfigPath, "utf8");
assert.match(extensionSource, /registerCommand\("ylang\.debugCurrentFile", debugCurrentFile\)/,
  "debug command must be registered in activate()");
assert.match(extensionSource, /shellArgs: \["debug", sourcePath, "-o", executablePath\]/,
  "debug command must pass the source file and output path to the compiler");

console.log("VS Code YLang extension manifest tests passed");
