#include "ylang/gui.h"

#include <stdio.h>

int main(void)
{
    if (!ylgui_init()) {
        fprintf(stderr, "YLang GUI: %s\n", ylgui_last_error());
        return 1;
    }

    YlguiWindow *window = ylgui_window_create("Hello from YLang GUI", 800, 500);
    if (window == NULL) {
        fprintf(stderr, "YLang GUI: %s\n", ylgui_last_error());
        ylgui_shutdown();
        return 1;
    }

    while (ylgui_window_poll(window)) {
        if (!ylgui_clear(window, 24, 28, 38) ||
            !ylgui_draw_rect(window, 80, 80, 240, 120, 22, 160, 180) ||
            !ylgui_present(window)) {
            fprintf(stderr, "YLang GUI: %s\n", ylgui_last_error());
            ylgui_window_destroy(window);
            ylgui_shutdown();
            return 1;
        }
    }

    ylgui_window_destroy(window);
    ylgui_shutdown();
    return 0;
}
