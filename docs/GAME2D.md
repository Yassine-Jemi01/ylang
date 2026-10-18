# YLang Game2D: the first playable-game library

The experimental game2d.yl module is a small, source-level helper layer over YLang's optional SDL2 GUI runtime. It is intended for learning and small 2D prototypes, not as a full game engine.

## What is included

- Create and close one window.
- Run a frame loop with event polling, background clear, and present.
- Read keyboard state, mouse position, and mouse buttons.
- Measure elapsed time between frames.
- Draw filled rectangles with RGB colors.
- Test rectangle overlap and point-in-rectangle.
- A complete playable Dodge example that demonstrates movement, timing, collision detection, and score.

This version does not yet provide sprites or image loading, text rendering, audio, animation systems, physics, scene management, or multiple active windows. image.open() opens a file in the operating system's default viewer; it does not draw a sprite inside the game window.

## Requirements

- A YLang checkout with the compiler built.
- SDL2 development files and pkg-config.
- A graphical desktop session to run games. CI can check and compile the code, but it cannot interactively test a real game window.

For Fedora:

    sudo dnf install SDL2-devel pkgconf-pkg-config

For Debian or Ubuntu:

    sudo apt install libsdl2-dev pkg-config

For macOS with Homebrew:

    brew install sdl2 pkg-config

## Build and play the example

From the repository root, build YLang first, then check and build the sample:

    make
    make test
    YLANG_PATH=stdlib ./build/ylang check examples/games/dodge.yl
    YLANG_PATH=stdlib ./build/ylang build examples/games/dodge.yl -o build/dodge
    ./build/dodge

Controls:

- Left Arrow: move the player left.
- Right Arrow: move the player right.
- Escape: quit.
- Avoid the falling red block. The score increases each time it passes you.

The example writes instructions and the final score to the terminal. The current renderer has no text-drawing API, so the score is not drawn inside the window.

## Install the optional runtime and helper module

    make install-gui PREFIX="$HOME/.local"

For a user-local installation, make the installed YLang module visible to imports by setting YLANG_PATH to the share directory. For example:

    YLANG_PATH="$HOME/.local/share/ylang" ylang build main.yl -o game

Then import the installed module by its path:

    from "stdlib/game2d.yl" import game2d_open, game2d_begin_frame, game2d_draw_rect, game2d_end_frame, game2d_close, game2d_key_down, game2d_delta_ms;

The example in this repository uses a relative import because it is intended to run from the checkout.

## API reference

| Function | Purpose |
| --- | --- |
| game2d_open(title, width, height) | Initialize SDL2, create a window, and start the frame timer. Returns bool. |
| game2d_begin_frame(red, green, blue) | Poll events and clear the frame background. Returns false when the user closes the window or an error occurs. |
| game2d_draw_rect(x, y, width, height, red, green, blue) | Draw a filled rectangle. Coordinates start at the top-left. |
| game2d_end_frame() | Present the finished frame. |
| game2d_key_down(name) | Test whether a key is held, e.g. "Left", "Right", "Space", or "Escape". |
| game2d_mouse_x() / game2d_mouse_y() | Read the current mouse position. |
| game2d_mouse_button_down(button) | Test a mouse button; left is 1, middle 2, right 3. |
| game2d_ticks_ms() | Read elapsed SDL time in milliseconds. |
| game2d_delta_ms() | Get milliseconds since the previous call. Clamp large values after pauses to avoid big simulation jumps. |
| game2d_rects_overlap(ax, ay, aw, ah, bx, by, bw, bh) | Axis-aligned rectangle collision test. Rectangles that only touch at an edge do not overlap. |
| game2d_point_in_rect(px, py, rx, ry, width, height) | Check whether a point is inside a rectangle; the right and bottom edges are excluded. |
| game2d_last_error() | Return the last GUI/runtime error message. |
| game2d_close() | Close the window and shut down SDL2. Call it before returning from the game. |

RGB channels must be between 0 and 255, and rectangle widths and heights must be positive. Always exit the loop cleanly and call game2d_close() so SDL resources are released.

## Example API pattern

    from "stdlib/game2d.yl" import game2d_open, game2d_begin_frame, game2d_draw_rect, game2d_end_frame, game2d_close;

    function main() -> int {
        if (not game2d_open("My game", 640, 400)) {
            return 1;
        }
        loop() {
            if (not game2d_begin_frame(20, 24, 35)) {
                break;
            }
            if (not game2d_draw_rect(80, 80, 40, 40, 80, 190, 220)) {
                break;
            }
            if (not game2d_end_frame()) {
                break;
            }
        }
        game2d_close();
        return 0;
    }

The API and helper implementation are experimental and may change before the stable YLang 2.0 release.
