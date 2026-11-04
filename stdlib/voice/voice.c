#include "ylang/voice.h"

#include <SDL.h>
#include <SDL_mixer.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool voice_initialized;
static Mix_Music *voice_music;
static SDL_RWops *voice_stream_rwops;
static bool voice_track_completed;
static char voice_error[256];

typedef struct {
    int64_t stream_id;
    YLVoiceStreamRead read_callback;
    YLVoiceStreamError error_callback;
    YLVoiceStreamClose close_callback;
    FILE *cache;
    uint64_t cached_length;
    uint64_t position;
    bool eof;
} YLVoiceStreamSource;

static void voice_set_error(const char *message);

#define YL_VOICE_STREAM_CACHE_CHUNK (16U * 1024U)

static bool voice_stream_cache_seek(FILE *file, uint64_t offset)
{
    if (!file || offset > (uint64_t)LONG_MAX) return false;
    return fseek(file, (long)offset, SEEK_SET) == 0;
}

/* Pull another bounded block from HTTP and append it to the seekable disk
   cache. MP3 decoders can rewind while probing metadata or starting playback. */
static bool voice_stream_cache_more(YLVoiceStreamSource *source, size_t limit)
{
    unsigned char bytes[YL_VOICE_STREAM_CACHE_CHUNK];
    if (!source || !source->cache || !source->read_callback || source->eof)
        return false;

    size_t request = limit;
    if (request == 0U || request > sizeof(bytes)) request = sizeof(bytes);
    int64_t received = source->read_callback(source->stream_id, bytes, request);
    if (received < 0) {
        const char *detail = source->error_callback
            ? source->error_callback(source->stream_id) : NULL;
        voice_set_error(detail && *detail ? detail : "HTTP audio stream read failed");
        return false;
    }
    if (received == 0) {
        source->eof = true;
        return false;
    }
    if ((uint64_t)received > (uint64_t)request ||
        source->cached_length > (uint64_t)LONG_MAX ||
        (uint64_t)received > (uint64_t)LONG_MAX - source->cached_length) {
        voice_set_error("HTTP audio stream exceeded the seekable cache limit");
        return false;
    }
    if (!voice_stream_cache_seek(source->cache, source->cached_length) ||
        fwrite(bytes, 1U, (size_t)received, source->cache) != (size_t)received ||
        fflush(source->cache) != 0) {
        voice_set_error("could not write the seekable HTTP audio cache");
        return false;
    }
    source->cached_length += (uint64_t)received;
    return true;
}

static bool voice_stream_cache_until(YLVoiceStreamSource *source, uint64_t target)
{
    while (source && source->cached_length < target && !source->eof) {
        uint64_t missing = target - source->cached_length;
        size_t request = missing > (uint64_t)YL_VOICE_STREAM_CACHE_CHUNK
            ? YL_VOICE_STREAM_CACHE_CHUNK : (size_t)missing;
        if (!voice_stream_cache_more(source, request)) break;
    }
    return source && source->cached_length >= target;
}

static bool voice_stream_add_offset(uint64_t base, Sint64 offset, uint64_t *target)
{
    if (!target) return false;
    if (offset < 0) {
        uint64_t magnitude = (uint64_t)(-(offset + 1)) + 1U;
        if (magnitude > base) return false;
        *target = base - magnitude;
        return true;
    }
    uint64_t delta = (uint64_t)offset;
    if (delta > UINT64_MAX - base) return false;
    *target = base + delta;
    return true;
}

static void voice_set_error(const char *message)
{
    (void)snprintf(voice_error, sizeof(voice_error), "%s",
                   message && *message ? message : "unknown audio error");
}

static void voice_release_music(void)
{
    if (voice_music) {
        Mix_FreeMusic(voice_music);
        voice_music = NULL;
    }
    if (voice_stream_rwops) {
        (void)SDL_RWclose(voice_stream_rwops);
        voice_stream_rwops = NULL;
    }
}

static Sint64 SDLCALL voice_stream_size(SDL_RWops *context)
{
    if (!context) return -1;
    YLVoiceStreamSource *source =
        (YLVoiceStreamSource *)context->hidden.unknown.data1;
    if (!source || !source->eof || source->cached_length > (uint64_t)INT64_MAX)
        return -1;
    return (Sint64)source->cached_length;
}

