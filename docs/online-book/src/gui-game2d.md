# GUI and Game2D

The optional GUI runtime uses SDL2. The Game2D helper module is a small, experimental layer for learning and simple 2D prototypes—not a general-purpose game engine.

It provides helpers for window lifecycle, event polling, keyboard and mouse input, elapsed frame time, rectangle drawing, collision checks, and presentation. The repository includes a playable Dodge example.

## Build the example

Install SDL2 development files and `pkg-config` for your platform, then from the repository root run:

```sh
make game2d-example
```

This builds the example as `build/dodge` on Linux and macOS, or `build/dodge.exe` on Windows. Run it from a graphical desktop session:

```sh
./build/dodge
```

On Windows, run `build/dodge.exe` from the appropriate MSYS2 environment.

## Understand the limitations

Headless CI verifies type-checking, compilation, linking, and non-window helper behavior. It does not verify that a live window behaves correctly on every desktop or driver. Test the game on a real graphical session before relying on it.

The library does not yet provide sprites, advanced animation, audio, a scene system, or a complete physics engine. See the [Game2D Library Reference](reference/GAME2D.md) and the [GUI API Contract](reference/GUI-API-SPEC.md).
