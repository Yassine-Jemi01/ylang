#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CUSTOM_ICON="$SCRIPT_DIR/icons/ylang-dark.svg"

if [[ ! -f "$CUSTOM_ICON" ]]; then
  echo "YLang icon not found: $CUSTOM_ICON" >&2
  echo "Run git pull in your YLang repository first." >&2
  exit 1
fi

SOURCE_THEME_FILE=""
for ext_root in \
  "$HOME/.vscode/extensions" \
  "$HOME/.vscode-insiders/extensions" \
  "$HOME/.vscode-oss/extensions" \
  "$HOME/.var/app/com.visualstudio.code/data/vscode/extensions" \
  "$HOME/.var/app/com.visualstudio.code/config/Code/extensions"; do
  [[ -d "$ext_root" ]] || continue
  SOURCE_THEME_FILE="$(find "$ext_root" -path '*/catppuccin.catppuccin-vsc-icons-*/dist/mocha/theme.json' -print -quit 2>/dev/null || true)"
  if [[ -n "$SOURCE_THEME_FILE" ]]; then
    break
  fi
  SOURCE_THEME_FILE="$(find "$ext_root" -path '*/Catppuccin.catppuccin-vsc-icons-*/dist/mocha/theme.json' -print -quit 2>/dev/null || true)"
  if [[ -n "$SOURCE_THEME_FILE" ]]; then
    break
  fi
done

if [[ -z "$SOURCE_THEME_FILE" ]]; then
  echo "Could not find Catppuccin Icons' generated Mocha theme." >&2
  echo "Install/enable 'Catppuccin Icons for VSCode' and run this script again." >&2
  exit 1
fi

SOURCE_EXTENSION="$(dirname -- "$(dirname -- "$(dirname -- "$SOURCE_THEME_FILE")")")"
DEST="$HOME/.vscode/extensions/yassine-jemi01.ylang-catppuccin-mocha-icons-0.1.0"

mkdir -p "$DEST/dist"
rm -rf "$DEST/dist/mocha"
cp -a "$SOURCE_EXTENSION/dist/mocha" "$DEST/dist/"
cp "$CUSTOM_ICON" "$DEST/dist/mocha/icons/ylang-custom.svg"

python3 - "$DEST/dist/mocha/theme.json" <<'PY'
import json
import sys
from pathlib import Path

theme_path = Path(sys.argv[1])
with theme_path.open("r", encoding="utf-8") as stream:
    theme = json.load(stream)

theme.setdefault("iconDefinitions", {})["_ylang_custom"] = {
    "iconPath": "./icons/ylang-custom.svg"
}
theme.setdefault("fileExtensions", {})["yl"] = "_ylang_custom"

with theme_path.open("w", encoding="utf-8") as stream:
    json.dump(theme, stream, indent=2)
    stream.write("\n")
PY

cat > "$DEST/package.json" <<'JSON'
{
  "name": "ylang-catppuccin-mocha-icons",
  "displayName": "YLang Catppuccin Mocha Icons",
  "description": "Catppuccin Mocha file icons with a custom YLang .yl icon.",
  "version": "0.1.0",
  "publisher": "yassine-jemi01",
  "engines": {
    "vscode": "^1.85.0"
  },
  "categories": [
    "Other"
  ],
  "contributes": {
    "iconThemes": [
      {
        "id": "ylang-catppuccin-mocha",
        "label": "YLang Catppuccin Mocha (YLang icon)",
        "path": "./dist/mocha/theme.json"
      }
    ]
  }
}
JSON

echo
echo "Installed: YLang Catppuccin Mocha (YLang icon)"
echo "In VS Code, set workbench.iconTheme to: ylang-catppuccin-mocha"
echo "This is a local copy of your currently installed Catppuccin Mocha icons."
echo "To refresh its icons after Catppuccin updates, rerun this script."
