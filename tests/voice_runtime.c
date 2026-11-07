#include "ylang/voice.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_u16(FILE *file, unsigned value)
{
    fputc((int)(value & 0xffU), file);
    fputc((int)((value >> 8U) & 0xffU), file);
}

static void write_u32(FILE *file, unsigned value)
{
    write_u16(file, value & 0xffffU);
    write_u16(file, (value >> 16U) & 0xffffU);
}

static int make_test_wave(const char *path)
{
    const unsigned sample_rate = 44100U;
    const unsigned sample_count = sample_rate / 4U;
    const unsigned data_size = sample_count * 2U;
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    fwrite("RIFF", 1U, 4U, file);
    write_u32(file, 36U + data_size);
    fwrite("WAVEfmt ", 1U, 8U, file);
    write_u32(file, 16U);
    write_u16(file, 1U);
    write_u16(file, 1U);
    write_u32(file, sample_rate);
    write_u32(file, sample_rate * 2U);
    write_u16(file, 2U);
    write_u16(file, 16U);
    fwrite("data", 1U, 4U, file);
    write_u32(file, data_size);
    for (unsigned i = 0U; i < sample_count; i++) write_u16(file, 0U);
    return fclose(file) == 0;
}

typedef struct {
    unsigned char *data;
    size_t length;
    size_t offset;
} TestStream;

static TestStream test_stream;
static size_t test_stream_bytes_read;
static size_t test_stream_expected_length;
static size_t test_stream_fail_after = SIZE_MAX;

static int load_test_stream(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    if (fseek(file, 0L, SEEK_END) != 0) { fclose(file); return 0; }
    long end = ftell(file);
    if (end <= 0L || fseek(file, 0L, SEEK_SET) != 0) { fclose(file); return 0; }
    test_stream.data = malloc((size_t)end);
    if (!test_stream.data) { fclose(file); return 0; }
    test_stream.length = (size_t)end;
    test_stream.offset = 0U;
    test_stream_bytes_read = 0U;
    test_stream_expected_length = (size_t)end;
    if (fread(test_stream.data, 1U, test_stream.length, file) != test_stream.length) {
        fclose(file);
        free(test_stream.data);
        memset(&test_stream, 0, sizeof(test_stream));
        return 0;
    }
    fclose(file);
    return 1;
}

static int64_t test_stream_read(int64_t stream_id, void *destination, size_t max_bytes)
{
    if (stream_id != 7 || !destination || !test_stream.data) return -1;
    if (test_stream.offset >= test_stream_fail_after) return -1;
    size_t remaining = test_stream.length - test_stream.offset;
    if (remaining > test_stream_fail_after - test_stream.offset)
        remaining = test_stream_fail_after - test_stream.offset;
    size_t count = remaining < max_bytes ? remaining : max_bytes;
    if (count > 512U) count = 512U;
    if (count) {
        memcpy(destination, test_stream.data + test_stream.offset, count);
        test_stream.offset += count;
        test_stream_bytes_read += count;
    }
    return (int64_t)count;
}

static const char *test_stream_error(int64_t stream_id)
{
    (void)stream_id;
    return "simulated HTTP stream transport failure";
}

static void test_stream_close(int64_t stream_id)
{
    if (stream_id == 7) {
        free(test_stream.data);
        memset(&test_stream, 0, sizeof(test_stream));
    }
}

static int fail(const char *message)
{
    fprintf(stderr, "Voice runtime test failed: %s (detail: %s)\n",
            message, ylvoice_last_error());
    ylvoice_shutdown();
    (void)remove("build/voice-test.wav");
    return 1;
}

typedef struct {
    SDL_sem *ready;
    SDL_sem *resume;
    bool snapshot_stable;
} ErrorSnapshotContext;

static int SDLCALL test_error_snapshot_thread(void *userdata)
{
    ErrorSnapshotContext *context = (ErrorSnapshotContext *)userdata;
    const char *snapshot = ylvoice_last_error();
    char expected[256];
    (void)snprintf(expected, sizeof(expected), "%s", snapshot);
    (void)SDL_SemPost(context->ready);
    (void)SDL_SemWait(context->resume);
    context->snapshot_stable = strcmp(snapshot, expected) == 0;
    return 0;
}

