#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static void stop_child(PROCESS_INFORMATION *p)
{
    if (p->hProcess) {
        (void)TerminateProcess(p->hProcess, 99U);
        (void)WaitForSingleObject(p->hProcess, 2000U);
    }
}

static int fail(const char *message, PROCESS_INFORMATION *p, HANDLE input, HANDLE output)
{
    fprintf(stderr, "Windows TUI integration failed: %s (error=%lu)\n",
            message, (unsigned long)GetLastError());
    stop_child(p);
    if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
    if (output != INVALID_HANDLE_VALUE) CloseHandle(output);
    (void)FreeConsole();
    if (p->hThread) CloseHandle(p->hThread);
    if (p->hProcess) CloseHandle(p->hProcess);
    return 1;
}

static bool console_contains(HANDLE output, const wchar_t *needle)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(output, &info)) return false;
    DWORD cells = (DWORD)info.dwSize.X * (DWORD)info.dwSize.Y;
    if (cells == 0U || cells > 1000000U) return false;
    wchar_t *buffer = (wchar_t *)calloc((size_t)cells + 1U, sizeof(wchar_t));
    if (!buffer) return false;
    COORD origin = { 0, 0 };
    DWORD count = 0U;
    bool okay = ReadConsoleOutputCharacterW(output, buffer, cells, origin, &count) != 0;
    bool found = okay && wcsstr(buffer, needle) != NULL;
    free(buffer);
    return found;
}

static bool send_key(HANDLE input, wchar_t character)
{
    SHORT mapped = character == L'\r' ? (SHORT)VK_RETURN : VkKeyScanW(character);
    if (mapped == -1) return false;
    WORD virtual_key = (WORD)(mapped & 0xff);
    INPUT_RECORD event;
    ZeroMemory(&event, sizeof(event));
    event.EventType = KEY_EVENT;
    event.Event.KeyEvent.bKeyDown = TRUE;
    event.Event.KeyEvent.wRepeatCount = 1U;
    event.Event.KeyEvent.wVirtualKeyCode = virtual_key;
    event.Event.KeyEvent.wVirtualScanCode = (WORD)MapVirtualKeyW(virtual_key, MAPVK_VK_TO_VSC);
    event.Event.KeyEvent.uChar.UnicodeChar = character;
    DWORD written = 0U;
    return WriteConsoleInputW(input, &event, 1U, &written) != 0 && written == 1U;
}

static bool send_line(HANDLE input, const wchar_t *line)
{
    while (*line) {
        if (!send_key(input, *line++)) return false;
    }
    return send_key(input, L'\r');
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    char command[32768];
    int length = snprintf(command, sizeof(command), "\"%s\"", argv[1]);
    if (length < 0 || (size_t)length >= sizeof(command)) return 2;

    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    ZeroMemory(&startup, sizeof(startup));
    ZeroMemory(&process, sizeof(process));
    startup.cb = sizeof(startup);
    if (!CreateProcessA(NULL, command, NULL, NULL, FALSE, CREATE_NEW_CONSOLE,
                        NULL, NULL, &startup, &process)) {
        fprintf(stderr, "CreateProcess failed: %lu\n", (unsigned long)GetLastError());
        return 1;
    }

    if (!FreeConsole() && GetLastError() != ERROR_INVALID_HANDLE)
        return fail("could not detach from runner console", &process,
                    INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE);

    bool attached = false;
    for (unsigned i = 0U; i < 200U; i++) {
        if (AttachConsole(process.dwProcessId)) { attached = true; break; }
        Sleep(25U);
    }
    if (!attached) return fail("could not attach to child console", &process,
                               INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE);

    HANDLE input = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0U, NULL);
    HANDLE output = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0U, NULL);
    if (input == INVALID_HANDLE_VALUE || output == INVALID_HANDLE_VALUE)
        return fail("could not open child console handles", &process, input, output);

    DWORD saved_input = 0U, saved_output = 0U;
    if (!GetConsoleMode(input, &saved_input) || !GetConsoleMode(output, &saved_output))
        return fail("could not read original console modes", &process, input, output);

    bool booted = false;
    for (unsigned i = 0U; i < 300U; i++) {
        if (WaitForSingleObject(process.hProcess, 10U) == WAIT_OBJECT_0) break;
        if (console_contains(output, L"BOOT")) { booted = true; break; }
    }
    if (!booted) return fail("child did not reach its pre-TUI input gate", &process, input, output);
    if (!send_line(input, L"go"))
        return fail("could not release the child input gate", &process, input, output);

    bool ready = false;
    for (unsigned i = 0U; i < 300U; i++) {
        if (WaitForSingleObject(process.hProcess, 10U) == WAIT_OBJECT_0) break;
        DWORD in_mode = 0U, out_mode = 0U;
        CONSOLE_CURSOR_INFO cursor;
        if (!GetConsoleMode(input, &in_mode) || !GetConsoleMode(output, &out_mode) ||
            !GetConsoleCursorInfo(output, &cursor)) continue;
        bool raw = (in_mode & (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT)) == 0U;
        bool ansi = (out_mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0U;
        if (raw && ansi && !cursor.bVisible && console_contains(output, L"READY")) {
            ready = true;
            break;
        }
    }
    if (!ready) return fail("raw input, VT output, or cursor hiding was not observed",
                            &process, input, output);

    if (!send_key(input, L'x'))
        return fail("could not send a key to raw console input", &process, input, output);
    if (WaitForSingleObject(process.hProcess, 5000U) != WAIT_OBJECT_0)
        return fail("TUI did not return after receiving a key", &process, input, output);

    DWORD exit_code = 1U;
    if (!GetExitCodeProcess(process.hProcess, &exit_code) || exit_code != 0U)
        return fail("TUI did not receive the expected key code", &process, input, output);

    DWORD restored_input = 0U, restored_output = 0U;
    if (!GetConsoleMode(input, &restored_input) || !GetConsoleMode(output, &restored_output))
        return fail("could not read restored console modes", &process, input, output);
    if (restored_input != saved_input || restored_output != saved_output) {
        fprintf(stderr, "console modes: input 0x%08lx -> 0x%08lx; output 0x%08lx -> 0x%08lx\n",
                (unsigned long)saved_input, (unsigned long)restored_input,
                (unsigned long)saved_output, (unsigned long)restored_output);
        return fail("console modes were not restored exactly", &process, input, output);
    }
    CONSOLE_CURSOR_INFO cursor;
    if (!GetConsoleCursorInfo(output, &cursor) || !cursor.bVisible)
        return fail("cursor was not restored automatically during shutdown", &process, input, output);

    CloseHandle(input);
    CloseHandle(output);
    (void)FreeConsole();
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    puts("PASS: Windows TUI raw input, ANSI output, cursor restoration, and console-mode restoration.");
    return 0;
}
