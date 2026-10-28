use zed_extension_api::{self as zed, Result};

struct YLangExtension;

impl zed::Extension for YLangExtension {
    fn new() -> Self {
        Self
    }

    fn language_server_command(
        &mut self,
        language_server_id: &zed::LanguageServerId,
        _worktree: &zed::Worktree,
    ) -> Result<zed::Command> {
        if language_server_id.as_ref() != "ylang-lsp" {
            return Err(format!("unknown language server: {language_server_id}"));
        }

        Ok(zed::Command {
            command: "ylang-lsp".to_string(),
            args: Vec::new(),
            env: Vec::new(),
        })
    }
}

zed::register_extension!(YLangExtension);
