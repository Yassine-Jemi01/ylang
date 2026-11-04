# YTilawa Extended

YTilawa is a YLang terminal Quran player. It loads the 114-surah and reciter catalogues from the official MP3Quran.net API v3, then derives each audio URL from the API-provided recording server and coverage list.

## Build

Install libcurl development files, SDL2_mixer with an MP3 decoder enabled, and a supported C compiler. From the repository root:

```sh
make
make voice
./build/ylang check apps/ytilawa/main.yl
./build/ylang build apps/ytilawa/main.yl -o build/ytilawa
./build/ytilawa
```

The program requires network access for the live catalogue and online recitations.

## Controls

| Key | Action |
|---|---|
| `B` | Browse surahs |
| `R` | Browse reciters |
| `/` | Search surah names |
| `G` | Search reciter names |
| `Space` | Play / pause / resume |
| `N` / `P` | Next / previous surah |
| `S` | Stop |
| `F` | Add current surah to favorites |
| `A` | Add current surah to the default playlist |
| `H` | Listening history |
| `V` | Favorites |
| `C` | Default playlist |
| `D` | Download current surah |
| `L` | Play a local MP3 path |
| `+` / `-` | Volume |
| `Q` | Quit |

## Data source and URL validation

- Surah catalogue: `https://www.mp3quran.net/api/v3/suwar?language=ar`
- Reciter and recording metadata: `https://www.mp3quran.net/api/v3/reciters?language=ar`

YTilawa reads each reciter's `moshaf[].server` and `moshaf[].surah_list` values. It checks that the selected surah is present before constructing the documented three-digit filename (for example, `001.mp3`). No reciter or audio-host URL is hard-coded. See the [official MP3Quran API documentation](https://www.mp3quran.net/eng/api/).

## Storage

Files are written in the current working directory:

- `ytilawa-favorites.txt`
- `ytilawa-history.txt`
- `ytilawa-playlist-default.txt`
- `ytilawa-NNN.mp3` downloads

Downloads use `http.stream_read_buffer` and `io.buffer_append_file`, so MP3 data is written incrementally rather than accumulated in one in-memory string.

## Current limitations

This is an initial Extended implementation, not yet a release candidate. The default playlist is append-only; favorites cannot yet be removed; download progress/cancellation and offline catalogue caching are not implemented; the player selects the first moshaf that contains the chosen surah. MP3 playback depends on the codecs in the installed SDL2_mixer build. The HTTP-to-Voice bridge and interactive Windows raw-key path must pass their CI/runtime checks before production use.
