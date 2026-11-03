#include "ylang/voice.h"

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
    const unsigned sample_count = sample_rate * 2U;
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
    ylvoice_resume();
    ylvoice_stop();
    if (ylvoice_is_playing()) return fail("stop did not stop playback");

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
