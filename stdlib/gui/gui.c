#include "ylang/gui.h"

#include <SDL.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

struct YlguiWindow {
    SDL_Window *window;
    SDL_Renderer *renderer;
    Uint32 window_id;
    bool open;
    struct YlguiWindow *next;
};

static bool gui_initialized;
static YlguiWindow *gui_windows;
static char gui_error[256];

static void set_error(const char *message)
{
    if (message == NULL) message = "Unknown GUI runtime error";
    (void)snprintf(gui_error, sizeof(gui_error), "%s", message);
}

static void set_sdl_error(const char *operation)
{
    const char *detail = SDL_GetError();
    (void)snprintf(gui_error, sizeof(gui_error), "%s: %s",
                   operation, detail != NULL ? detail : "SDL reported no details");
}

const char *ylgui_last_error(void)
{
    return gui_error[0] != '\0' ? gui_error : "No error";
}

bool ylgui_init(void)
{
    if (gui_initialized) return true;
    gui_error[0] = '\0';
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        set_sdl_error("SDL video initialization failed");
        return false;
    }
    gui_initialized = true;
    return true;
}

YlguiWindow *ylgui_window_create(const char *title, int width, int height)
{
    if (!gui_initialized) {
        set_error("Call ylgui_init() before creating a window");
        return NULL;
    }
    if (title == NULL || width <= 0 || height <= 0) {
        set_error("Window title must not be null and dimensions must be positive");
        return NULL;
    }

    YlguiWindow *result = calloc(1, sizeof(*result));
    if (result == NULL) {
        set_error("Out of memory while creating a window");
        return NULL;
    }

    result->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
                                      SDL_WINDOWPOS_CENTERED, width, height,
                                      SDL_WINDOW_SHOWN);
    if (result->window == NULL) {
        set_sdl_error("Could not create window");
        free(result);
        return NULL;
    }

    result->renderer = SDL_CreateRenderer(result->window, -1,
                                           SDL_RENDERER_ACCELERATED |
                                           SDL_RENDERER_PRESENTVSYNC);
    if (result->renderer == NULL) {
        /* Software rendering is useful on virtual machines and older devices. */
        result->renderer = SDL_CreateRenderer(result->window, -1,
                                               SDL_RENDERER_SOFTWARE);
    }
    if (result->renderer == NULL) {
        set_sdl_error("Could not create renderer");
        SDL_DestroyWindow(result->window);
        free(result);
        return NULL;
    }

    result->window_id = SDL_GetWindowID(result->window);
    result->open = true;
    result->next = gui_windows;
    gui_windows = result;
    gui_error[0] = '\0';
    return result;
}

bool ylgui_window_poll(YlguiWindow *window)
{
    if (window == NULL || window->window == NULL || !gui_initialized) {
        set_error("Cannot poll an invalid or destroyed window");
        return false;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
            for (YlguiWindow *at = gui_windows; at != NULL; at = at->next) {
                at->open = false;
            }
        } else if (event.type == SDL_WINDOWEVENT &&
                   event.window.event == SDL_WINDOWEVENT_CLOSE) {
            for (YlguiWindow *at = gui_windows; at != NULL; at = at->next) {
                if (at->window_id == event.window.windowID) {
                    at->open = false;
                    break;
                }
            }
        }
    }
    gui_error[0] = '\0';
    return window->open;
}

bool ylgui_clear(YlguiWindow *window, uint8_t red, uint8_t green, uint8_t blue)
{
    if (window == NULL || window->renderer == NULL) {
        set_error("Cannot clear an invalid window");
        return false;
    }
    if (SDL_SetRenderDrawColor(window->renderer, red, green, blue, 255) != 0 ||
        SDL_RenderClear(window->renderer) != 0) {
        set_sdl_error("Could not clear window");
        return false;
    }
    gui_error[0] = '\0';
    return true;
}

