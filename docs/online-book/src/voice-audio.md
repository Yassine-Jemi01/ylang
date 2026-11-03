# YLang Voice and Audio API (experimental)

YLang's optional Voice runtime plays local audio files through SDL2_mixer. It is intended for narration, recitation playback, and game audio. It is **not** a text-to-speech engine and does not synthesize speech from text.

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
| `voice.play(path)` | `bool` | Load and play one audio file, replacing the current track |
| `voice.pause()` | `void` | Pause music playback |
| `voice.resume()` | `void` | Resume paused playback |
| `voice.stop()` | `void` | Stop playback and release the loaded track |
| `voice.is_playing()` | `bool` | Return whether the mixer reports active music playback |
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

A runnable source example is available at [examples/voice.yl](../../examples/voice.yl).

## Formats and limitations

SDL2_mixer supports formats according to the codecs enabled in the installed build. Commonly available formats include WAV, Ogg Vorbis, FLAC, and MP3; do not assume every format is enabled on every platform.

This initial runtime manages one music stream. It does not yet provide text-to-speech, audio recording, multiple simultaneous music streams, per-sound channels, playlists, seeking, playback-rate changes, or streaming from network URLs. These are separate future features, not implied by the current API.

The C backend is the supported backend for this optional runtime. The experimental NASM backend does not implement `voice.*`.
