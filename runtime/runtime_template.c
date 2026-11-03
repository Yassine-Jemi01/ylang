#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <math.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef YLANG_USE_HTTP
#include <curl/curl.h>
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

bool ylgui_api_init(void);
#ifdef YLANG_USE_VOICE
bool ylvoice_init(void);
bool ylvoice_play(const char *);
void ylvoice_pause(void);
void ylvoice_resume(void);
void ylvoice_stop(void);
bool ylvoice_is_playing(void);
bool ylvoice_set_volume(int64_t);
const char *ylvoice_last_error(void);
void ylvoice_shutdown(void);
#endif
bool ylgui_api_create_window(const char *, int64_t, int64_t);
bool ylgui_api_poll(void);
bool ylgui_api_key_down(const char *);
int64_t ylgui_api_mouse_x(void);
int64_t ylgui_api_mouse_y(void);
bool ylgui_api_mouse_button_down(int64_t);
int64_t ylgui_api_ticks_ms(void);
bool ylgui_api_clear(int64_t, int64_t, int64_t);
bool ylgui_api_draw_rect(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
bool ylgui_api_present(void);
const char *ylgui_api_last_error(void);
void ylgui_api_close(void);
void ylgui_api_shutdown(void);

static void yl_runtime_error(const char *message) {
    fprintf(stderr, "YLang runtime error: %s\n", message);
    exit(70);
}
typedef struct YLTracked {
    void *ptr;
    size_t size;
    struct YLTracked *next;
} YLTracked;
static YLTracked *yl_tracked = NULL;
static size_t yl_tracked_allocations = 0U;
static size_t yl_tracked_bytes = 0U;
static int yl_memory_stats_state = -1;

static bool yl_memory_stats_enabled(void) {
    if (yl_memory_stats_state < 0) {
        const char *value = getenv("YLANG_MEMORY_STATS");
        yl_memory_stats_state = value && strcmp(value, "1") == 0 ? 1 : 0;
    }
    return yl_memory_stats_state == 1;
}

static size_t yl_saturating_add_size(size_t left, size_t right) {
    return SIZE_MAX - left < right ? SIZE_MAX : left + right;
}

static void yl_binary_stream_shutdown(void);
static void yl_binary_buffer_shutdown(void);

static void yl_cleanup(void) {
#ifdef YLANG_USE_VOICE
    ylvoice_shutdown();
#endif
    yl_binary_stream_shutdown();
    yl_binary_buffer_shutdown();
    if (yl_memory_stats_enabled()) {
        fprintf(stderr,
                "YLang memory stats: tracked_allocations=%zu tracked_bytes=%zu\n",
                yl_tracked_allocations, yl_tracked_bytes);
    }
    while (yl_tracked) {
        YLTracked *next = yl_tracked->next;
        free(yl_tracked->ptr);
        free(yl_tracked);
        yl_tracked = next;
    }
}
static void *yl_track(void *ptr, size_t size) {
    YLTracked *node = malloc(sizeof(*node));
    if (!node) yl_runtime_error("out of memory");
    node->ptr = ptr;
    node->size = size;
    node->next = yl_tracked;
    yl_tracked = node;
    if (yl_memory_stats_enabled()) {
        yl_tracked_allocations = yl_saturating_add_size(yl_tracked_allocations, 1U);
        yl_tracked_bytes = yl_saturating_add_size(yl_tracked_bytes, size);
    }
    return ptr;
}
static void yl_release(const char *value) {
    if (!value) return;
    YLTracked **link = &yl_tracked;
    while (*link) {
        YLTracked *node = *link;
        if (node->ptr == (const void *)value) {
            *link = node->next;
            if (yl_memory_stats_enabled()) {
                if (yl_tracked_allocations > 0U) yl_tracked_allocations--;
                if (yl_tracked_bytes >= node->size) yl_tracked_bytes -= node->size;
                else yl_tracked_bytes = 0U;
            }
            free(node->ptr);
            free(node);
            return;
        }
        link = &node->next;
    }
}

static const char *yl_move(const char **slot) {
    if (!slot) yl_runtime_error("invalid owned string move");
    const char *value = *slot;
    *slot = NULL;
    return value;
}

static const char *yl_string_own(const char *value) {
    if (!value) yl_runtime_error("cannot clone a null string");
    size_t length = strlen(value);
    if (length == SIZE_MAX) yl_runtime_error("owned string is too large");
    char *copy = malloc(length + 1U);
    if (!copy) yl_runtime_error("out of memory");
    memcpy(copy, value, length + 1U);
    return (const char *)yl_track(copy, length + 1U);
}

#ifdef YLANG_USE_HTTP
#define YL_HTTP_MAX_RESPONSE (8U * 1024U * 1024U)
#define YL_HTTP_MAX_HEADERS (16U * 1024U)
typedef struct { char *data; size_t length; size_t capacity; bool too_large; } YLHttpBuffer;
static char yl_http_error[256];
static int64_t yl_http_last_status_code;
static bool yl_http_initialized;
static char yl_http_response_header_text[YL_HTTP_MAX_HEADERS + 1U];
static size_t yl_http_response_header_length;
static bool yl_http_response_headers_too_large;
static void yl_http_set_error(const char *message) {
    if (!message) message = "unknown HTTP error";
    (void)snprintf(yl_http_error, sizeof(yl_http_error), "%s", message);
}
static size_t yl_http_write(char *data, size_t size, size_t count, void *userdata) {
    YLHttpBuffer *buffer = (YLHttpBuffer *)userdata;
    if (size && count > SIZE_MAX / size) return 0U;
    size_t bytes = size * count;
    if (buffer->length > YL_HTTP_MAX_RESPONSE ||
        bytes > YL_HTTP_MAX_RESPONSE - buffer->length ||
        bytes > SIZE_MAX - buffer->length - 1U) {
        buffer->too_large = true; return 0U;
    }
    size_t needed = buffer->length + bytes + 1U;
    if (needed > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 4096U;
        while (capacity < needed) {
            if (capacity > (YL_HTTP_MAX_RESPONSE + 1U) / 2U) {
                capacity = YL_HTTP_MAX_RESPONSE + 1U; break;
            }
            capacity *= 2U;
        }
        char *grown = realloc(buffer->data, capacity);
        if (!grown) return 0U;
        buffer->data = grown; buffer->capacity = capacity;
    }
    if (bytes) memcpy(buffer->data + buffer->length, data, bytes);
    buffer->length += bytes; buffer->data[buffer->length] = '\0';
    return bytes;
}
static size_t yl_http_header_write(char *data, size_t size, size_t count, void *userdata) {
    (void)userdata;
    if (size && count > SIZE_MAX / size) return 0U;
    size_t bytes = size * count;
    /* A redirect or interim status starts a fresh response-header block. */
    if (bytes >= 5U && memcmp(data, "HTTP/", 5U) == 0) {
        yl_http_response_header_length = 0U;
        yl_http_response_header_text[0] = '\0';
        yl_http_response_headers_too_large = false;
        return bytes;
    }
    size_t length = bytes;
    while (length > 0U && (data[length - 1U] == '\n' || data[length - 1U] == '\r')) length--;
    if (length == 0U) return bytes;
    if (length > YL_HTTP_MAX_HEADERS - yl_http_response_header_length - 1U) {
        yl_http_response_headers_too_large = true;
        return 0U;
    }
    memcpy(yl_http_response_header_text + yl_http_response_header_length, data, length);
    yl_http_response_header_length += length;
    yl_http_response_header_text[yl_http_response_header_length++] = '\n';
    yl_http_response_header_text[yl_http_response_header_length] = '\0';
    return bytes;
}

static bool yl_http_header_name_ok(const char *name, size_t length) {
    if (!length) return false;
    for (size_t i = 0; i < length; i++) {
        unsigned char ch = (unsigned char)name[i];
        bool alpha = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
        bool digit = ch >= '0' && ch <= '9';
        if (!alpha && !digit && !strchr("!#$%&*+-.^_|~", (int)ch)) return false;
    }
    return true;
}
static bool yl_http_headers(const char *raw, struct curl_slist **result) {
    *result = NULL;
    if (!raw || !*raw) return true;
    if (strlen(raw) > YL_HTTP_MAX_HEADERS) {
        yl_http_set_error("HTTP headers exceed the 16 KiB limit"); return false;
    }
    const char *line = raw;
    while (*line) {
        const char *end = strchr(line, '\n');
        size_t length = end ? (size_t)(end - line) : strlen(line);
        if (length && line[length - 1U] == '\r') {
            yl_http_set_error("Use newline-separated HTTP headers without carriage returns");
            curl_slist_free_all(*result); *result = NULL; return false;
        }
        if (!length) { if (!end) break; line = end + 1; continue; }
        const char *colon = memchr(line, ':', length);
        if (!colon || !yl_http_header_name_ok(line, (size_t)(colon - line))) {
            yl_http_set_error("Malformed HTTP header; expected Header-Name: value");
            curl_slist_free_all(*result); *result = NULL; return false;
        }
        for (size_t i = 0; i < length; i++) {
            unsigned char ch = (unsigned char)line[i];
            if ((ch < 32U && ch != '\t') || ch == 127U) {
                yl_http_set_error("HTTP headers cannot contain control characters");
                curl_slist_free_all(*result); *result = NULL; return false;
            }
        }
        char *copy = malloc(length + 1U);
        if (!copy) { yl_http_set_error("Out of memory preparing HTTP headers"); curl_slist_free_all(*result); *result = NULL; return false; }
        memcpy(copy, line, length); copy[length] = '\0';
        struct curl_slist *grown = curl_slist_append(*result, copy); free(copy);
        if (!grown) { yl_http_set_error("Out of memory preparing HTTP headers"); curl_slist_free_all(*result); *result = NULL; return false; }
        *result = grown;
        if (!end) break; line = end + 1;
    }
    return true;
}
static bool yl_http_redirects_allowed(const struct curl_slist *headers) {
    for (const struct curl_slist *item = headers; item; item = item->next) {
        if (!item->data || strncmp(item->data, "Content-Type:", 13U) != 0) return false;
    }
    return true;
}

static const char *yl_http_empty(void) {
    char *value = malloc(1U); if (!value) yl_runtime_error("out of memory");
    value[0] = '\0'; return (const char *)yl_track(value, 1U);
}
static const char *yl_http_request(const char *method, const char *url, const char *body, const char *raw_headers) {
    yl_http_error[0] = '\0'; yl_http_last_status_code = 0;
    yl_http_response_header_length = 0U;
    yl_http_response_header_text[0] = '\0';
    yl_http_response_headers_too_large = false;
    bool supported_method = method &&
        (strcmp(method, "GET") == 0 || strcmp(method, "POST") == 0 ||
         strcmp(method, "PUT") == 0 || strcmp(method, "PATCH") == 0 ||
         strcmp(method, "DELETE") == 0 || strcmp(method, "HEAD") == 0);
    if (!supported_method) {
        yl_http_set_error("Unsupported HTTP method; use GET, POST, PUT, PATCH, DELETE, or HEAD");
        return yl_http_empty();
    }
    if (!url || (strncmp(url, "http://", 7U) != 0 && strncmp(url, "https://", 8U) != 0)) {
        yl_http_set_error("URL must begin with http:// or https://"); return yl_http_empty();
    }
    if (!yl_http_initialized) {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
            yl_http_set_error("Could not initialize HTTP runtime"); return yl_http_empty();
        }
        yl_http_initialized = true;
    }
    struct curl_slist *headers = NULL;
    if (!yl_http_headers(raw_headers, &headers)) return yl_http_empty();
    CURL *curl = curl_easy_init();
    if (!curl) { curl_slist_free_all(headers); yl_http_set_error("Could not create HTTP request"); return yl_http_empty(); }
    YLHttpBuffer response = {0}; char error_buffer[CURL_ERROR_SIZE] = {0}; CURLcode code = CURLE_OK;
#define YL_HTTP_OPTION(option, value) do { if (code == CURLE_OK) code = curl_easy_setopt(curl, option, value); } while (0)
    YL_HTTP_OPTION(CURLOPT_URL, url);
    YL_HTTP_OPTION(CURLOPT_ERRORBUFFER, error_buffer);
    YL_HTTP_OPTION(CURLOPT_WRITEFUNCTION, yl_http_write);
    YL_HTTP_OPTION(CURLOPT_WRITEDATA, &response);
    YL_HTTP_OPTION(CURLOPT_HEADERFUNCTION, yl_http_header_write);
    YL_HTTP_OPTION(CURLOPT_HEADERDATA, NULL);
    YL_HTTP_OPTION(CURLOPT_NOSIGNAL, 1L);
    YL_HTTP_OPTION(CURLOPT_CONNECTTIMEOUT_MS, 5000L);
    YL_HTTP_OPTION(CURLOPT_TIMEOUT_MS, 15000L);
    YL_HTTP_OPTION(CURLOPT_FOLLOWLOCATION, yl_http_redirects_allowed(headers) ? 1L : 0L);
    YL_HTTP_OPTION(CURLOPT_MAXREDIRS, 5L);
    YL_HTTP_OPTION(CURLOPT_SSL_VERIFYPEER, 1L);
    YL_HTTP_OPTION(CURLOPT_SSL_VERIFYHOST, 2L);
    YL_HTTP_OPTION(CURLOPT_PROTOCOLS, (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
    YL_HTTP_OPTION(CURLOPT_REDIR_PROTOCOLS, (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
    YL_HTTP_OPTION(CURLOPT_USERAGENT, "YLang/2.2");
    if (headers) YL_HTTP_OPTION(CURLOPT_HTTPHEADER, headers);
    if (strcmp(method, "GET") == 0) {
        YL_HTTP_OPTION(CURLOPT_HTTPGET, 1L);
    } else if (strcmp(method, "HEAD") == 0) {
        YL_HTTP_OPTION(CURLOPT_CUSTOMREQUEST, "HEAD");
        YL_HTTP_OPTION(CURLOPT_NOBODY, 1L);
    } else if (strcmp(method, "POST") == 0) {
        YL_HTTP_OPTION(CURLOPT_POST, 1L);
        YL_HTTP_OPTION(CURLOPT_POSTFIELDS, body ? body : "");
        YL_HTTP_OPTION(CURLOPT_POSTFIELDSIZE, (long)(body ? strlen(body) : 0U));
    } else {
        YL_HTTP_OPTION(CURLOPT_CUSTOMREQUEST, method);
        YL_HTTP_OPTION(CURLOPT_POSTFIELDS, body ? body : "");
        YL_HTTP_OPTION(CURLOPT_POSTFIELDSIZE, (long)(body ? strlen(body) : 0U));
    }
#undef YL_HTTP_OPTION
    if (code == CURLE_OK) code = curl_easy_perform(curl);
    long response_code = 0;
    (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    yl_http_last_status_code = (int64_t)response_code;
    if (code != CURLE_OK) {
        if (yl_http_response_headers_too_large) yl_http_set_error("HTTP response headers exceeded the 16 KiB limit");
        else if (response.too_large) yl_http_set_error("HTTP response exceeded the 8 MiB limit");
        else if (error_buffer[0]) yl_http_set_error(error_buffer);
        else yl_http_set_error(curl_easy_strerror(code));
        free(response.data); response.data = NULL; response.length = 0U;
    }
    curl_easy_cleanup(curl); curl_slist_free_all(headers);
    if (!response.data) { response.data = malloc(1U); if (!response.data) yl_runtime_error("out of memory"); response.data[0] = '\0'; response.length = 0U; }
    return (const char *)yl_track(response.data, response.length + 1U);
}
static const char *yl_http_get(const char *url) { return yl_http_request("GET", url, NULL, NULL); }
static const char *yl_http_get_with_headers(const char *url, const char *headers) { return yl_http_request("GET", url, NULL, headers); }
static const char *yl_http_post(const char *url, const char *body) { return yl_http_request("POST", url, body, "Content-Type: application/json"); }
static const char *yl_http_post_with_headers(const char *url, const char *body, const char *headers) { return yl_http_request("POST", url, body, headers); }
static int64_t yl_http_status(void) { return yl_http_last_status_code; }
static const char *yl_http_response_headers(void) { return yl_http_response_header_text; }
static const char *yl_http_last_error(void) { return yl_http_error; }

typedef struct {
    int64_t id;
    bool active;
    bool done;
    bool attached;
    CURL *easy;
    struct curl_slist *headers;
    char *url;
    char *body_text;
    YLHttpBuffer body;
    char error_buffer[CURL_ERROR_SIZE];
    char error[256];
    int64_t status;
    char response_headers[YL_HTTP_MAX_HEADERS + 1U];
    size_t response_header_length;
    bool response_headers_too_large;
} YLHttpTask;
#define YL_HTTP_MAX_TASKS 32U
static YLHttpTask yl_http_tasks[YL_HTTP_MAX_TASKS];
static CURLM *yl_http_multi;
static int64_t yl_http_next_task_id = 1;

static void yl_http_task_error(YLHttpTask *task, const char *message) {
    if (task) (void)snprintf(task->error, sizeof(task->error), "%s", message ? message : "unknown HTTP error");
}
static void yl_http_task_release(YLHttpTask *task) {
    if (!task) return;
    if (task->easy) {
        if (task->attached && yl_http_multi) (void)curl_multi_remove_handle(yl_http_multi, task->easy);
        curl_easy_cleanup(task->easy);
    }
    curl_slist_free_all(task->headers);
    free(task->url);
    free(task->body_text);
    free(task->body.data);
    memset(task, 0, sizeof(*task));
}
static void yl_http_async_shutdown(void) {
    for (size_t i = 0; i < YL_HTTP_MAX_TASKS; i++)
        if (yl_http_tasks[i].active) yl_http_task_release(&yl_http_tasks[i]);
    if (yl_http_multi) { curl_multi_cleanup(yl_http_multi); yl_http_multi = NULL; }
    curl_global_cleanup();
}
static YLHttpTask *yl_http_task_find(int64_t id) {
    for (size_t i = 0; i < YL_HTTP_MAX_TASKS; i++)
        if (yl_http_tasks[i].active && yl_http_tasks[i].id == id) return &yl_http_tasks[i];
    return NULL;
}
static size_t yl_http_task_header_callback(char *data, size_t size, size_t count, void *userdata) {
    YLHttpTask *task = (YLHttpTask *)userdata;
    if (!task || (size && count > SIZE_MAX / size)) return 0U;
    size_t bytes = size * count;
    if (bytes >= 5U && memcmp(data, "HTTP/", 5U) == 0) {
        task->response_header_length = 0U;
        task->response_headers[0] = '\0';
        task->response_headers_too_large = false;
        return bytes;
    }
    size_t length = bytes;
    while (length && (data[length - 1U] == '\r' || data[length - 1U] == '\n')) length--;
    if (!length) return bytes;
    if (task->response_header_length >= YL_HTTP_MAX_HEADERS ||
        length > YL_HTTP_MAX_HEADERS - task->response_header_length - 1U) {
        task->response_headers_too_large = true;
        return 0U;
    }
    memcpy(task->response_headers + task->response_header_length, data, length);
    task->response_header_length += length;
    task->response_headers[task->response_header_length++] = '\n';
    task->response_headers[task->response_header_length] = '\0';
    return bytes;
}
static void yl_http_task_poll_completions(void) {
    int remaining = 0;
    CURLMsg *message = NULL;
    while ((message = curl_multi_info_read(yl_http_multi, &remaining)) != NULL) {
        if (message->msg != CURLMSG_DONE) continue;
        YLHttpTask *task = NULL;
        (void)curl_easy_getinfo(message->easy_handle, CURLINFO_PRIVATE, &task);
        if (!task || !task->active) {
            (void)curl_multi_remove_handle(yl_http_multi, message->easy_handle);
            continue;
        }
        long response_code = 0;
        (void)curl_easy_getinfo(message->easy_handle, CURLINFO_RESPONSE_CODE, &response_code);
        task->status = (int64_t)response_code;
        (void)curl_multi_remove_handle(yl_http_multi, message->easy_handle);
        task->attached = false;
        if (message->data.result != CURLE_OK) {
            if (task->response_headers_too_large) yl_http_task_error(task, "HTTP response headers exceeded the 16 KiB limit");
            else if (task->body.too_large) yl_http_task_error(task, "HTTP response exceeded the 8 MiB limit");
            else if (task->error_buffer[0]) yl_http_task_error(task, task->error_buffer);
            else yl_http_task_error(task, curl_easy_strerror(message->data.result));
        }
        task->done = true;
    }
}
static int64_t yl_http_request_async(const char *method, const char *url, const char *body, const char *raw_headers) {
    yl_http_error[0] = '\0'; yl_http_last_status_code = 0;
    yl_http_response_header_text[0] = '\0'; yl_http_response_header_length = 0U;
    bool allowed = method &&
        (strcmp(method, "GET") == 0 || strcmp(method, "POST") == 0 ||
         strcmp(method, "PUT") == 0 || strcmp(method, "PATCH") == 0 ||
         strcmp(method, "DELETE") == 0 || strcmp(method, "HEAD") == 0);
    if (!allowed) { yl_http_set_error("Unsupported HTTP method; use GET, POST, PUT, PATCH, DELETE, or HEAD"); return 0; }
    if (!url || (strncmp(url, "http://", 7U) != 0 && strncmp(url, "https://", 8U) != 0)) {
        yl_http_set_error("URL must begin with http:// or https://"); return 0;
    }
    if (!yl_http_initialized) {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) { yl_http_set_error("Could not initialize HTTP runtime"); return 0; }
        yl_http_initialized = true;
    }
    if (!yl_http_multi) {
        yl_http_multi = curl_multi_init();
        if (!yl_http_multi) { yl_http_set_error("Could not initialize asynchronous HTTP runtime"); return 0; }
        (void)curl_multi_setopt(yl_http_multi, CURLMOPT_MAX_TOTAL_CONNECTIONS, (long)YL_HTTP_MAX_TASKS);
        (void)atexit(yl_http_async_shutdown);
    }
    YLHttpTask *task = NULL;
    for (size_t i = 0; i < YL_HTTP_MAX_TASKS; i++) if (!yl_http_tasks[i].active) { task = &yl_http_tasks[i]; break; }
    if (!task) { yl_http_set_error("Too many in-flight HTTP tasks (limit 32)"); return 0; }
    memset(task, 0, sizeof(*task));
    task->active = true; task->id = yl_http_next_task_id++;
    if (yl_http_next_task_id <= 0) yl_http_next_task_id = 1;
    size_t url_length = strlen(url), body_length = strlen(body ? body : "");
    task->url = malloc(url_length + 1U);
    task->body_text = malloc(body_length + 1U);
    if (!task->url || !task->body_text) { yl_http_task_error(task, "Out of memory preparing HTTP request"); task->done = true; return task->id; }
    memcpy(task->url, url, url_length + 1U); memcpy(task->body_text, body ? body : "", body_length + 1U);
    if (!yl_http_headers(raw_headers, &task->headers)) { yl_http_task_error(task, yl_http_error); task->done = true; return task->id; }
    task->easy = curl_easy_init();
    if (!task->easy) { yl_http_task_error(task, "Could not create HTTP request"); task->done = true; return task->id; }
    CURLcode code = CURLE_OK;
#define YL_HTTP_TASK_OPTION(option, value) do { if (code == CURLE_OK) code = curl_easy_setopt(task->easy, option, value); } while (0)
    YL_HTTP_TASK_OPTION(CURLOPT_URL, task->url);
    YL_HTTP_TASK_OPTION(CURLOPT_PRIVATE, task);
    YL_HTTP_TASK_OPTION(CURLOPT_ERRORBUFFER, task->error_buffer);
    YL_HTTP_TASK_OPTION(CURLOPT_WRITEFUNCTION, yl_http_write);
    YL_HTTP_TASK_OPTION(CURLOPT_WRITEDATA, &task->body);
    YL_HTTP_TASK_OPTION(CURLOPT_HEADERFUNCTION, yl_http_task_header_callback);
    YL_HTTP_TASK_OPTION(CURLOPT_HEADERDATA, task);
    YL_HTTP_TASK_OPTION(CURLOPT_NOSIGNAL, 1L);
    YL_HTTP_TASK_OPTION(CURLOPT_CONNECTTIMEOUT_MS, 5000L);
    YL_HTTP_TASK_OPTION(CURLOPT_TIMEOUT_MS, 15000L);
    YL_HTTP_TASK_OPTION(CURLOPT_FOLLOWLOCATION, yl_http_redirects_allowed(task->headers) ? 1L : 0L);
    YL_HTTP_TASK_OPTION(CURLOPT_MAXREDIRS, 5L);
    YL_HTTP_TASK_OPTION(CURLOPT_SSL_VERIFYPEER, 1L);
    YL_HTTP_TASK_OPTION(CURLOPT_SSL_VERIFYHOST, 2L);
    YL_HTTP_TASK_OPTION(CURLOPT_PROTOCOLS, (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
    YL_HTTP_TASK_OPTION(CURLOPT_REDIR_PROTOCOLS, (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
    YL_HTTP_TASK_OPTION(CURLOPT_USERAGENT, "YLang/2.2");
    if (task->headers) YL_HTTP_TASK_OPTION(CURLOPT_HTTPHEADER, task->headers);
    if (strcmp(method, "GET") == 0) YL_HTTP_TASK_OPTION(CURLOPT_HTTPGET, 1L);
    else if (strcmp(method, "HEAD") == 0) {
        YL_HTTP_TASK_OPTION(CURLOPT_CUSTOMREQUEST, "HEAD");
        YL_HTTP_TASK_OPTION(CURLOPT_NOBODY, 1L);
    } else if (strcmp(method, "POST") == 0) {
        YL_HTTP_TASK_OPTION(CURLOPT_POST, 1L);
        YL_HTTP_TASK_OPTION(CURLOPT_POSTFIELDS, task->body_text);
        YL_HTTP_TASK_OPTION(CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)body_length);
    } else {
        YL_HTTP_TASK_OPTION(CURLOPT_CUSTOMREQUEST, method);
        YL_HTTP_TASK_OPTION(CURLOPT_POSTFIELDS, task->body_text);
        YL_HTTP_TASK_OPTION(CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)body_length);
    }
#undef YL_HTTP_TASK_OPTION
    if (code != CURLE_OK) { yl_http_task_error(task, curl_easy_strerror(code)); task->done = true; return task->id; }
    CURLMcode multi_code = curl_multi_add_handle(yl_http_multi, task->easy);
    if (multi_code != CURLM_OK) { yl_http_task_error(task, curl_multi_strerror(multi_code)); task->done = true; return task->id; }
    task->attached = true;
    return task->id;
}
static const char *yl_http_await(int64_t id) {
    YLHttpTask *task = yl_http_task_find(id);
    if (!task) {
        yl_http_set_error("Invalid, cancelled, or already awaited HTTP task handle");
        yl_http_last_status_code = 0; yl_http_response_header_text[0] = '\0';
        return yl_http_empty();
    }
    while (!task->done && yl_http_multi) {
        int running = 0;
        CURLMcode code = curl_multi_perform(yl_http_multi, &running);
        yl_http_task_poll_completions();
        task = yl_http_task_find(id);
        if (!task) break;
        if (code != CURLM_OK) {
            for (size_t i = 0; i < YL_HTTP_MAX_TASKS; i++) {
                YLHttpTask *item = &yl_http_tasks[i];
                if (!item->active || item->done) continue;
                yl_http_task_error(item, curl_multi_strerror(code));
                if (item->easy && item->attached) { (void)curl_multi_remove_handle(yl_http_multi, item->easy); item->attached = false; }
                item->done = true;
            }
            break;
        }
        if (!task->done && running > 0) { int descriptors = 0; (void)curl_multi_poll(yl_http_multi, NULL, 0U, 1000, &descriptors); }
    }
    task = yl_http_task_find(id);
    if (!task) { yl_http_set_error("HTTP task was consumed while awaiting"); return yl_http_empty(); }
    yl_http_last_status_code = task->status;
    yl_http_response_header_length = task->response_header_length;
    memcpy(yl_http_response_header_text, task->response_headers, task->response_header_length + 1U);
    yl_http_set_error(task->error[0] ? task->error : "");
    char *result = NULL;
    if (!task->error[0] && task->body.data) {
        result = malloc(task->body.length + 1U);
        if (result) memcpy(result, task->body.data, task->body.length + 1U);
    }
    if (!result) { result = malloc(1U); if (!result) yl_runtime_error("out of memory after HTTP await"); result[0] = '\0'; }
    size_t result_length = strlen(result);
    yl_http_task_release(task);
    return (const char *)yl_track(result, result_length + 1U);
}
static bool yl_http_cancel(int64_t id) {
    YLHttpTask *task = yl_http_task_find(id);
    if (!task) { yl_http_set_error("Invalid, cancelled, or already awaited HTTP task handle"); return false; }
    yl_http_set_error("HTTP task cancelled");
    yl_http_last_status_code = 0; yl_http_response_header_text[0] = '\0';
    yl_http_task_release(task);
    return true;
}

#endif


/* Bounded JSON parser used by the std.json built-ins. */
#define YL_JSON_MAX_INPUT (16U * 1024U * 1024U)
#define YL_JSON_MAX_DEPTH 128U
static char yl_json_error_text[192];

typedef struct { const char *s; size_t n; size_t i; unsigned depth; } YLJson;
static void yl_json_error(const char *message) {
    (void)snprintf(yl_json_error_text, sizeof(yl_json_error_text), "%s", message);
}
static void yl_json_ws(YLJson *p) {
    while (p->i < p->n && (p->s[p->i] == ' ' || p->s[p->i] == '\t' ||
           p->s[p->i] == '\r' || p->s[p->i] == '\n')) p->i++;
}
static bool yl_json_hex4(const char *s, uint32_t *out) {
    uint32_t v = 0U;
    for (size_t i = 0; i < 4U; i++) {
        unsigned char c = (unsigned char)s[i];
        unsigned d;
        if (c >= '0' && c <= '9') d = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (unsigned)(c - 'a') + 10U;
        else if (c >= 'A' && c <= 'F') d = (unsigned)(c - 'A') + 10U;
        else return false;
        v = (v << 4U) | d;
    }
    *out = v; return true;
}
static bool yl_json_utf8_advance(YLJson *p, unsigned char lead) {
    unsigned need;
    unsigned char second_min = 0x80U, second_max = 0xBFU;
    if (lead >= 0xC2U && lead <= 0xDFU) need = 1U;
    else if (lead >= 0xE0U && lead <= 0xEFU) {
        need = 2U;
        if (lead == 0xE0U) second_min = 0xA0U;
        if (lead == 0xEDU) second_max = 0x9FU;
    } else if (lead >= 0xF0U && lead <= 0xF4U) {
        need = 3U;
        if (lead == 0xF0U) second_min = 0x90U;
        if (lead == 0xF4U) second_max = 0x8FU;
    } else return false;
    if (p->n - p->i < need) return false;
    unsigned char second = (unsigned char)p->s[p->i];
    if (second < second_min || second > second_max) return false;
    p->i++;
    for (unsigned j = 1U; j < need; j++) {
        unsigned char next = (unsigned char)p->s[p->i++];
        if (next < 0x80U || next > 0xBFU) return false;
    }
    return true;
}
static bool yl_json_string_end(YLJson *p, size_t *begin, size_t *end) {
    if (p->i >= p->n || p->s[p->i] != '"') return false;
    p->i++; *begin = p->i;
    while (p->i < p->n) {
        unsigned char c = (unsigned char)p->s[p->i++];
        if (c == '"') { *end = p->i - 1U; return true; }
        if (c < 0x20U) return false;
        if (c >= 0x80U && !yl_json_utf8_advance(p, c)) return false;
        if (c == '\\') {
            if (p->i >= p->n) return false;
            char e = p->s[p->i++];
            if (e == '"' || e == '\\' || e == '/' || e == 'b' ||
                e == 'f' || e == 'n' || e == 'r' || e == 't') continue;
            if (e == 'u') {
                uint32_t cp;
                if (p->n - p->i < 4U || !yl_json_hex4(p->s + p->i, &cp)) return false;
                p->i += 4U;
                if (cp >= 0xD800U && cp <= 0xDBFFU) {
                    uint32_t low;
                    if (p->n - p->i < 6U || p->s[p->i] != '\\' ||
                        p->s[p->i + 1U] != 'u' ||
                        !yl_json_hex4(p->s + p->i + 2U, &low) ||
                        low < 0xDC00U || low > 0xDFFFU) return false;
                    p->i += 6U;
                } else if (cp >= 0xDC00U && cp <= 0xDFFFU) return false;
                continue;
            }
            return false;
        }
    }
    return false;
}
static bool yl_json_value(YLJson *p, size_t *start, size_t *end);
static bool yl_json_number(YLJson *p) {
    size_t i = p->i;
    if (i < p->n && p->s[i] == '-') i++;
    if (i >= p->n) return false;
    if (p->s[i] == '0') i++;
    else {
        if (p->s[i] < '1' || p->s[i] > '9') return false;
        while (i < p->n && p->s[i] >= '0' && p->s[i] <= '9') i++;
    }
    if (i < p->n && p->s[i] == '.') {
        i++;
        if (i >= p->n || p->s[i] < '0' || p->s[i] > '9') return false;
        while (i < p->n && p->s[i] >= '0' && p->s[i] <= '9') i++;
    }
    if (i < p->n && (p->s[i] == 'e' || p->s[i] == 'E')) {
        i++;
        if (i < p->n && (p->s[i] == '+' || p->s[i] == '-')) i++;
        if (i >= p->n || p->s[i] < '0' || p->s[i] > '9') return false;
        while (i < p->n && p->s[i] >= '0' && p->s[i] <= '9') i++;
    }
    p->i = i; return true;
}
static bool yl_json_value(YLJson *p, size_t *start, size_t *end) {
    yl_json_ws(p);
    if (p->depth >= YL_JSON_MAX_DEPTH || p->i >= p->n) return false;
    *start = p->i;
    char c = p->s[p->i];
    if (c == '"') {
        size_t a, b;
        if (!yl_json_string_end(p, &a, &b)) return false;
    } else if (c == '{' || c == '[') {
        char close = c == '{' ? '}' : ']';
        p->i++; p->depth++; yl_json_ws(p);
        if (p->i < p->n && p->s[p->i] == close) p->i++;
        else {
            for (;;) {
                if (c == '{') {
                    size_t a, b;
                    if (!yl_json_string_end(p, &a, &b)) { p->depth--; return false; }
                    yl_json_ws(p);
                    if (p->i >= p->n || p->s[p->i++] != ':') { p->depth--; return false; }
                }
                size_t vs, ve;
                if (!yl_json_value(p, &vs, &ve)) { p->depth--; return false; }
                yl_json_ws(p);
                if (p->i < p->n && p->s[p->i] == close) { p->i++; break; }
                if (p->i >= p->n || p->s[p->i++] != ',') { p->depth--; return false; }
                yl_json_ws(p);
            }
        }
        p->depth--;
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        if (!yl_json_number(p)) return false;
    } else if (p->n - p->i >= 4U && memcmp(p->s + p->i, "true", 4U) == 0) p->i += 4U;
    else if (p->n - p->i >= 5U && memcmp(p->s + p->i, "false", 5U) == 0) p->i += 5U;
    else if (p->n - p->i >= 4U && memcmp(p->s + p->i, "null", 4U) == 0) p->i += 4U;
    else return false;
    *end = p->i; return true;
}
static bool yl_json_root(const char *text, YLJson *p, size_t *start, size_t *end) {
    if (!text) { yl_json_error("JSON input is null"); return false; }
    size_t n = strlen(text);
    if (n > YL_JSON_MAX_INPUT) { yl_json_error("JSON input exceeds the 16 MiB limit"); return false; }
    p->s = text; p->n = n; p->i = 0U; p->depth = 0U;
    if (!yl_json_value(p, start, end)) { yl_json_error("Invalid JSON syntax"); return false; }
    yl_json_ws(p);
    if (p->i != p->n) { yl_json_error("Unexpected data after JSON value"); return false; }
    yl_json_error(""); return true;
}
static const char *yl_json_parse(const char *text) {
    YLJson p; size_t a, b;
    if (!yl_json_root(text, &p, &a, &b)) return "";
    size_t length = strlen(text);
    char *copy = malloc(length + 1U);
    if (!copy) yl_runtime_error("out of memory");
    memcpy(copy, text, length + 1U);
    return (const char *)yl_track(copy, length + 1U);
}
static bool yl_json_valid(const char *text) {
    YLJson p; size_t a, b; return yl_json_root(text, &p, &a, &b);
}
static const char *yl_json_stringify(const char *raw) {
    YLJson p; size_t a, b;
    if (!yl_json_root(raw, &p, &a, &b)) return "";
    size_t length = strlen(raw);
    char *copy = malloc(length + 1U);
    if (!copy) yl_runtime_error("out of memory");
    memcpy(copy, raw, length + 1U);
    return (const char *)yl_track(copy, strlen(copy) + 1U);
}

typedef struct { char *data; size_t length; size_t capacity; } YLJsonBuffer;
static void yl_json_buffer_reserve(YLJsonBuffer *b, size_t extra) {
    if (extra > SIZE_MAX - b->length - 1U) yl_runtime_error("JSON output is too large");
    size_t needed = b->length + extra + 1U;
    if (needed <= b->capacity) return;
    size_t capacity = b->capacity ? b->capacity : 128U;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2U) { capacity = needed; break; }
        capacity *= 2U;
    }
    char *next = realloc(b->data, capacity);
    if (!next) yl_runtime_error("out of memory while formatting JSON");
    b->data = next; b->capacity = capacity;
}
static void yl_json_buffer_char(YLJsonBuffer *b, char c) {
    yl_json_buffer_reserve(b, 1U);
    b->data[b->length++] = c;
    b->data[b->length] = '\0';
}
static void yl_json_buffer_indent(YLJsonBuffer *b, unsigned depth) {
    yl_json_buffer_char(b, '\n');
    for (unsigned i = 0; i < depth * 2U; i++) yl_json_buffer_char(b, ' ');
}
static const char *yl_json_pretty(const char *raw) {
    YLJson parser; size_t start, end;
    if (!yl_json_root(raw, &parser, &start, &end)) return "";
    YLJsonBuffer out = {0};
    bool in_string = false, escaped = false;
    unsigned depth = 0U;
    for (size_t i = 0U; raw[i]; i++) {
        char c = raw[i];
        if (in_string) {
            yl_json_buffer_char(&out, c);
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') in_string = false;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        if (c == '"') {
            in_string = true;
            yl_json_buffer_char(&out, c);
        } else if (c == '{' || c == '[') {
            yl_json_buffer_char(&out, c);
            size_t next = i + 1U;
            while (raw[next] == ' ' || raw[next] == '\t' || raw[next] == '\r' || raw[next] == '\n') next++;
            if (raw[next] != (c == '{' ? '}' : ']')) {
                depth++;
                yl_json_buffer_indent(&out, depth);
            }
        } else if (c == '}' || c == ']') {
            char opening = c == '}' ? '{' : '[';
            if (out.length && out.data[out.length - 1U] != opening) {
                if (depth) depth--;
                yl_json_buffer_indent(&out, depth);
            }
            yl_json_buffer_char(&out, c);
        } else if (c == ',') {
            yl_json_buffer_char(&out, c);
            yl_json_buffer_indent(&out, depth);
        } else if (c == ':') {
            yl_json_buffer_char(&out, c);
            yl_json_buffer_char(&out, ' ');
        } else {
            yl_json_buffer_char(&out, c);
        }
    }
    if (!out.data) {
        out.data = malloc(1U);
        if (!out.data) yl_runtime_error("out of memory while formatting JSON");
        out.data[0] = '\0';
    }
    return (const char *)yl_track(out.data, out.length + 1U);
}
static bool yl_json_key_equal(const char *s, size_t a, size_t b, const char *key) {
    size_t k = 0U;
    for (size_t i = a; i < b; i++) {
        unsigned char bytes[4];
        size_t count = 1U;
        unsigned char c = (unsigned char)s[i];
        if (c == '\\') {
            if (++i >= b) return false;
            char e = s[i];
            if (e == 'u') {
                uint32_t cp;
                if (b - i - 1U < 4U || !yl_json_hex4(s + i + 1U, &cp)) return false;
                i += 4U;
                if (cp >= 0xD800U && cp <= 0xDBFFU) {
                    uint32_t low;
                    if (b - i < 6U || s[i + 1U] != '\\' || s[i + 2U] != 'u' ||
                        !yl_json_hex4(s + i + 3U, &low) || low < 0xDC00U || low > 0xDFFFU) return false;
                    i += 6U;
                    cp = 0x10000U + ((cp - 0xD800U) << 10U) + (low - 0xDC00U);
                } else if (cp >= 0xDC00U && cp <= 0xDFFFU) return false;
                if (cp < 0x80U) bytes[0] = (unsigned char)cp;
                else if (cp < 0x800U) {
                    bytes[0] = (unsigned char)(0xC0U | (cp >> 6U));
                    bytes[1] = (unsigned char)(0x80U | (cp & 63U)); count = 2U;
                } else if (cp < 0x10000U) {
                    bytes[0] = (unsigned char)(0xE0U | (cp >> 12U));
                    bytes[1] = (unsigned char)(0x80U | ((cp >> 6U) & 63U));
                    bytes[2] = (unsigned char)(0x80U | (cp & 63U)); count = 3U;
                } else {
                    bytes[0] = (unsigned char)(0xF0U | (cp >> 18U));
                    bytes[1] = (unsigned char)(0x80U | ((cp >> 12U) & 63U));
                    bytes[2] = (unsigned char)(0x80U | ((cp >> 6U) & 63U));
                    bytes[3] = (unsigned char)(0x80U | (cp & 63U)); count = 4U;
                }
            } else {
                switch (e) {
                    case '"': c = '"'; break; case '\\': c = '\\'; break; case '/': c = '/'; break;
                    case 'b': c = '\b'; break; case 'f': c = '\f'; break; case 'n': c = '\n'; break;
                    case 'r': c = '\r'; break; case 't': c = '\t'; break; default: return false;
                }
                bytes[0] = c;
            }
        } else bytes[0] = c;
        for (size_t j = 0U; j < count; j++) {
            if (!key[k] || (unsigned char)key[k] != bytes[j]) return false;
            k++;
        }
    }
    return key[k] == '\0';
}
static bool yl_json_lookup(const char *text, const char *key, size_t *a, size_t *b) {
    YLJson p; size_t rs, re;
    if (!yl_json_root(text, &p, &rs, &re)) return false;
    p.i = rs; yl_json_ws(&p);
    if (p.i >= p.n || p.s[p.i++] != '{') { yl_json_error("JSON value is not an object"); return false; }
    yl_json_ws(&p);
    while (p.i < p.n && p.s[p.i] != '}') {
        size_t ka, kb, vs, ve;
        if (!yl_json_string_end(&p, &ka, &kb)) return false;
        yl_json_ws(&p); p.i++; yl_json_ws(&p);
        if (!yl_json_value(&p, &vs, &ve)) return false;
        if (yl_json_key_equal(p.s, ka, kb, key ? key : "")) { *a=vs; *b=ve; return true; }
        yl_json_ws(&p);
        if (p.i < p.n && p.s[p.i] == ',') { p.i++; yl_json_ws(&p); }
        else break;
    }
    yl_json_error("JSON object key was not found"); return false;
}
static const char *yl_json_get(const char *text, const char *key) {
    size_t a,b;
    if (!yl_json_lookup(text,key,&a,&b)) return "";
    char *out = malloc(b-a+1U); if (!out) yl_runtime_error("out of memory");
    memcpy(out,text+a,b-a); out[b-a]='\0';
    return (const char *)yl_track(out,b-a+1U);
}
static const char *yl_json_decode_string(const char *s, size_t a, size_t b) {
    size_t cap = (b-a)*3U+1U, n=0U;
    char *out=malloc(cap); if(!out) yl_runtime_error("out of memory");
    for(size_t i=a;i<b;i++) {
        unsigned char c=(unsigned char)s[i];
        if(c=='\\') {
            char e=s[++i];
            if(e=='u') {
                uint32_t cp; (void)yl_json_hex4(s+i+1U,&cp); i+=4U;
                if(cp>=0xD800U && cp<=0xDBFFU) {
                    uint32_t low; (void)yl_json_hex4(s+i+3U,&low); i+=6U;
                    cp=0x10000U+((cp-0xD800U)<<10U)+(low-0xDC00U);
                }
                if(cp==0U) {
                    free(out);
                    yl_json_error("JSON string contains U+0000, which YLang strings cannot represent");
                    return "";
                }
                if(cp<0x80U) out[n++]=(char)cp;
                else if(cp<0x800U){out[n++]=(char)(0xC0U|(cp>>6U));out[n++]=(char)(0x80U|(cp&63U));}
                else if(cp<0x10000U){out[n++]=(char)(0xE0U|(cp>>12U));out[n++]=(char)(0x80U|((cp>>6U)&63U));out[n++]=(char)(0x80U|(cp&63U));}
                else {out[n++]=(char)(0xF0U|(cp>>18U));out[n++]=(char)(0x80U|((cp>>12U)&63U));out[n++]=(char)(0x80U|((cp>>6U)&63U));out[n++]=(char)(0x80U|(cp&63U));}
            } else { switch(e){case 'b':c='\b';break;case 'f':c='\f';break;case 'n':c='\n';break;case 'r':c='\r';break;case 't':c='\t';break;default:c=(unsigned char)e;break;} out[n++]=(char)c; }
        } else out[n++]=(char)c;
    }
    out[n]='\0'; return (const char *)yl_track(out,n+1U);
}
static const char *yl_json_get_string(const char *text,const char *key) {
    size_t a,b; if(!yl_json_lookup(text,key,&a,&b) || text[a]!='"') {yl_json_error("JSON property is not a string");return "";}
    YLJson p={text,strlen(text),a,0U}; size_t x,y;
    if(!yl_json_string_end(&p,&x,&y)) return "";
    return yl_json_decode_string(text,x,y);
}
static int64_t yl_json_get_int(const char *text,const char *key) {
    size_t a,b; if(!yl_json_lookup(text,key,&a,&b)) return 0;
    char *end=NULL; errno=0; long long v=strtoll(text+a,&end,10);
    if(errno==ERANGE || end!=text+b) {yl_json_error("JSON property is not a signed integer");return 0;}
    return (int64_t)v;
}
static double yl_json_get_float(const char *text,const char *key) {
    size_t a,b; if(!yl_json_lookup(text,key,&a,&b)) return 0.0;
    size_t raw_length=b-a;
    char *raw=malloc(raw_length+1U), *end=NULL;
    if(!raw) yl_runtime_error("out of memory");
    memcpy(raw,text+a,raw_length); raw[raw_length]='\0';
    errno=0; double v=strtod(raw,&end); bool okay=errno!=ERANGE && end && *end=='\0'; free(raw);
    if(!okay){yl_json_error("JSON property is not a number");return 0.0;} return v;
}
static bool yl_json_get_bool(const char *text,const char *key) {
    size_t a,b; if(!yl_json_lookup(text,key,&a,&b)) return false;
    if(b-a==4U && memcmp(text+a,"true",4U)==0) return true;
    if(b-a==5U && memcmp(text+a,"false",5U)==0) return false;
    yl_json_error("JSON property is not a boolean"); return false;
}
static bool yl_json_has(const char *text,const char *key) {
    size_t a,b; return yl_json_lookup(text,key,&a,&b);
}
static int64_t yl_json_array_length(const char *text) {
    YLJson p; size_t a,b; if(!yl_json_root(text,&p,&a,&b)) return 0;
    p.i=a; if(p.s[p.i++]!='['){yl_json_error("JSON value is not an array");return 0;}
    yl_json_ws(&p); if(p.i<p.n && p.s[p.i]==']') return 0;
    int64_t count=0;
    for(;;){size_t x,y;if(!yl_json_value(&p,&x,&y))return count;count++;yl_json_ws(&p);
        if(p.i<p.n && p.s[p.i]==']') return count;
        if(p.i>=p.n || p.s[p.i++]!=',') break;yl_json_ws(&p);}
    yl_json_error("Invalid JSON array"); return count;
}
static const char *yl_json_at(const char *text,int64_t index) {
    YLJson p; size_t a,b; if(index<0 || !yl_json_root(text,&p,&a,&b)) return "";
    p.i=a; if(p.s[p.i++]!='['){yl_json_error("JSON value is not an array");return "";}
    yl_json_ws(&p);
    for(int64_t i=0;;i++){size_t x,y;if(!yl_json_value(&p,&x,&y)){yl_json_error("JSON array index out of range");return "";}
        if(i==index){size_t len=y-x;char *o=malloc(len+1U);if(!o)yl_runtime_error("out of memory");memcpy(o,text+x,len);o[len]='\0';return (const char *)yl_track(o,len+1U);}
        yl_json_ws(&p);if(p.i>=p.n || p.s[p.i++]!=',')break;yl_json_ws(&p);}
    yl_json_error("JSON array index out of range");return "";
}
static const char *yl_json_escape(const char *value) {
    size_t n=strlen(value), cap;
    if(n>(SIZE_MAX-3U)/6U) yl_runtime_error("JSON string is too large");
    cap=n*6U+3U; char *o=malloc(cap);if(!o)yl_runtime_error("out of memory");
    size_t j=0U;o[j++]='"';
    static const char hex[]="0123456789abcdef";
    for(size_t i=0;i<n;i++){unsigned char c=(unsigned char)value[i];
        if(c=='"'||c=='\\'){o[j++]='\\';o[j++]=(char)c;}
        else if(c=='\b'){o[j++]='\\';o[j++]='b';}
        else if(c=='\f'){o[j++]='\\';o[j++]='f';}
        else if(c=='\n'){o[j++]='\\';o[j++]='n';}
        else if(c=='\r'){o[j++]='\\';o[j++]='r';}
        else if(c=='\t'){o[j++]='\\';o[j++]='t';}
        else if(c<0x20U){o[j++]='\\';o[j++]='u';o[j++]='0';o[j++]='0';o[j++]=hex[c>>4U];o[j++]=hex[c&15U];}
        else o[j++]=(char)c;
    }
    o[j++]='"';o[j]='\0';return (const char *)yl_track(o,j+1U);
}
static const char *yl_json_last_error(void) { return yl_json_error_text; }

static const char *yl_env_get(const char *name) {
    if (!name || !*name) return "";
    const char *value = getenv(name);
    return value ? value : "";
}

static const char *yl_boolstr(bool value) { return value ? "true" : "false"; }
static const char *yl_format(const char *format, ...);
static const char *yl_to_string_int(int64_t value) { return yl_format("%lld", (long long)value); }
static const char *yl_to_string_float(double value) { return yl_format("%.17g", value); }
static const char *yl_to_string_char(char value) { return yl_format("%c", (int)value); }
static int64_t yl_to_int(double value) {
    if (!isfinite(value) ||
        value < -9223372036854775808.0 ||
        value >= 9223372036854775808.0) {
        yl_runtime_error("to_int value is outside the signed 64-bit range");
    }
    return (int64_t)value;
}
static bool yl_string_is_int(const char *value) {
    if (!value || !*value) return false;
    errno = 0;
    char *end = NULL;
    (void)strtoll(value, &end, 10);
    return errno != ERANGE && end != value && end && *end == '\0';
}
static int64_t yl_string_parse_int(const char *value) {
    if (!yl_string_is_int(value)) {
        yl_runtime_error("string.parse_int received invalid or out-of-range integer text");
    }
    return (int64_t)strtoll(value, NULL, 10);
}
static const char *yl_format(const char *format, ...) {
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(args);
        yl_runtime_error("formatting failed");
    }
    char *buffer = malloc((size_t)needed + 1);
    if (!buffer) {
        va_end(args);
        yl_runtime_error("out of memory");
    }
    (void)vsnprintf(buffer, (size_t)needed + 1, format, args);
    va_end(args);
    return (const char *)yl_track(buffer, (size_t)needed + 1U);
}
static int64_t yl_add_i64(int64_t a, int64_t b) {
    int64_t result;
    if (__builtin_add_overflow(a, b, &result)) {
        yl_runtime_error("integer overflow in addition");
    }
    return result;
}
static int64_t yl_add_assign_i64(int64_t *target, int64_t value) {
    *target = yl_add_i64(*target, value);
    return *target;
}
static double yl_add_assign_f64(double *target, double value) {
    *target += value;
    return *target;
}
static int64_t yl_sub_i64(int64_t a, int64_t b) {
    int64_t result;
    if (__builtin_sub_overflow(a, b, &result)) {
        yl_runtime_error("integer overflow in subtraction");
    }
    return result;
}
static int64_t yl_mul_i64(int64_t a, int64_t b) {
    int64_t result;
    if (__builtin_mul_overflow(a, b, &result)) {
        yl_runtime_error("integer overflow in multiplication");
    }
    return result;
}
static int64_t yl_neg_i64(int64_t value) {
    if (value == INT64_MIN) yl_runtime_error("integer overflow in negation");
    return -value;
}
static int64_t yl_div_i64(int64_t a, int64_t b) {
    if (b == 0) yl_runtime_error("integer division by zero");
    if (a == INT64_MIN && b == -1) {
        yl_runtime_error("integer overflow in division");
    }
    return a / b;
}
static int64_t yl_mod_i64(int64_t a, int64_t b) {
    if (b == 0) yl_runtime_error("integer modulo by zero");
    if (a == INT64_MIN && b == -1) return 0;
    return a % b;
}
static double yl_div_f64(double a, double b) {
    if (b == 0.0) yl_runtime_error("floating-point division by zero");
    return a / b;
}
static size_t yl_bounds(int64_t index, size_t length) {
    if (index < 0 || (uint64_t)index >= (uint64_t)length) {
        yl_runtime_error("array index out of bounds");
    }
    return (size_t)index;
}
static double yl_math_clamp(double value, double low, double high) {
    if (low > high) yl_runtime_error("math.clamp lower bound exceeds upper bound");
    return fmin(fmax(value, low), high);
}
static int64_t yl_string_length(const char *value) {
    return (int64_t)strlen(value);
}
static bool yl_string_contains(const char *value, const char *needle) {
    return strstr(value, needle) != NULL;
}
static bool yl_string_starts_with(const char *value, const char *prefix) {
    size_t prefix_length = strlen(prefix);
    return strncmp(value, prefix, prefix_length) == 0;
}
static bool yl_string_ends_with(const char *value, const char *suffix) {
    size_t value_length = strlen(value);
    size_t suffix_length = strlen(suffix);
    return suffix_length <= value_length &&
           memcmp(value + value_length - suffix_length, suffix, suffix_length) == 0;
}
static const char *yl_string_concat(const char *left, const char *right) {
    return yl_format("%s%s", left, right);
}
static const char *yl_string_replace(const char *value, const char *needle,
                                    const char *replacement) {
    if (!*needle) return yl_format("%s", value);
    size_t value_length = strlen(value);
    size_t needle_length = strlen(needle);
    size_t replacement_length = strlen(replacement);
    size_t match_count = 0;
    const char *scan = value;
    while ((scan = strstr(scan, needle)) != NULL) {
        match_count++;
        scan += needle_length;
    }
    size_t result_length;
    if (replacement_length >= needle_length) {
        size_t delta = replacement_length - needle_length;
        if (delta && match_count > (SIZE_MAX - value_length - 1) / delta) {
            yl_runtime_error("string replacement result is too large");
        }
        result_length = value_length + match_count * delta;
    } else {
        result_length = value_length - match_count * (needle_length - replacement_length);
    }
    if (result_length == SIZE_MAX) {
        yl_runtime_error("string replacement result is too large");
    }
    char *result = malloc(result_length + 1);
    if (!result) yl_runtime_error("out of memory");
    const char *src = value;
    char *dst = result;
    const char *match;
    while ((match = strstr(src, needle)) != NULL) {
        size_t span = (size_t)(match - src);
        memcpy(dst, src, span);
        dst += span;
        memcpy(dst, replacement, replacement_length);
        dst += replacement_length;
        src = match + needle_length;
    }
    strcpy(dst, src);
    return (const char *)yl_track(result, result_length + 1U);
}
static const char *yl_string_case(const char *value, bool upper) {
    size_t length = strlen(value);
    char *result = malloc(length + 1);
    if (!result) yl_runtime_error("out of memory");
    for (size_t i = 0; i < length; i++) {
        unsigned char ch = (unsigned char)value[i];
        result[i] = (char)(upper ? toupper(ch) : tolower(ch));
    }
    result[length] = '\0';
    return (const char *)yl_track(result, length + 1U);
}
static const char *yl_string_upper(const char *value) { return yl_string_case(value, true); }
static const char *yl_string_lower(const char *value) { return yl_string_case(value, false); }
static const char *yl_read_line(void) {
    size_t capacity = 128U;
    size_t length = 0U;
    char *line = malloc(capacity);
    if (!line) yl_runtime_error("out of memory");
    int ch = 0;
    while ((ch = fgetc(stdin)) != EOF && ch != '\n') {
        if (length + 1U >= capacity) {
            if (capacity > SIZE_MAX / 2U) {
                free(line);
                yl_runtime_error("input line is too large");
            }
            size_t next = capacity * 2U;
            char *grown = realloc(line, next);
            if (!grown) {
                free(line);
                yl_runtime_error("out of memory");
            }
            line = grown;
            capacity = next;
        }
        line[length++] = (char)ch;
    }
    if (ch == EOF && ferror(stdin)) {
        free(line);
        yl_runtime_error("failed to read standard input");
    }
    while (length > 0U && line[length - 1U] == '\r') length--;
    line[length] = '\0';
    return (const char *)yl_track(line, capacity);
}
static const char *yl_read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) yl_runtime_error("cannot open file for reading");
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        yl_runtime_error("cannot seek input file");
    }
    long end = ftell(file);
    if (end < 0 || (uintmax_t)end >= (uintmax_t)SIZE_MAX) {
        fclose(file);
        yl_runtime_error("input file is too large");
    }
    rewind(file);
    size_t size = (size_t)end;
    char *data = malloc(size + 1);
    if (!data) {
        fclose(file);
        yl_runtime_error("out of memory");
    }
    size_t got = fread(data, 1, size, file);
    bool failed = ferror(file) != 0 || got != size;
    fclose(file);
    if (failed) {
        free(data);
        yl_runtime_error("failed to read input file");
    }
    if (memchr(data, '\0', size)) {
        free(data);
        yl_runtime_error("binary files are not supported by string I/O");
    }
    data[size] = '\0';
    return (const char *)yl_track(data, size + 1U);
}
static bool yl_write_file(const char *path, const char *content) {
    FILE *file = fopen(path, "wb");
    if (!file) return false;
    size_t size = strlen(content);
    bool ok = fwrite(content, 1, size, file) == size;
    if (fclose(file) != 0) ok = false;
    return ok;
}
#define YL_BINARY_MAX_READ (16U * 1024U * 1024U)
static bool yl_binary_seek(FILE *file, int64_t offset) {
    return file && offset >= 0 && (uint64_t)offset <= (uint64_t)LONG_MAX &&
           fseek(file, (long)offset, SEEK_SET) == 0;
}
static int64_t yl_binary_size(const char *path) {
    if (!path) return -1;
    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    bool ok = fseek(file, 0, SEEK_END) == 0;
    long end = ok ? ftell(file) : -1L;
    if (fclose(file) != 0) ok = false;
    return ok && end >= 0 ? (int64_t)end : -1;
}
static int64_t yl_binary_read_u8(const char *path, int64_t offset) {
    if (!path || offset < 0) return -1;
    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    int value = yl_binary_seek(file, offset) ? fgetc(file) : EOF;
    bool failed = ferror(file) != 0;
    fclose(file);
    return failed || value == EOF ? -1 : (int64_t)(unsigned char)value;
}
static bool yl_binary_write_u8(const char *path, int64_t offset, int64_t value) {
    if (!path || offset < 0 || value < 0 || value > 255) return false;
    FILE *file = fopen(path, "r+b");
    if (!file) file = fopen(path, "w+b");
    if (!file) return false;
    bool ok = yl_binary_seek(file, offset) && fputc((unsigned char)value, file) != EOF;
    if (fclose(file) != 0) ok = false;
    return ok;
}
static const char *yl_binary_read_hex(const char *path, int64_t offset, int64_t length) {
    static const char digits[] = "0123456789abcdef";
    if (!path || offset < 0 || length < 0 || (uint64_t)length > YL_BINARY_MAX_READ)
        yl_runtime_error("invalid binary read range (maximum 16 MiB)");
    FILE *file = fopen(path, "rb");
    if (!file) yl_runtime_error("cannot open binary file for reading");
    if (!yl_binary_seek(file, offset)) {
        fclose(file);
        yl_runtime_error("cannot seek binary input file");
    }
    size_t count = (size_t)length;
    unsigned char *bytes = count ? malloc(count) : NULL;
    if (count && !bytes) { fclose(file); yl_runtime_error("out of memory"); }
    size_t got = count ? fread(bytes, 1U, count, file) : 0U;
    bool failed = ferror(file) != 0;
    fclose(file);
    if (failed) { free(bytes); yl_runtime_error("failed to read binary input"); }
    size_t hex_size = got * 2U;
    char *hex = malloc(hex_size + 1U);
    if (!hex) { free(bytes); yl_runtime_error("out of memory"); }
    for (size_t i = 0; i < got; i++) {
        hex[i * 2U] = digits[bytes[i] >> 4U];
        hex[i * 2U + 1U] = digits[bytes[i] & 15U];
    }
    hex[hex_size] = '\0';
    free(bytes);
    return (const char *)yl_track(hex, hex_size + 1U);
}
static int yl_binary_hex_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}
static bool yl_binary_write_hex(const char *path, int64_t offset, const char *hex) {
    if (!path || !hex || offset < 0) return false;
    size_t length = strlen(hex);
    if ((length & 1U) != 0U || length / 2U > YL_BINARY_MAX_READ) return false;
    for (size_t i = 0; i < length; i++) {
        if (yl_binary_hex_value(hex[i]) < 0) return false;
    }
    FILE *file = fopen(path, "r+b");
    if (!file) file = fopen(path, "w+b");
    if (!file) return false;
    bool ok = yl_binary_seek(file, offset);
    for (size_t i = 0; ok && i < length; i += 2U) {
        int high = yl_binary_hex_value(hex[i]);
        int low = yl_binary_hex_value(hex[i + 1U]);
        if (fputc((high << 4) | low, file) == EOF) ok = false;
    }
    if (fclose(file) != 0) ok = false;
    return ok;
}

