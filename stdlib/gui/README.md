# YLang GUI runtime

The optional GUI runtime is a small SDL2-backed C library.

## API

- `ylgui_init()` / `ylgui_shutdown()` — initialize and release SDL.
- `ylgui_window_create(title, width, height)` — create a window.
- `ylgui_window_poll(window)` — process events and report whether it remains open.
- `ylgui_clear(window, r, g, b)` — clear the frame.
- `ylgui_draw_rect(window, x, y, width, height, r, g, b)` — draw a filled rectangle.
- `ylgui_present(window)` — present the frame.
- `ylgui_window_destroy(window)` — release the window and renderer.
- `ylgui_last_error()` — retrieve a readable error message.

See [include/ylang/gui.h](../../include/ylang/gui.h) and [examples/gui.c](../../examples/gui.c).

## Build

Install SDL2 development headers and `pkg-config`, then run:

```sh
make gui gui-example
./build/gui-example
```

The runtime is an optional target so users who do not need GUI support do not need SDL2. This is currently a C API, not yet a directly callable YLang standard-library module: direct `.yl` bindings require the language's foreign-function interface and are explicitly deferred.