bool ylgui_draw_rect(YlguiWindow *window, int x, int y, int width, int height,
                     uint8_t red, uint8_t green, uint8_t blue)
{
    if (window == NULL || window->renderer == NULL ||
        width <= 0 || height <= 0) {
        set_error("Rectangle requires a valid window and positive dimensions");
        return false;
    }

    SDL_Rect rectangle = { x, y, width, height };
    if (SDL_SetRenderDrawColor(window->renderer, red, green, blue, 255) != 0 ||
        SDL_RenderFillRect(window->renderer, &rectangle) != 0) {
        set_sdl_error("Could not draw rectangle");
        return false;
    }
    gui_error[0] = '\0';
    return true;
}

bool ylgui_present(YlguiWindow *window)
{
    if (window == NULL || window->renderer == NULL) {
        set_error("Cannot present an invalid window");
        return false;
    }
    SDL_RenderPresent(window->renderer);
    gui_error[0] = '\0';
    return true;
}

void ylgui_window_destroy(YlguiWindow *window)
{
    if (window == NULL) return;
    YlguiWindow **link = &gui_windows;
    while (*link != NULL && *link != window) link = &(*link)->next;
    if (*link == window) *link = window->next;
    if (window->renderer != NULL) SDL_DestroyRenderer(window->renderer);
    if (window->window != NULL) SDL_DestroyWindow(window->window);
    free(window);
}

void ylgui_shutdown(void)
{
    if (!gui_initialized) return;

    /* Destroy owned windows and renderers before tearing SDL down. */
    while (gui_windows != NULL) {
        ylgui_window_destroy(gui_windows);
    }
    SDL_Quit();
    gui_initialized = false;
    gui_error[0] = '\0';
}

/* Narrow adapter used by the YLang C backend; keeps raw pointers out of .yl. */
static YlguiWindow *ylgui_active_window;

static bool ylgui_api_channel(int64_t value, uint8_t *out)
{
    if (value < 0 || value > 255) {
        set_error("Color channels must be between 0 and 255");
        return false;
    }
    *out = (uint8_t)value;
    return true;
}

bool ylgui_api_init(void)
{
    return ylgui_init();
}

bool ylgui_api_create_window(const char *title, int64_t width, int64_t height)
{
    if (width <= 0 || height <= 0 || width > INT_MAX || height > INT_MAX) {
        set_error("Window dimensions must be positive and fit in int");
        return false;
    }
    if (ylgui_active_window != NULL) {
        set_error("Only one active YLang GUI window is supported");
        return false;
    }
    ylgui_active_window = ylgui_window_create(title, (int)width, (int)height);
    return ylgui_active_window != NULL;
}

bool ylgui_api_poll(void)
{
    return ylgui_window_poll(ylgui_active_window);
}

bool ylgui_api_clear(int64_t red, int64_t green, int64_t blue)
{
    uint8_t r, g, b;
    if (!ylgui_api_channel(red, &r) || !ylgui_api_channel(green, &g) ||
        !ylgui_api_channel(blue, &b)) return false;
    return ylgui_clear(ylgui_active_window, r, g, b);
}

bool ylgui_api_draw_rect(int64_t x, int64_t y, int64_t width, int64_t height,
                         int64_t red, int64_t green, int64_t blue)
{
    uint8_t r, g, b;
    if (x < INT_MIN || x > INT_MAX || y < INT_MIN || y > INT_MAX ||
        width <= 0 || width > INT_MAX || height <= 0 || height > INT_MAX) {
        set_error("Rectangle coordinates must fit in int and dimensions must be positive");
        return false;
    }
    if (!ylgui_api_channel(red, &r) || !ylgui_api_channel(green, &g) ||
        !ylgui_api_channel(blue, &b)) return false;
    return ylgui_draw_rect(ylgui_active_window, (int)x, (int)y,
                           (int)width, (int)height, r, g, b);
}

bool ylgui_api_present(void)
{
    return ylgui_present(ylgui_active_window);
}

const char *ylgui_api_last_error(void)
{
    return ylgui_last_error();
}

void ylgui_api_close(void)
{
    ylgui_window_destroy(ylgui_active_window);
    ylgui_active_window = NULL;
}

void ylgui_api_shutdown(void)
{
    ylgui_api_close();
    ylgui_shutdown();
}
