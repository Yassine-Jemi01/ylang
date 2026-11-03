# YLang Voice runtime (experimental)

The optional Voice runtime provides music/voice-file playback through SDL2_mixer.
It is designed as a small playback backend for narration, recitation, and game audio,
not as a text-to-speech synthesizer.

## YLang API\n\nThe compiler recognizes these builtins directly; no import is required:\n\n| Function | Return | Purpose |\n|---|---|---|\n| `voice.init()` | `bool` | Initialize the audio device |\n| `voice.play(path)` | `bool` | Play one file, replacing the current track |\n| `voice.pause()` / `voice.resume()` | `void` | Pause or resume playback |\n| `voice.stop()` | `void` | Stop and release the current track |\n| `voice.is_playing()` | `bool` | Check playback state |\n| `voice.set_volume(0..100)` | `bool` | Set music volume |\n| `voice.last_error()` | `string` | Read the latest audio error |\n| `voice.shutdown()` | `void` | Release audio resources |\n\nSee [`examples/voice.yl`](../../examples/voice.yl) for a complete compiler-level example.\n\n## Current C API

- `ylvoice_init()` — initialize the audio device.
- `ylvoice_play(path)` — load and play one audio file, replacing the previous track.
- `ylvoice_pause()`, `ylvoice_resume()`, `ylvoice_stop()`.
- `ylvoice_is_playing()` — report whether playback is active.
- `ylvoice_set_volume(percent)` — set music volume from 0 to 100.
- `ylvoice_last_error()` — retrieve the latest error.
- `ylvoice_shutdown()` — release the track and audio device.

The runtime uses SDL2_mixer codecs available in the installed build (commonly WAV,
Ogg Vorbis, FLAC, and MP3). Codec support depends on how SDL2_mixer was packaged.
It currently supports one music stream at a time. It does not yet expose TTS,
multiple simultaneous voices, seeking, playlists, or a YLang-level `voice.*`
binding; those require the next compiler/runtime integration milestone.

## Build

Install SDL2, SDL2_mixer, and pkg-config development files, then run:

```sh
make voice
```

Fedora: `sudo dnf install SDL2-devel SDL2_mixer-devel pkgconf-pkg-config`

Debian/Ubuntu: `sudo apt install libsdl2-dev libsdl2-mixer-dev pkg-config`

macOS: `brew install sdl2 sdl2_mixer pkg-config`
