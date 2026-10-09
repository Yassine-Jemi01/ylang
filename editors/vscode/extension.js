"use strict";

const path = require("node:path");
const vscode = require("vscode");
const { LanguageClient, TransportKind } = require("vscode-languageclient/node");

let client;
let clientStart;

function activate(context) {
  const serverModule = context.asAbsolutePath(path.join("server", "server.js"));
  const serverOptions = {
    run: {
      command: process.execPath,
      args: [serverModule],
      transport: TransportKind.stdio
    },
    debug: {
      command: process.execPath,
      args: [serverModule],
      transport: TransportKind.stdio,
      options: { execArgv: ["--nolazy", "--inspect=6009"] }
    }
  };

  const clientOptions = {
    documentSelector: [
      { scheme: "file", language: "ylang" },
      { scheme: "untitled", language: "ylang" }
    ],
    synchronize: {
      configurationSection: "ylang"
    },
    outputChannelName: "YLang Language Server"
  };

  client = new LanguageClient(
    "ylangLanguageServer",
    "YLang Language Server",
    serverOptions,
    clientOptions
  );

  clientStart = client.start().catch((error) => {
    vscode.window.showErrorMessage("YLang Language Server failed to start: " + error.message);
    throw error;
  });

  context.subscriptions.push(
    vscode.commands.registerCommand("ylang.checkCurrentFile", async () => {
      const editor = vscode.window.activeTextEditor;
      if (!editor || editor.document.languageId !== "ylang") {
        vscode.window.showInformationMessage("Open a YLang (.yl) file first.");
        return;
      }
      try {
        await clientStart;
        await client.sendNotification("ylang/checkDocument", {
          uri: editor.document.uri.toString()
        });
      } catch (error) {
        vscode.window.showErrorMessage("Could not check YLang file: " + error.message);
      }
    }),
    vscode.commands.registerCommand("ylang.showOutput", () => {
      vscode.commands.executeCommand("workbench.action.output.toggleOutput");
    })
  );

  context.subscriptions.push({
    dispose: () => {
      if (client) void client.stop();
    }
  });
}

async function deactivate() {
  if (client) {
    const running = client;
    client = undefined;
    await running.stop();
  }
}

module.exports = { activate, deactivate };
