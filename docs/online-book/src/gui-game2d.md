# GUI and Game2D

The optional GUI runtime uses SDL2. The Game2D helper module is a small, experimental layer for learning and simple 2D prototypes—not a general-purpose game engine.

It provides helpers for window lifecycle, event polling, keyboard and mouse input, elapsed frame time, rectangle drawing, collision checks, and presentation. The repository includes a playable Dodge example. This chapter explains the setup and development workflow; the [Game2D Library Reference](reference/GAME2D.md) and [GUI API Contract](reference/GUI-API-SPEC.md) define the exact API and its limitations.

## Before you start

You need a working YLang compiler, a supported C toolchain, SDL2 development files, and \`pkg-config\`. Runtime testing also requires a graphical desktop session. The SDL2 runtime alone may not provide the headers and metadata needed to compile the example.

On Fedora, install the development dependencies with:

\`\`\`sh
sudo dnf install SDL2-devel pkgconf-pkg-config
\`\`\`

On Debian or Ubuntu:

\`\`\`sh
sudo apt install libsdl2-dev pkg-config
\`\`\`

On macOS with Homebrew:

\`\`\`sh
brew install sdl2 pkg-config
\`\`\`

For Windows, follow the dedicated [Windows Setup](reference/WINDOWS.md) instructions; the native path has additional toolchain and packaging restrictions.

## Build the example

From the repository root, run:

\`\`\`sh
make game2d-example
\`\`\`

This builds the example as \`build/dodge\` on Linux and macOS, or \`build/dodge.exe\` on Windows. Run it from a graphical desktop session:

\`\`\`sh
./build/dodge
\`\`\`

On Windows, run \`build/dodge.exe\` from the appropriate MSYS2 environment.

If the build cannot find SDL2, check that its development files are installed and that \`pkg-config\` can locate them. If the program compiles but no window appears, confirm that you are running it in a graphical session rather than a headless terminal or CI environment.

## What the frame loop does

A small real-time application typically repeats a few tasks:

1. Poll input and window events so the application can respond to user actions.
2. Update the program state, such as player position.
3. Check collisions or other simple game rules.
4. Draw the current frame.
5. Present the frame and continue until the user closes the window.

Keep the update logic separate from drawing where practical. This makes the game easier to reason about and makes non-graphical logic easier to test.

## Understand the limitations

Headless CI verifies type-checking, compilation, linking, and non-window helper behavior. It does not verify that a live window behaves correctly on every desktop, display server, or graphics driver. Test the game on a real graphical session before relying on it.

The library does not yet provide sprites, advanced animation, audio, a scene system, or a complete physics engine. Do not assume it is a replacement for a mature game engine. Check the API contract for supported functions and the exact behavior of window and event operations.

## Troubleshooting

- **SDL2 headers or libraries are missing:** install the development package for your platform.
- **\`pkg-config\` cannot find SDL2:** check the package installation and environment paths.
- **The example builds but no window appears:** run it from an interactive graphical desktop session.
- **A feature is missing:** check the Game2D reference before assuming it exists.
- **The behavior differs across operating systems:** record the platform, compiler version, and smallest example, then compare the documented support levels.

## Practice

Build and run the Dodge example. Identify where it reads input, updates game state, checks collisions, and draws the frame. Make one small change, rebuild, and test it in a live graphical session. Keep the change within the API described by the Game2D reference.