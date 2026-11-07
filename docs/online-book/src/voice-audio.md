# YLang Voice and Audio API (experimental)

YLang's optional Voice runtime plays local audio files and progressive HTTP audio streams through SDL2_mixer. It is intended for narration, recitation playback, and game audio. It is **not** a text-to-speech engine and does not synthesize speech from text.

## Requirements

Install SDL2, SDL2_mixer, and pkg-config development files.

- Fedora: `sudo dnf install SDL2-devel SDL2_mixer-devel pkgconf-pkg-config`
- Debian/Ubuntu: `sudo apt install libsdl2-dev libsdl2-mixer-dev pkg-config`
- macOS: `brew install sdl2 sdl2_mixer pkg-config`
- Windows: use the MSYS2 MINGW64 environment and install `mingw-w64-x86_64-SDL2`, `mingw-w64-x86_64-SDL2_mixer`, and `mingw-w64-x86_64-pkgconf`.

Build the compiler normally, then build an example:

```sh
make
make voice
make voice-example
```

The compiler automatically links the optional runtime when a program calls a `voice.*` builtin. The runtime source and header must be available in the source tree, or installed with `make install-voice`.

## API

| Function | Return type | Behavior |
|---|---|---|
| `voice.init()` | `bool` | Initialize SDL audio and the mixer |
| `voice.play(path)` | `bool` | Load and play one local audio file, replacing the current track |
| `voice.play_stream(stream)` | `bool` | Consume an `http_stream` progressively through a custom SDL_RWops reader |
| `voice.pause()` | `void` | Pause music playback |
| `voice.resume()` | `void` | Resume paused playback |
| `voice.stop()` | `void` | Stop playback and release the loaded track |
| `voice.is_playing()` | `bool` | Return whether music is actively playing; reaps a completed track |
| `voice.is_paused()` | `bool` | Return whether the current music track is paused |
| `voice.is_finished()` | `bool` | Detect natural end-of-track and release its mixer resource |
| `voice.set_volume(percent)` | `bool` | Set music volume from 0 through 100 |
| `voice.last_error()` | `string` | Return the latest runtime error, or `No error` |
| `voice.shutdown()` | `void` | Stop playback and release audio resources |

No import is needed for these compiler-recognized builtins.

## Example

```ylang
function main() -> int {
    if (!voice.init()) {
        print(voice.last_error());
        return 1;
    }

    if (!voice.set_volume(80)) {
        print(voice.last_error());
        voice.shutdown();
        return 1;
    }

    if (!voice.play("recitation.mp3")) {
        print(voice.last_error());
        voice.shutdown();
        return 1;
    }

    print("Playback started.");
    voice.pause();
    voice.resume();
    voice.stop();
    voice.shutdown();
    return 0;
}
```

A runnable source example is available at [examples/voice.yl](https://github.com/Yassine-Jemi01/ylang/blob/main/examples/voice.yl).

## Formats and limitations

SDL2_mixer supports formats according to the codecs enabled in the installed build. Commonly available formats include WAV, Ogg Vorbis, FLAC, and MP3; do not assume every format is enabled on every platform.

This runtime manages one music track. `voice.play_stream(http_stream)` transfers ownership of the HTTP stream to the Voice runtime and reads it incrementally through SDL_RWops; the handle must not be used again by the caller. If stream setup fails, the runtime still closes the transferred handle. The custom source is forward-only and does not support seeking. Serialize playback and lifecycle calls on one owner thread. Stream callbacks may run on SDL_mixer's audio thread, so they must be thread-safe and must not call Voice playback/lifecycle functions; `ylvoice_last_error()` is safe to call from other threads. Specific HTTP stream callback errors are preserved if decoding or playback startup fails, rather than being hidden by a generic SDL_mixer message. Error-state updates are synchronized across the playback and decoder paths; the C `ylvoice_last_error()` accessor returns a per-thread snapshot valid until its next call on that thread. `voice.is_finished()` reports natural end-of-track; polling either playback-state query releases the completed mixer object. Codec support for MP3 and other formats still depends on the installed SDL2_mixer build. It does not yet provide text-to-speech, audio recording, multiple simultaneous music streams, per-sound channels, playlists, or playback-rate changes.

The C backend is the supported backend for this optional runtime. The experimental NASM backend does not implement `voice.*`.
