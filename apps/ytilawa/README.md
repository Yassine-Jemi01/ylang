# YTilawa Extended

YTilawa is a YLang terminal Quran player. It loads the 114-surah catalogue from the official MP3Quran.net API v3 and displays surah names as lowercase Latin slugs (for example, \`fatiha\`, \`baqara\`, and \`aal-imran\`). The reciter directory uses the API's English-language names and letter labels. Audio URLs are still derived only from the API-provided recording servers and surah coverage lists.

The terminal interface uses restrained ANSI colors and an arrow marker rather than a full-row selection block. Screens are updated in place with synchronized terminal frames instead of repeatedly clearing the display. Surah and reciter browsers redraw only when the selection changes, adapt their visible page size to the terminal height, and support arrow keys, Page Up/Down, Home, End, and `/` search. The reader browser puts a curated set of widely known reciters first; the API does not expose popularity statistics, so this is a maintained featured order rather than a live popularity ranking.

## Build

Install libcurl development files, SDL2_mixer with an MP3 decoder enabled, and a supported C compiler. From the repository root:

\`\`\`sh
make
make voice
./build/ylang check apps/ytilawa/main.yl
./build/ylang build apps/ytilawa/main.yl -o build/ytilawa
./build/ytilawa
\`\`\`

The program requires network access for the live catalogue and online recitations.

## Controls

| Key | Action |
|---|---|
| \`B\` | Browse surahs |
| \`R\` | Browse reciters |
| \`/\` | Search surah names |
| \`G\` | Search reader names (English) |
| \`Space\` | Play / pause / resume |
| \`N\` / \`P\` | Next / previous surah |
| \`S\` | Stop |
| \`F\` | Add current surah to favorites |
| \`A\` | Add current surah to the default playlist |
| \`H\` | Listening history |
| \`V\` | Favorites |
| \`C\` | Default playlist |
| \`D\` | Download current surah |
| \`L\` | Play a local MP3 path |
| \`+\` / \`-\` | Volume |
| \`Q\` | Quit |

Inside either browser: Up/Down moves one row, Page Up/Down moves one visible page, Home/End jumps to the beginning/end, `/` searches by a surah name (Latin or Arabic) or an English reader name, Enter selects, Escape returns, and Ctrl+C exits cleanly.

## Data source and URL validation

- Surah catalogue: \`https://www.mp3quran.net/api/v3/suwar?language=ar\`
- English reciter and recording metadata: \`https://www.mp3quran.net/api/v3/reciters?language=eng\`

The English-language API response retains the \`reciters[].moshaf[].server\` and \`surah_list\` fields used by playback. YTilawa checks that the selected surah is present before constructing the documented three-digit filename (for example, \`001.mp3\`). No reciter or audio-host URL is hard-coded. See the [official MP3Quran API documentation](https://www.mp3quran.net/eng/api/).

## Storage

Files are written in the current working directory:

- \`ytilawa-favorites.txt\`
- \`ytilawa-history.txt\`
- \`ytilawa-playlist-default.txt\`
- \`ytilawa-SURAH.mp3\` downloads (for example, \`ytilawa-baqara.mp3\`)

Downloads use \`http.stream_read_buffer\` and \`io.buffer_append_file\`, so MP3 data is written incrementally rather than accumulated in one in-memory string.

## Current limitations

This is an initial Extended implementation, not yet a release candidate. The default playlist is append-only; favorites cannot yet be removed; download progress/cancellation and offline catalogue caching are not implemented; the player selects the first moshaf that contains the chosen surah. MP3 playback depends on the codecs in the installed SDL2_mixer build. The HTTP-to-Voice bridge and interactive Windows raw-key path must pass their CI/runtime checks before production use.
