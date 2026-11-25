# YLang Game2D: the first playable-game library

The experimental game2d.yl module is a small, source-level helper layer over YLang's optional SDL2 GUI runtime. It is intended for learning and small 2D prototypes, not as a full game engine.

## What is included

- Create and close one window.
- Run a frame loop with event polling, background clear, and present.
- Read held, newly pressed, and newly released keyboard keys, plus mouse position and buttons.
- Measure elapsed time between frames.
- Draw filled rectangles, compact bitmap text, and BMP sprites.
- Generate repeatable pseudo-random values and use small animation/physics helpers.
- Test rectangle overlap and point-in-rectangle.
- A complete playable Dodge example that demonstrates movement, timing, collision detection, and score.

PNG/JPEG loading, audio playback through Game2D, a scene manager, and multiple active windows remain outside this update. BMP loading uses SDL2 directly; audio playback is available separately through YLang's optional Voice library. Text uses a built-in 3x5 bitmap font and does not require SDL2_ttf.

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
    make game2d-example
    ./build/dodge

Controls:

- Left Arrow: move the player left.
- Right Arrow: move the player right.
- Escape: quit.
- Avoid the falling red block. The score increases each time it passes you.

The Dodge example still reports its final score in the terminal. Game2D now also supports compact in-window text for HUDs and score labels.

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
| game2d_key_down(name) | Test whether a key is held. |
| game2d_key_pressed(name) | Test whether a key was pressed during the current frame (non-repeating key-down). |
| game2d_key_released(name) | Test whether a key was released during the current frame. |
| game2d_draw_text(text, x, y, scale, red, green, blue) | Draw compact 3x5 bitmap text; scale must be 1–16. |
| game2d_load_bmp(path) | Load a BMP image and return an integer handle, or -1 on failure. |
| game2d_draw_image(handle, x, y) | Draw a loaded BMP image at its native dimensions. |
| game2d_free_image(handle) | Release a loaded image handle. Images are also released when the window closes. |
| game2d_random_seed(seed) / game2d_random_range(min, max) | Seed and use the Park-Miller pseudo-random generator; not for cryptographic use. |
| game2d_animation_frame(elapsed, duration, count, looping) | Calculate a frame index for a time-based animation. |
| game2d_lerp / game2d_clamp_float / game2d_move_towards | Basic floating-point motion helpers. |
| game2d_circles_overlap | Circle collision test using squared distances. |
| game2d_mouse_x() / game2d_mouse_y() | Read the current mouse position. |
| game2d_mouse_button_down(button) | Test a mouse button; left is 1, middle 2, right 3. |
| game2d_ticks_ms() | Read elapsed SDL time in milliseconds. |
| game2d_delta_ms() | Get milliseconds since the previous call. Clamp large values after pauses to avoid big simulation jumps. |
| game2d_rects_overlap(ax, ay, aw, ah, bx, by, bw, bh) | Axis-aligned rectangle collision test. Rectangles that only touch at an edge do not overlap. |
| game2d_point_in_rect(px, py, rx, ry, width, height) | Check whether a point is inside a rectangle; the right and bottom edges are excluded. |
| game2d_last_error() | Return the last GUI/runtime error message. |
| game2d_close() | Close the window and shut down SDL2. Call it before returning from the game. |

`game2d_delta_ms()` initializes its timing baseline when `game2d_open()` succeeds; clamp unusually large deltas after pauses. The collision helpers are deterministic and do not open a window, but `game2d.yl` also wraps the optional GUI runtime, so importing the combined module still requires SDL2 even when a program only calls the collision helpers.

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