static Sint64 SDLCALL voice_stream_seek(SDL_RWops *context, Sint64 offset, int whence)
{
    if (!context) return -1;
    YLVoiceStreamSource *source =
        (YLVoiceStreamSource *)context->hidden.unknown.data1;
    if (!source || !source->cache) return -1;

    uint64_t target = 0U;
    if (whence == RW_SEEK_SET) {
        if (offset < 0) return -1;
        target = (uint64_t)offset;
    } else if (whence == RW_SEEK_CUR) {
        if (!voice_stream_add_offset(source->position, offset, &target)) return -1;
    } else if (whence == RW_SEEK_END) {
        while (!source->eof) {
            if (!voice_stream_cache_more(source, YL_VOICE_STREAM_CACHE_CHUNK) &&
                !source->eof) return -1;
        }
        if (!voice_stream_add_offset(source->cached_length, offset, &target)) return -1;
    } else {
        return -1;
    }

    if (target > (uint64_t)INT64_MAX || target > (uint64_t)LONG_MAX) return -1;
    if (target > source->cached_length &&
        !voice_stream_cache_until(source, target)) return -1;
    source->position = target;
    return (Sint64)target;
}

static size_t SDLCALL voice_stream_read(SDL_RWops *context, void *destination,
                                        size_t size, size_t maxnum)
{
    if (!context || !destination || size == 0U || maxnum == 0U ||
        maxnum > SIZE_MAX / size) return 0U;
    YLVoiceStreamSource *source =
        (YLVoiceStreamSource *)context->hidden.unknown.data1;
    if (!source || !source->cache || !source->read_callback) return 0U;

    size_t requested = size * maxnum;
    size_t total = 0U;
    uint64_t read_position = source->position;
    while (total < requested) {
        if (read_position < source->cached_length) {
            uint64_t available64 = source->cached_length - read_position;
            size_t take = requested - total;
            if (available64 < (uint64_t)take) take = (size_t)available64;
            if (!voice_stream_cache_seek(source->cache, read_position)) {
                voice_set_error("could not seek within the cached HTTP audio stream");
                break;
            }
            size_t got = fread((unsigned char *)destination + total, 1U,
                               take, source->cache);
            if (got == 0U) {
                voice_set_error("could not read the cached HTTP audio stream");
                break;
            }
            total += got;
            read_position += (uint64_t)got;
            continue;
        }
        if (source->eof) break;
        size_t remaining = requested - total;
        size_t request = remaining > YL_VOICE_STREAM_CACHE_CHUNK
            ? YL_VOICE_STREAM_CACHE_CHUNK : remaining;
        if (!voice_stream_cache_more(source, request)) break;
    }

    size_t complete = total / size;
    size_t consumed = complete * size;
    source->position += (uint64_t)consumed;
    return complete;
}
static size_t SDLCALL voice_stream_write(SDL_RWops *context, const void *source,
                                         size_t size, size_t num)
{
    (void)context;
    (void)source;
    (void)size;
    (void)num;
    return 0U;
}

static int SDLCALL voice_stream_close(SDL_RWops *context)
{
    if (!context) return 0;
    YLVoiceStreamSource *source =
        (YLVoiceStreamSource *)context->hidden.unknown.data1;
    if (source) {
        if (source->close_callback)
            source->close_callback(source->stream_id);
        if (source->cache) (void)fclose(source->cache);
        free(source);
    }
    SDL_FreeRW(context);
    return 0;
}

static SDL_RWops *voice_create_stream_rwops(int64_t stream_id,
                                             YLVoiceStreamRead read_callback,
                                             YLVoiceStreamError error_callback,
                                             YLVoiceStreamClose close_callback)
{
    YLVoiceStreamSource *source = calloc(1U, sizeof(*source));
    if (!source) return NULL;
    source->stream_id = stream_id;
    source->read_callback = read_callback;
    source->error_callback = error_callback;
    source->close_callback = close_callback;
    source->cache = tmpfile();
    if (!source->cache) {
        free(source);
        return NULL;
    }

    SDL_RWops *rwops = SDL_AllocRW();
    if (!rwops) {
        (void)fclose(source->cache);
        free(source);
        return NULL;
    }
    memset(rwops, 0, sizeof(*rwops));
    rwops->size = voice_stream_size;
    rwops->seek = voice_stream_seek;
    rwops->read = voice_stream_read;
    rwops->write = voice_stream_write;
    rwops->close = voice_stream_close;
    rwops->type = SDL_RWOPS_UNKNOWN;
    rwops->hidden.unknown.data1 = source;
    rwops->hidden.unknown.data2 = NULL;
    return rwops;
}

