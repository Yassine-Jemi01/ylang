#include "ylang/gui.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct YlguiWindow {
    SDL_Window *window;
    SDL_Renderer *renderer;
    Uint32 window_id;
    bool open;
};

static bool gui_initialized;
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
            window->open = false;
        } else if (event.type == SDL_WINDOWEVENT &&
                   event.window.windowID == window->window_id &&
                   event.window.event == SDL_WINDOWEVENT_CLOSE) {
            window->open = false;
        }
    }
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
    return true;
}

bool ylgui_present(YlguiWindow *window)
{
    if (window == NULL || window->renderer == NULL) {
        set_error("Cannot present an invalid window");
        return false;
    }
    SDL_RenderPresent(window->renderer);
    return true;
}

void ylgui_window_destroy(YlguiWindow *window)
{
    if (window == NULL) return;
    if (window->renderer != NULL) SDL_DestroyRenderer(window->renderer);
    if (window->window != NULL) SDL_DestroyWindow(window->window);
    free(window);
}

void ylgui_shutdown(void)
{
    if (!gui_initialized) return;
    SDL_Quit();
    gui_initialized = false;
}
