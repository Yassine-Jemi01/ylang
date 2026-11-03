# YLang Voice runtime (experimental)

The optional Voice runtime provides music/voice-file playback through SDL2_mixer.
It is designed as a small playback backend for narration, recitation, and game audio,
not as a text-to-speech synthesizer.

## YLang API

The compiler recognizes these builtins directly; no import is required:

| Function | Return | Purpose |
|---|---|---|
| `voice.init()` | `bool` | Initialize the audio device |
| `voice.play(path)` | `bool` | Play one local file, replacing the current track |
| `voice.play_stream(stream)` | `bool` | Consume an `http_stream` and decode it progressively through SDL2_mixer |
| `voice.pause()` / `voice.resume()` | `void` | Pause or resume playback |
| `voice.stop()` | `void` | Stop and release the current track |
| `voice.is_playing()` | `bool` | Check whether music is actively playing |
| `voice.is_paused()` | `bool` | Check whether the current track is paused |
| `voice.is_finished()` | `bool` | Detect natural end-of-track and release the finished music object |
| `voice.set_volume(0..100)` | `bool` | Set music volume |
| `voice.last_error()` | `string` | Read the latest audio error |
| `voice.shutdown()` | `void` | Release audio resources |

See [`examples/voice.yl`](../../examples/voice.yl) for a complete compiler-level example.

## Current C API

- `ylvoice_init()` — initialize the audio device.
- `ylvoice_play(path)` — load and play one local audio file, replacing the previous track.
- `ylvoice_play_stream(stream, read, error, close)` — consume a pull-based stream through a custom SDL_RWops reader.
- `ylvoice_pause()`, `ylvoice_resume()`, `ylvoice_stop()`.
- `ylvoice_is_playing()` — report whether playback is active and reap a naturally completed track.
- `ylvoice_is_paused()` — report whether the current track is paused.
- `ylvoice_is_finished()` — report natural completion and release the finished mixer object.
- `ylvoice_set_volume(percent)` — set music volume from 0 to 100.
- `ylvoice_last_error()` — retrieve the latest error.
- `ylvoice_shutdown()` — release the track and audio device.

The runtime uses SDL2_mixer codecs available in the installed build (commonly WAV,
Ogg Vorbis, FLAC, and MP3). Codec support depends on how SDL2_mixer was packaged.
It currently supports one music track at a time. Natural completion is observable
through `voice.is_finished()`; polling either state query releases the completed mixer
object. `voice.play_stream(http_stream)` transfers ownership of the stream to the audio
runtime and reads incrementally through SDL_RWops; do not use the HTTP handle again after
passing it to Voice. The stream reader is forward-only and does not support seeking.
Text-to-speech, multiple simultaneous voices, seeking, playlists, and recording are not
implemented yet.

## Build

Install SDL2, SDL2_mixer, and pkg-config development files, then run:

```sh
make voice
```

Fedora: `sudo dnf install SDL2-devel SDL2_mixer-devel pkgconf-pkg-config`

Debian/Ubuntu: `sudo apt install libsdl2-dev libsdl2-mixer-dev pkg-config`

macOS: `brew install sdl2 sdl2_mixer pkg-config`