bool ylvoice_init(void)
{
    if (voice_initialized) return true;
    voice_error[0] = '\0';
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        voice_set_error(SDL_GetError());
        return false;
    }
    (void)Mix_Init(MIX_INIT_MP3 | MIX_INIT_OGG | MIX_INIT_FLAC);
    if (Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 2048) != 0) {
        voice_set_error(Mix_GetError());
        Mix_Quit();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    voice_initialized = true;
    return true;
}

bool ylvoice_play(const char *path)
{
    if (!voice_initialized && !ylvoice_init()) return false;
    if (!path || !*path) {
        voice_set_error("audio path must not be empty");
        return false;
    }
    Mix_Music *next = Mix_LoadMUS(path);
    if (!next) {
        voice_set_error(Mix_GetError());
        return false;
    }
    Mix_HaltMusic();
    voice_release_music();
    if (Mix_PlayMusic(next, 0) != 0) {
        voice_set_error(Mix_GetError());
        Mix_FreeMusic(next);
        return false;
    }
    voice_music = next;
    voice_track_completed = false;
    voice_error[0] = '\0';
    return true;
}

bool ylvoice_play_stream(int64_t stream_id, YLVoiceStreamRead read_callback,
                         YLVoiceStreamError error_callback,
                         YLVoiceStreamClose close_callback)
{
    if (!voice_initialized && !ylvoice_init()) {
        if (close_callback) close_callback(stream_id);
        return false;
    }
    if (stream_id <= 0 || !read_callback || !close_callback) {
        voice_set_error("invalid HTTP audio stream callbacks or handle");
        if (close_callback) close_callback(stream_id);
        return false;
    }
    SDL_RWops *rwops = voice_create_stream_rwops(
        stream_id, read_callback, error_callback, close_callback);
    if (!rwops) {
        voice_set_error("could not allocate an HTTP audio stream reader");
        close_callback(stream_id);
        return false;
    }
    /* Keep ownership of RWops explicit: Mix_FreeMusic does not own it when
       freesrc is zero, so we close it only after the decoder is halted/freed. */
    Mix_Music *next = Mix_LoadMUS_RW(rwops, 0);
    if (!next) {
        voice_set_error(Mix_GetError());
        (void)SDL_RWclose(rwops);
        return false;
    }
    Mix_HaltMusic();
    voice_release_music();
    if (Mix_PlayMusic(next, 0) != 0) {
        voice_set_error(Mix_GetError());
        Mix_FreeMusic(next);
        (void)SDL_RWclose(rwops);
        return false;
    }
    voice_music = next;
    voice_stream_rwops = rwops;
    voice_track_completed = false;
    voice_error[0] = '\0';
    return true;
}

void ylvoice_pause(void) { if (voice_initialized) Mix_PauseMusic(); }
void ylvoice_resume(void) { if (voice_initialized) Mix_ResumeMusic(); }

void ylvoice_stop(void)
{
    if (!voice_initialized) return;
    Mix_HaltMusic();
    voice_release_music();
    voice_track_completed = false;
}

static void voice_release_if_finished(void)
{
    if (voice_music && Mix_PlayingMusic() == 0 && Mix_PausedMusic() == 0) {
        voice_release_music();
        voice_track_completed = true;
    }
}

bool ylvoice_is_playing(void)
{
    voice_release_if_finished();
    return voice_initialized && voice_music != NULL &&
           Mix_PlayingMusic() != 0 && Mix_PausedMusic() == 0;
}

bool ylvoice_is_paused(void)
{
    voice_release_if_finished();
    return voice_initialized && voice_music != NULL && Mix_PausedMusic() != 0;
}

bool ylvoice_is_finished(void)
{
    voice_release_if_finished();
    return voice_track_completed;
}

bool ylvoice_set_volume(int64_t percent)
{
    if (percent < 0 || percent > 100) {
        voice_set_error("volume must be between 0 and 100");
        return false;
    }
    if (!voice_initialized && !ylvoice_init()) return false;
    (void)Mix_VolumeMusic((int)((percent * MIX_MAX_VOLUME) / 100));
    voice_error[0] = '\0';
    return true;
}

const char *ylvoice_last_error(void)
{
    return voice_error[0] ? voice_error : "No error";
}

void ylvoice_shutdown(void)
{
    if (!voice_initialized) return;
    ylvoice_stop();
    Mix_CloseAudio();
    Mix_Quit();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    voice_initialized = false;
}