int main(void)
{
    SDL_SetMainReady();
    const char *path = "build/voice-test.wav";
    if (!make_test_wave(path)) return fail("could not create the WAV fixture");
    if (!ylvoice_init()) return fail("audio initialization failed");
    if (!ylvoice_set_volume(80)) return fail("valid volume was rejected");
    if (!ylvoice_play(path)) return fail("valid WAV playback failed");
    if (!ylvoice_is_playing()) return fail("music did not enter the playing state");

    ylvoice_pause();
    if (!ylvoice_is_paused()) return fail("pause state was not reported");
    ylvoice_resume();
    if (ylvoice_is_paused()) return fail("resume did not clear the paused state");
    SDL_Delay(400U);
    if (!ylvoice_is_finished()) return fail("natural playback completion was not reported");
    if (ylvoice_is_playing()) return fail("completed music still reports as playing");

    if (!ylvoice_play(path)) return fail("replay after completion failed");
    ylvoice_stop();
    if (ylvoice_is_playing()) return fail("stop did not stop playback");
    if (ylvoice_is_finished()) return fail("explicit stop was incorrectly reported as natural completion");

    if (!load_test_stream(path)) return fail("could not load the callback stream fixture");
    if (!ylvoice_play_stream(7, test_stream_read, test_stream_error, test_stream_close))
        return fail("callback-backed stream playback failed");
    if (!ylvoice_is_playing()) return fail("stream playback did not enter the playing state");
    SDL_Delay(400U);
    if (!ylvoice_is_finished()) return fail("stream playback did not report natural completion");
    if (test_stream_bytes_read != test_stream_expected_length) return fail("audio decoder did not consume the complete stream");
    if (test_stream.data != NULL) return fail("stream close callback did not release its source");

    /* Pause/resume/stop must also work for callback-backed streams and
       stopping must release the transferred stream exactly once. */
    if (!load_test_stream(path)) return fail("could not reload the callback stream fixture");
    if (!ylvoice_play_stream(7, test_stream_read, test_stream_error, test_stream_close))
        return fail("second callback-backed stream playback failed");
    ylvoice_pause();
    if (!ylvoice_is_paused()) return fail("stream pause state was not reported");
    ylvoice_resume();
    if (ylvoice_is_paused()) return fail("stream resume did not clear the paused state");
    SDL_Delay(20U);
    ylvoice_stop();
    if (ylvoice_is_playing()) return fail("stopped stream still reports as playing");
    if (test_stream.data != NULL) return fail("stopping a stream did not release its source");

    /* A callback's transport diagnostic must not be replaced by SDL_mixer's
       generic decoder error when the stream fails during initial decoding. */
    if (!load_test_stream(path)) return fail("could not load the failing callback stream fixture");
    test_stream_fail_after = 12U;
    bool failing_stream_started = ylvoice_play_stream(
        7, test_stream_read, test_stream_error, test_stream_close);
    if (failing_stream_started) {
        for (unsigned i = 0U; i < 100U && !ylvoice_is_finished(); i++)
            SDL_Delay(10U);
    }
    if (strstr(ylvoice_last_error(), "simulated HTTP stream transport failure") == NULL)
        return fail("specific stream callback error was overwritten by the decoder");
    ylvoice_stop();
    if (test_stream.data != NULL) return fail("failed stream did not release its source");
    test_stream_fail_after = SIZE_MAX;

    if (ylvoice_set_volume(101)) return fail("volume above 100 was accepted");
    if (strstr(ylvoice_last_error(), "volume") == NULL)
        return fail("invalid volume did not produce a useful error");

    if (ylvoice_play("build/does-not-exist.wav"))
        return fail("missing audio file was accepted");
    if (ylvoice_last_error()[0] == '\0')
        return fail("missing audio file did not report an error");

    /* A snapshot returned on one thread must not be overwritten when another
       thread updates the shared latest-error message. */
    if (ylvoice_set_volume(101)) return fail("invalid volume was accepted before snapshot test");
    ErrorSnapshotContext snapshot_context = {
        SDL_CreateSemaphore(0U), SDL_CreateSemaphore(0U), false
    };
    if (!snapshot_context.ready || !snapshot_context.resume) {
        if (snapshot_context.ready) SDL_DestroySemaphore(snapshot_context.ready);
        if (snapshot_context.resume) SDL_DestroySemaphore(snapshot_context.resume);
        return fail("could not create error snapshot test semaphores");
    }
    SDL_Thread *snapshot_thread = SDL_CreateThread(
        test_error_snapshot_thread, "voice-error-snapshot", &snapshot_context);
    if (!snapshot_thread) {
        SDL_DestroySemaphore(snapshot_context.ready);
        SDL_DestroySemaphore(snapshot_context.resume);
        return fail("could not create error snapshot test thread");
    }
    if (SDL_SemWaitTimeout(snapshot_context.ready, 5000U) != 0) {
        (void)SDL_SemPost(snapshot_context.resume);
        (void)SDL_WaitThread(snapshot_thread, NULL);
        SDL_DestroySemaphore(snapshot_context.ready);
        SDL_DestroySemaphore(snapshot_context.resume);
        return fail("error snapshot worker did not become ready");
    }
    (void)ylvoice_play("build/does-not-exist.wav");
    (void)SDL_SemPost(snapshot_context.resume);
    (void)SDL_WaitThread(snapshot_thread, NULL);
    SDL_DestroySemaphore(snapshot_context.ready);
    SDL_DestroySemaphore(snapshot_context.resume);
    if (!snapshot_context.snapshot_stable)
        return fail("another thread overwrote the error snapshot");

    ylvoice_shutdown();
    ylvoice_shutdown();
    (void)remove(path);
    puts("Voice runtime tests passed.");
    return 0;
}