static uint8_t yl_to_byte(int64_t value) {
    if (value < 0 || value > 255) yl_runtime_error("byte conversion is outside the range 0..255");
    return (uint8_t)value;
}
static int64_t yl_binary_read_bytes(const char *path, int64_t offset,
                                    uint8_t *buffer, size_t capacity) {
    if (!path || (!buffer && capacity != 0U) || offset < 0 ||
        (uint64_t)offset > (uint64_t)LONG_MAX) return -1;
    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    bool positioned = yl_binary_seek(file, offset);
    size_t count = positioned && capacity ? fread(buffer, 1U, capacity, file) : 0U;
    bool failed = !positioned || ferror(file) != 0;
    fclose(file);
    return failed ? -1 : (int64_t)count;
}
static int64_t yl_binary_write_bytes(const char *path, int64_t offset,
                                     const uint8_t *buffer, size_t length) {
    if (!path || (!buffer && length != 0U) || offset < 0 ||
        (uint64_t)offset > (uint64_t)LONG_MAX) return -1;
    FILE *file = fopen(path, "r+b");
    if (!file) file = fopen(path, "w+b");
    if (!file) return -1;
    bool positioned = yl_binary_seek(file, offset);
    size_t count = positioned && length ? fwrite(buffer, 1U, length, file) : 0U;
    bool failed = !positioned || ferror(file) != 0;
    if (fclose(file) != 0) failed = true;
    return failed ? -1 : (int64_t)count;
}
static bool yl_binary_read_scalar(const char *path, int64_t offset,
                                  unsigned width, bool big_endian,
                                  uint64_t *value) {
    if (!path || !value || offset < 0 || width == 0U || width > 8U) return false;
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    bool ok = yl_binary_seek(file, offset);
    uint8_t bytes[8] = {0};
    if (ok) ok = fread(bytes, 1U, width, file) == width;
    if (fclose(file) != 0) ok = false;
    if (!ok) return false;
    uint64_t result = 0U;
    for (unsigned i = 0; i < width; i++) {
        unsigned source = big_endian ? i : (width - 1U - i);
        result = (result << 8U) | (uint64_t)bytes[source];
    }
    *value = result;
    return true;
}
static bool yl_binary_write_scalar(const char *path, int64_t offset,
                                   unsigned width, bool big_endian,
                                   uint64_t value) {
    if (!path || offset < 0 || width == 0U || width > 8U) return false;
    FILE *file = fopen(path, "r+b");
    if (!file) file = fopen(path, "w+b");
    if (!file) return false;
    bool ok = yl_binary_seek(file, offset);
    for (unsigned i = 0; ok && i < width; i++) {
        unsigned shift = big_endian ? (width - 1U - i) * 8U : i * 8U;
        if (fputc((int)((value >> shift) & UINT64_C(0xff)), file) == EOF) ok = false;
    }
    if (fclose(file) != 0) ok = false;
    return ok;
}
#define YL_DEFINE_BINARY_SCALAR(bits, width) \
static int64_t yl_binary_read_u##bits##_le(const char *p, int64_t o) { \
    uint64_t v = 0U; int64_t r = -1; \
    if (yl_binary_read_scalar(p, o, width, false, &v)) { \
        if (width == 8U) memcpy(&r, &v, sizeof(r)); else r = (int64_t)v; \
    } return r; \
} \
static int64_t yl_binary_read_u##bits##_be(const char *p, int64_t o) { \
    uint64_t v = 0U; int64_t r = -1; \
    if (yl_binary_read_scalar(p, o, width, true, &v)) { \
        if (width == 8U) memcpy(&r, &v, sizeof(r)); else r = (int64_t)v; \
    } return r; \
} \
static bool yl_binary_write_u##bits##_le(const char *p, int64_t o, int64_t v) { \
    return yl_binary_write_scalar(p, o, width, false, (uint64_t)v); \
} \
static bool yl_binary_write_u##bits##_be(const char *p, int64_t o, int64_t v) { \
    return yl_binary_write_scalar(p, o, width, true, (uint64_t)v); \
}
YL_DEFINE_BINARY_SCALAR(16, 2U)
YL_DEFINE_BINARY_SCALAR(32, 4U)
YL_DEFINE_BINARY_SCALAR(64, 8U)
#undef YL_DEFINE_BINARY_SCALAR

