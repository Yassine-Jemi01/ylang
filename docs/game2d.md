# YLang 2D helpers (experimental)

The first `game2d.yl` module is a small SDL2-backed starting point, not a complete Pygame replacement. Add the standard-library directory to `YLANG_PATH` when compiling a project:

```sh
YLANG_PATH=stdlib ./build/ylang build examples/game2d.yl -o build/game2d
```

## Current API

| Function | Purpose |
| --- | --- |
| `game2d_open(title, width, height)` | Initialize SDL2 and create one window. |
| `game2d_begin_frame(r, g, b)` | Poll window events and clear the frame. Returns false when the window closes or clearing fails. |
| `game2d_draw_rect(x, y, width, height, r, g, b)` | Draw a filled rectangle. |
| `game2d_end_frame()` | Present the rendered frame. |
| `game2d_last_error()` | Read the most recent GUI error message. |
| `game2d_close()` | Close the window and shut down the GUI runtime. |
| `game2d_rects_overlap(...)` | Test axis-aligned rectangle overlap; edge-touching rectangles do not overlap. |
| `game2d_point_in_rect(...)` | Test whether a point lies inside a rectangle; right/bottom edges are exclusive. |

The module requires SDL2 development files when linked because it wraps `gui.*`. The collision helpers are deterministic and do not open a window, but programs importing this combined module still link the GUI runtime.

## Not implemented yet

Keyboard/mouse state, delta time, sprites/textures, audio, a built-in game loop, physics, and a package manifest are future work. The API is experimental; use `gui.last_error()` through `game2d_last_error()` when a drawing/window operation fails.
