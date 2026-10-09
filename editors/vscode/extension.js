"use strict";

const childProcess = require("node:child_process");
const os = require("node:os");
const path = require("node:path");
const vscode = require("vscode");
const { LanguageClient, TransportKind } = require("vscode-languageclient/node");

let client;
let clientStart;
let startupError;
let output;

function expandUserPath(value) {
  if (value === "~") return os.homedir();
  if (value.startsWith("~/") || value.startsWith("~\\\\")) {
    return path.join(os.homedir(), value.slice(2));
  }
  return value;
}

function runFile(executable, args, cwd) {
  return new Promise((resolve, reject) => {
    childProcess.execFile(executable, args, {
      cwd,
      windowsHide: true,
      timeout: 120000,
      maxBuffer: 4 * 1024 * 1024,
      encoding: "utf8"
    }, (error, stdout, stderr) => {
      if (error) {
        error.stdout = stdout || "";
        error.stderr = stderr || "";
        reject(error);
      } else {
        resolve({ stdout: stdout || "", stderr: stderr || "" });
      }
    });
  });
}

function appendResult(channel, result) {
  if (result.stdout) channel.append(result.stdout);
  if (result.stderr) channel.append(result.stderr);
  const text = (result.stdout || "") + (result.stderr || "");
  if (text && !text.endsWith("\n")) channel.appendLine("");
}

function activate(context) {
  const serverModule = context.asAbsolutePath(path.join("server", "server.js"));
  const serverOptions = {
    run: {
      module: serverModule,
      transport: TransportKind.stdio
    },
    debug: {
      module: serverModule,
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

  output = vscode.window.createOutputChannel("YLang");
  context.subscriptions.push(output);

  clientStart = client.start().catch((error) => {
    startupError = error;
    vscode.window.showErrorMessage("YLang Language Server failed to start: " + error.message);
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
        if (startupError) return;
        await client.sendNotification("ylang/checkDocument", {
          uri: editor.document.uri.toString()
        });
      } catch (error) {
        vscode.window.showErrorMessage("Could not check YLang file: " + error.message);
      }
    }),
    vscode.commands.registerCommand("ylang.buildAndRunCurrentFile", async () => {
      const editor = vscode.window.activeTextEditor;
      if (!editor || editor.document.languageId !== "ylang") {
        vscode.window.showInformationMessage("Open a YLang (.yl) file first.");
        return;
      }
      if (editor.document.uri.scheme !== "file") {
        vscode.window.showInformationMessage("Save this YLang file to disk before building it.");
        return;
      }
      if (!(await editor.document.save()) || editor.document.isDirty) return;

      const sourcePath = editor.document.uri.fsPath;
      const directory = path.dirname(sourcePath);
      const name = path.basename(sourcePath, path.extname(sourcePath));
      const executablePath = path.join(
        directory,
        name + (process.platform === "win32" ? ".exe" : "")
      );
      const compilerPath = expandUserPath(
        vscode.workspace.getConfiguration("ylang").get("compilerPath", "ylang")
      );

      output.clear();
      output.appendLine("YLang: building " + sourcePath);
      output.appendLine("Compiler: " + compilerPath);
      output.show(true);

      try {
        const result = await runFile(
          compilerPath,
          ["build", sourcePath, "-o", executablePath],
          directory
        );
        appendResult(output, result);
        output.appendLine("Build succeeded: " + executablePath);
        const terminal = vscode.window.createTerminal({
          name: "YLang: " + name,
          cwd: directory,
          shellPath: executablePath,
          shellArgs: []
        });
        terminal.show();
      } catch (error) {
        appendResult(output, { stdout: error.stdout, stderr: error.stderr });
        output.appendLine("Build failed: " + String(error.message || error));
        if (error.code === "ENOENT") {
          vscode.window.showErrorMessage(
            "YLang compiler not found. Set ylang.compilerPath to the compiler executable (ylang.exe on Windows)."
          );
        } else {
          vscode.window.showErrorMessage("YLang build failed. See the YLang output channel for details.");
        }
      }
    }),
    vscode.commands.registerCommand("ylang.showOutput", () => {
      vscode.commands.executeCommand("workbench.action.output.toggleOutput");
    })
  );

}

async function deactivate() {
  if (client) {
    const running = client;
    client = undefined;
    await running.stop();
  }
}

module.exports = { activate, deactivate };