#define YL_BINARY_STREAM_SLOTS 64U
typedef struct {
    int64_t id;
    FILE *file;
} YLBinaryStream;
static YLBinaryStream yl_binary_streams[YL_BINARY_STREAM_SLOTS];
static int64_t yl_binary_next_stream_id = 1;
static int64_t yl_binary_stream_open(const char *path, const char *mode) {
    if (!path) return -1;
    size_t slot = YL_BINARY_STREAM_SLOTS;
    for (size_t i = 0; i < YL_BINARY_STREAM_SLOTS; i++) {
        if (!yl_binary_streams[i].file) { slot = i; break; }
    }
    if (slot == YL_BINARY_STREAM_SLOTS || yl_binary_next_stream_id <= 0 ||
        yl_binary_next_stream_id == INT64_MAX) return -1;
    FILE *file = fopen(path, mode);
    if (!file) return -1;
    int64_t id = yl_binary_next_stream_id++;
    yl_binary_streams[slot].id = id;
    yl_binary_streams[slot].file = file;
    return id;
}
static int64_t yl_binary_stream_open_read(const char *path) {
    return yl_binary_stream_open(path, "rb");
}
static int64_t yl_binary_stream_open_write(const char *path) {
    return yl_binary_stream_open(path, "wb");
}
static YLBinaryStream *yl_binary_stream_find(int64_t id) {
    if (id <= 0) return NULL;
    for (size_t i = 0; i < YL_BINARY_STREAM_SLOTS; i++)
        if (yl_binary_streams[i].file && yl_binary_streams[i].id == id)
            return &yl_binary_streams[i];
    return NULL;
}
static int64_t yl_binary_stream_read(int64_t id, uint8_t *buffer, size_t capacity) {
    YLBinaryStream *stream = yl_binary_stream_find(id);
    if (!stream || (!buffer && capacity != 0U)) return -1;
    size_t count = capacity ? fread(buffer, 1U, capacity, stream->file) : 0U;
    return ferror(stream->file) ? -1 : (int64_t)count;
}
static int64_t yl_binary_stream_write(int64_t id, const uint8_t *buffer, size_t length) {
    YLBinaryStream *stream = yl_binary_stream_find(id);
    if (!stream || (!buffer && length != 0U)) return -1;
    size_t count = length ? fwrite(buffer, 1U, length, stream->file) : 0U;
    return ferror(stream->file) ? -1 : (int64_t)count;
}
static bool yl_binary_stream_close(int64_t id) {
    YLBinaryStream *stream = yl_binary_stream_find(id);
    if (!stream) return false;
    FILE *file = stream->file;
    stream->file = NULL;
    stream->id = 0;
    return fclose(file) == 0;
}
static void yl_binary_stream_shutdown(void) {
    for (size_t i = 0; i < YL_BINARY_STREAM_SLOTS; i++) {
        if (yl_binary_streams[i].file) {
            fclose(yl_binary_streams[i].file);
            yl_binary_streams[i].file = NULL;
            yl_binary_streams[i].id = 0;
        }
    }
}
static bool yl_path_exists(const char *path) {
    struct stat info;
    return path && stat(path, &info) == 0;
}
static bool yl_path_separator(char ch) {
    return ch == '/' || ch == '\\';
}
static const char *yl_path_basename(const char *path) {
    if (!path) return "";
    size_t end = strlen(path);
    while (end > 1U && yl_path_separator(path[end - 1U])) end--;
    size_t start = end;
    while (start > 0U && !yl_path_separator(path[start - 1U])) start--;
    size_t length = end - start;
    bool root = length == 0U && end == 1U && yl_path_separator(path[0]);
    if (root) length = 1U;
    char *result = malloc(length + 1U);
    if (!result) yl_runtime_error("out of memory");
    memcpy(result, path + (root ? 0U : start), length);
    result[length] = '\0';
    return (const char *)yl_track(result, length + 1U);
}
static const char *yl_path_extension(const char *path) {
    const char *base = yl_path_basename(path);
    const char *dot = strrchr(base, '.');
    if (!dot || dot == base) return "";
    return dot + 1;
}
#ifdef _WIN32
static bool yl_image_open(const char *path) {
    if (!path || !*path || !yl_path_exists(path)) return false;
    HINSTANCE result = ShellExecuteA(NULL, "open", path, NULL, NULL, SW_SHOWNORMAL);
    return (INT_PTR)result > 32;
}
#else
static bool yl_image_open(const char *path) {
    if (!path || !*path || !yl_path_exists(path)) return false;
    pid_t child = fork();
    if (child < 0) return false;
    if (child == 0) {
#if defined(__APPLE__)
        execlp("open", "open", path, (char *)NULL);
#else
        execlp("xdg-open", "xdg-open", path, (char *)NULL);
#endif
        _exit(127);
    }
    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) continue;
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
#endif


