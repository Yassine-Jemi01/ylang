use zed_extension_api::{self as zed, Result};

struct YLangExtension;

impl zed::Extension for YLangExtension {
    fn new() -> Self {
        Self
    }

    fn language_server_command(
        &mut self,
        language_server_id: &zed::LanguageServerId,
        worktree: &zed::Worktree,
    ) -> Result<zed::Command> {
        if language_server_id.as_ref() != "ylang-lsp" {
            return Err(format!("unknown language server: {language_server_id}"));
        }

        let command = worktree.which("ylang-lsp").ok_or_else(|| {
            "Could not find `ylang-lsp` on PATH. Install the YLang toolchain and editor language server, then restart Zed.".to_string()
        })?;

        Ok(zed::Command {
            command,
            args: Vec::new(),
            env: worktree.shell_env(),
        })
    }
}

zed::register_extension!(YLangExtension);
