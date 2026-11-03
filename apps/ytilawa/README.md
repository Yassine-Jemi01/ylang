# YTilawa Extended (development MVP)

YTilawa is a terminal Quran recitation player written in YLang. It uses the public
MP3Quran.net API v3 for the Surah and reciter catalogs, and streams recitations
through YLang's HTTP stream and Voice runtimes.

## Current features

- Loads the 114-Surah catalog and Arabic reciter catalog at startup.
- Browses Surahs and reciters with arrow keys and page navigation.
- Searches Surah names with `/`.
- Streams the selected recitation using `http.stream_open` and `voice.play_stream`.
- Pause/resume, stop, next-selection, and volume controls.
- Persists favorites, playback history, and a default playlist as local text files.
- Downloads the selected MP3 using binary-safe HTTP reads and file appends.

## Build

Install the YLang build dependencies, libcurl, SDL2, and SDL2_mixer, then from
the repository root:

```sh
make
make voice
./build/ylang check apps/ytilawa/src/main.yl
./build/ylang build apps/ytilawa/src/main.yl -o ytilawa
./ytilawa
```

The program needs network access for the catalog and remote recitations. Downloaded
files and the favorites/history/playlist text files are written in the current
working directory.

## Controls

- Surah browser: Up/Down, Page Up/Page Down, Enter, `/` to search, Q to quit.
- Reciter browser: Up/Down, Enter, Escape to return.
- Player: P pause/resume, N next selection, S stop, F favorite, L add to playlist,
  D download, + / - volume, Q quit.

## Scope and limitations

This is an early, usable application skeleton, not a release candidate. The app
currently writes playlist/history records but does not yet provide dedicated
browsing/editing screens for them. Previous-track navigation, multiple named
playlists, resilient catalog caching, download progress/cancellation, and an
end-to-end HTTP-to-audio test are still outstanding. MP3 decoding depends on the
installed SDL2_mixer codec build. The C backend is required; NASM does not support
the optional HTTP/Voice/TUI runtime calls.

Catalog and audio URLs are provided by MP3Quran.net; YTilawa does not bundle or
redistribute recitation files.
