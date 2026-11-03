#include "ylang/voice.h"

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
    size_t remaining = test_stream.length - test_stream.offset;
    size_t count = remaining < max_bytes ? remaining : max_bytes;
    if (count > 512U) count = 512U;
    if (count) {
        memcpy(destination, test_stream.data + test_stream.offset, count);
        test_stream.offset += count;
    }
    return (int64_t)count;
}

static const char *test_stream_error(int64_t stream_id)
{
    (void)stream_id;
    return "";
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

int main(void)
{
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
    if (test_stream.data != NULL) return fail("stream close callback did not release its source");

    if (ylvoice_set_volume(101)) return fail("volume above 100 was accepted");
    if (strstr(ylvoice_last_error(), "volume") == NULL)
        return fail("invalid volume did not produce a useful error");

    if (ylvoice_play("build/does-not-exist.wav"))
        return fail("missing audio file was accepted");
    if (ylvoice_last_error()[0] == '\0')
        return fail("missing audio file did not report an error");

    ylvoice_shutdown();
    ylvoice_shutdown();
    (void)remove(path);
    puts("Voice runtime tests passed.");
    return 0;
}
