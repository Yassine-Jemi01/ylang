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

static void yl_cleanup(void) {
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
    YL_HTTP_OPTION(CURLOPT_FOLLOWLOCATION, 1L);
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
    YL_HTTP_TASK_OPTION(CURLOPT_FOLLOWLOCATION, 1L);
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
