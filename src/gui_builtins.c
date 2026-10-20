#include "internal.h"

#include <string.h>

const GuiBuiltinInfo ylang_gui_builtins[] = {
    {"gui.init", 0U, TYPE_BOOL, {TYPE_VOID}, "ylgui_api_init"},
    {"gui.create_window", 3U, TYPE_BOOL,
     {TYPE_STRING, TYPE_INT, TYPE_INT}, "ylgui_api_create_window"},
    {"gui.poll", 0U, TYPE_BOOL, {TYPE_VOID}, "ylgui_api_poll"},
    {"gui.key_down", 1U, TYPE_BOOL, {TYPE_STRING}, "ylgui_api_key_down"},
    {"gui.mouse_x", 0U, TYPE_INT, {TYPE_VOID}, "ylgui_api_mouse_x"},
    {"gui.mouse_y", 0U, TYPE_INT, {TYPE_VOID}, "ylgui_api_mouse_y"},
    {"gui.mouse_button_down", 1U, TYPE_BOOL, {TYPE_INT},
     "ylgui_api_mouse_button_down"},
    {"gui.ticks_ms", 0U, TYPE_INT, {TYPE_VOID}, "ylgui_api_ticks_ms"},
    {"gui.clear", 3U, TYPE_BOOL, {TYPE_INT, TYPE_INT, TYPE_INT},
     "ylgui_api_clear"},
    {"gui.draw_rect", 7U, TYPE_BOOL,
     {TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT},
     "ylgui_api_draw_rect"},
    {"gui.present", 0U, TYPE_BOOL, {TYPE_VOID}, "ylgui_api_present"},
    {"gui.last_error", 0U, TYPE_STRING, {TYPE_VOID}, "ylgui_api_last_error"},
    {"gui.close", 0U, TYPE_VOID, {TYPE_VOID}, "ylgui_api_close"},
    {"gui.shutdown", 0U, TYPE_VOID, {TYPE_VOID}, "ylgui_api_shutdown"}
};

const size_t ylang_gui_builtin_count =
    sizeof(ylang_gui_builtins) / sizeof(ylang_gui_builtins[0]);

const GuiBuiltinInfo *find_gui_builtin_info(const char *name)
{
    if (!name) return NULL;
    for (size_t i = 0; i < ylang_gui_builtin_count; i++) {
        if (strcmp(name, ylang_gui_builtins[i].name) == 0) {
            return &ylang_gui_builtins[i];
        }
    }
    return NULL;
}
