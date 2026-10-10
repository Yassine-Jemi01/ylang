#ifndef YLANG_GUI_H
#define YLANG_GUI_H

/*
 * YLang GUI runtime — SDL2-backed, optional C API.
 *
 * Build with `make gui` after installing SDL2 development files.
 * This runtime is currently a C API; direct .yl bindings require a future
 * foreign-function interface in the YLang language/compiler.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct YlguiWindow YlguiWindow;

/* Initialize SDL's video subsystem. Returns false and records an error on failure. */
bool ylgui_init(void);

/* Create one window. Width and height must be positive. */
YlguiWindow *ylgui_window_create(const char *title, int width, int height);

/*
 * Process pending events for this window.
 * Returns true while the window remains open; false after a close event
 * or when called with a null/invalid window.
 */
bool ylgui_window_poll(YlguiWindow *window);

/* Set the renderer clear color and clear the current frame. */
bool ylgui_clear(YlguiWindow *window, uint8_t red, uint8_t green, uint8_t blue);

/* Draw a filled rectangle. Returns false for invalid dimensions or renderer errors. */
bool ylgui_draw_rect(YlguiWindow *window, int x, int y, int width, int height,
                     uint8_t red, uint8_t green, uint8_t blue);

/* Present the frame to the window. */
bool ylgui_present(YlguiWindow *window);

/* Destroy a window and its renderer. Safe to call with null. */
void ylgui_window_destroy(YlguiWindow *window);

/* Shut down SDL. Destroy windows before calling this. */
void ylgui_shutdown(void);

/* Human-readable message for the most recent failure; never returns null. */
const char *ylgui_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
