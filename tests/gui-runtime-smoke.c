#include "ylang/gui.h"

#include <SDL.h>
#include <stdio.h>

int main(void)
{
    if (!ylgui_init()) {
        fprintf(stderr, "init: %s\n", ylgui_last_error());
        return 1;
    }

    YlguiWindow *window = ylgui_window_create("YLang headless GUI smoke", 64, 64);
    if (window == NULL) {
        fprintf(stderr, "create: %s\n", ylgui_last_error());
        ylgui_shutdown();
        return 1;
    }

    if (!ylgui_clear(window, 12, 24, 36) ||
        !ylgui_draw_rect(window, 4, 4, 16, 16, 200, 100, 50) ||
        !ylgui_present(window)) {
        fprintf(stderr, "render: %s\n", ylgui_last_error());
        ylgui_window_destroy(window);
        ylgui_shutdown();
        return 1;
    }

    SDL_Event close_event = {0};
    close_event.type = SDL_QUIT;
    if (SDL_PushEvent(&close_event) < 0) {
        fprintf(stderr, "event: %s\n", SDL_GetError());
        ylgui_window_destroy(window);
        ylgui_shutdown();
        return 1;
    }
    if (ylgui_window_poll(window)) {
        fputs("close event did not stop the window loop\n", stderr);
        ylgui_window_destroy(window);
        ylgui_shutdown();
        return 1;
    }

    ylgui_window_destroy(window);
    ylgui_shutdown();
    puts("YLang GUI runtime smoke test passed.");
    return 0;
}