#define YL_BINARY_BUFFER_SLOTS 64U
#define YL_BINARY_BUFFER_MAX (256U * 1024U * 1024U)
typedef struct {
    int64_t id;
    uint8_t *data;
    size_t length;
    size_t capacity;
} YLBinaryBuffer;
static YLBinaryBuffer yl_binary_buffers[YL_BINARY_BUFFER_SLOTS];
static int64_t yl_binary_next_buffer_id = 1;
static YLBinaryBuffer *yl_binary_buffer_find(int64_t id) {
    if (id <= 0) return NULL;
    for (size_t i = 0; i < YL_BINARY_BUFFER_SLOTS; i++)
        if (yl_binary_buffers[i].id == id && id != 0) return &yl_binary_buffers[i];
    return NULL;
}
static int64_t yl_binary_buffer_new(int64_t initial_capacity) {
    if (initial_capacity < 0 || (uint64_t)initial_capacity > YL_BINARY_BUFFER_MAX ||
        yl_binary_next_buffer_id <= 0 || yl_binary_next_buffer_id == INT64_MAX) return -1;
    size_t slot = YL_BINARY_BUFFER_SLOTS;
    for (size_t i = 0; i < YL_BINARY_BUFFER_SLOTS; i++)
        if (yl_binary_buffers[i].id == 0) { slot = i; break; }
    if (slot == YL_BINARY_BUFFER_SLOTS) return -1;
    size_t capacity = (size_t)initial_capacity;
    uint8_t *data = capacity ? malloc(capacity) : NULL;
    if (capacity && !data) return -1;
    int64_t id = yl_binary_next_buffer_id++;
    yl_binary_buffers[slot] = (YLBinaryBuffer){id, data, 0U, capacity};
    return id;
}
static bool yl_binary_buffer_reserve(YLBinaryBuffer *buffer, size_t needed) {
    if (!buffer || needed > YL_BINARY_BUFFER_MAX) return false;
    if (needed <= buffer->capacity) return true;
    size_t capacity = buffer->capacity ? buffer->capacity : 16U;
    while (capacity < needed) {
        if (capacity > YL_BINARY_BUFFER_MAX / 2U) {
            capacity = YL_BINARY_BUFFER_MAX;
            break;
        }
        capacity *= 2U;
    }
    if (capacity < needed) return false;
    uint8_t *grown = realloc(buffer->data, capacity);
    if (!grown) return false;
    buffer->data = grown;
    buffer->capacity = capacity;
    return true;
}
static int64_t yl_binary_buffer_length(int64_t id) {
    YLBinaryBuffer *buffer = yl_binary_buffer_find(id);
    return buffer ? (int64_t)buffer->length : -1;
}
static int64_t yl_binary_buffer_capacity(int64_t id) {
    YLBinaryBuffer *buffer = yl_binary_buffer_find(id);
    return buffer ? (int64_t)buffer->capacity : -1;
}
static int64_t yl_binary_buffer_get(int64_t id, int64_t index) {
    YLBinaryBuffer *buffer = yl_binary_buffer_find(id);
    if (!buffer || index < 0 || (uint64_t)index >= buffer->length) return -1;
    return (int64_t)buffer->data[(size_t)index];
}
static bool yl_binary_buffer_set(int64_t id, int64_t index, int64_t value) {
    YLBinaryBuffer *buffer = yl_binary_buffer_find(id);
    if (!buffer || index < 0 || (uint64_t)index >= buffer->length ||
        value < 0 || value > 255) return false;
    buffer->data[(size_t)index] = (uint8_t)value;
    return true;
}
static bool yl_binary_buffer_push(int64_t id, int64_t value) {
    YLBinaryBuffer *buffer = yl_binary_buffer_find(id);
    if (!buffer || value < 0 || value > 255 || buffer->length >= YL_BINARY_BUFFER_MAX ||
        !yl_binary_buffer_reserve(buffer, buffer->length + 1U)) return false;
    buffer->data[buffer->length++] = (uint8_t)value;
    return true;
}
static bool yl_binary_buffer_resize(int64_t id, int64_t new_length) {
    YLBinaryBuffer *buffer = yl_binary_buffer_find(id);
    if (!buffer || new_length < 0 || (uint64_t)new_length > YL_BINARY_BUFFER_MAX) return false;
    size_t length = (size_t)new_length;
    if (!yl_binary_buffer_reserve(buffer, length)) return false;
    if (length > buffer->length)
        memset(buffer->data + buffer->length, 0, length - buffer->length);
    buffer->length = length;
    return true;
}
static bool yl_binary_buffer_clear(int64_t id) {
    YLBinaryBuffer *buffer = yl_binary_buffer_find(id);
    if (!buffer) return false;
    buffer->length = 0U;
    return true;
}
static bool yl_binary_buffer_free(int64_t id) {
    YLBinaryBuffer *buffer = yl_binary_buffer_find(id);
    if (!buffer) return false;
    free(buffer->data);
    memset(buffer, 0, sizeof(*buffer));
    return true;
}
static int64_t yl_binary_buffer_load(const char *path, int64_t offset, int64_t length) {
    if (!path || offset < 0 || length < 0 ||
        (uint64_t)length > YL_BINARY_BUFFER_MAX) return -1;
    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    if (!yl_binary_seek(file, offset)) { fclose(file); return -1; }
    int64_t id = yl_binary_buffer_new(length);
    YLBinaryBuffer *buffer = yl_binary_buffer_find(id);
    if (!buffer) { fclose(file); return -1; }
    size_t count = length ? fread(buffer->data, 1U, (size_t)length, file) : 0U;
    bool failed = ferror(file) != 0;
    if (fclose(file) != 0) failed = true;
    if (failed) { (void)yl_binary_buffer_free(id); return -1; }
    buffer->length = count;
    return id;
}
static int64_t yl_binary_buffer_save(const char *path, int64_t offset, int64_t id) {
    YLBinaryBuffer *buffer = yl_binary_buffer_find(id);
    if (!path || !buffer || offset < 0) return -1;
    FILE *file = fopen(path, "r+b");
    if (!file) file = fopen(path, "w+b");
    if (!file) return -1;
    bool positioned = yl_binary_seek(file, offset);
    size_t count = positioned && buffer->length ?
        fwrite(buffer->data, 1U, buffer->length, file) : 0U;
    bool failed = !positioned || ferror(file) != 0;
    if (fclose(file) != 0) failed = true;
    return failed ? -1 : (int64_t)count;
}
static void yl_binary_buffer_shutdown(void) {
    for (size_t i = 0; i < YL_BINARY_BUFFER_SLOTS; i++) {
        free(yl_binary_buffers[i].data);
        memset(&yl_binary_buffers[i], 0, sizeof(yl_binary_buffers[i]));
    }
}
