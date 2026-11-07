#include "ylang/voice.h"

#include <SDL.h>
#include <SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool voice_initialized;
static Mix_Music *voice_music;
static SDL_RWops *voice_stream_rwops;
static bool voice_track_completed;
static char voice_error[256];
static SDL_SpinLock voice_error_lock;

static void voice_clear_error(void)
{
    SDL_AtomicLock(&voice_error_lock);
    voice_error[0] = '\0';
    SDL_AtomicUnlock(&voice_error_lock);
}

static void voice_set_error_if_empty(const char *message)
{
    SDL_AtomicLock(&voice_error_lock);
    if (!voice_error[0]) {
        (void)snprintf(voice_error, sizeof(voice_error), "%s",
                       message && *message ? message : "unknown audio error");
    }
    SDL_AtomicUnlock(&voice_error_lock);
}
#define VOICE_STREAM_CACHE_LIMIT (256U * 1024U * 1024U)
#define VOICE_STREAM_READ_CHUNK (64U * 1024U)

typedef struct {
    int64_t stream_id;
    YLVoiceStreamRead read_callback;
    YLVoiceStreamError error_callback;
    YLVoiceStreamClose close_callback;
    /* SDL_mixer decoders such as WAV seek past the data chunk while parsing
       headers and then rewind before playback. Keep fetched bytes so those
       decoders work with a forward-only HTTP callback without losing audio. */
    unsigned char *cache;
    size_t cache_length;
    size_t cache_capacity;
    uint64_t position;
    bool reached_eof;
} YLVoiceStreamSource;

static void voice_set_error(const char *message)
{
    SDL_AtomicLock(&voice_error_lock);
    (void)snprintf(voice_error, sizeof(voice_error), "%s",
                   message && *message ? message : "unknown audio error");
    SDL_AtomicUnlock(&voice_error_lock);
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

static bool voice_stream_cache_reserve(YLVoiceStreamSource *source, size_t extra)
{
    if (extra > SIZE_MAX - source->cache_length) {
        voice_set_error("audio stream size overflow");
        return false;
    }
    size_t needed = source->cache_length + extra;
    if (needed > VOICE_STREAM_CACHE_LIMIT) {
        voice_set_error("audio stream exceeds the 256 MiB seek buffer limit");
        return false;
    }
    if (needed <= source->cache_capacity) return true;
    size_t grown = source->cache_capacity ? source->cache_capacity : VOICE_STREAM_READ_CHUNK;
    while (grown < needed) {
        if (grown > VOICE_STREAM_CACHE_LIMIT / 2U) {
            grown = VOICE_STREAM_CACHE_LIMIT;
            break;
        }
        grown *= 2U;
    }
    unsigned char *cache = realloc(source->cache, grown);
    if (!cache) {
        voice_set_error("out of memory buffering an audio stream");
        return false;
    }
    source->cache = cache;
    source->cache_capacity = grown;
    return true;
}

/* Fetch at least `wanted` bytes when possible. Returns the number of newly
   buffered bytes, or 0 at EOF/on failure after recording the diagnostic. */
static size_t voice_stream_fill(YLVoiceStreamSource *source, size_t wanted)
{
    size_t added = 0U;
    while (added < wanted) {
        size_t chunk = wanted - added;
        if (chunk > VOICE_STREAM_READ_CHUNK) chunk = VOICE_STREAM_READ_CHUNK;
        if (!voice_stream_cache_reserve(source, chunk)) break;
        int64_t received = source->read_callback(
            source->stream_id, source->cache + source->cache_length, chunk);
        if (received < 0) {
            const char *detail = source->error_callback
                ? source->error_callback(source->stream_id) : NULL;
            voice_set_error(detail && *detail ? detail : "HTTP audio stream read failed");
            break;
        }
        if ((uint64_t)received > (uint64_t)chunk) {
            voice_set_error("HTTP audio stream returned an invalid byte count");
            break;
        }
        if (received == 0) {
            source->reached_eof = true;
            break;
        }
        source->cache_length += (size_t)received;
        added += (size_t)received;
    }
    return added;
}

static Sint64 SDLCALL voice_stream_size(SDL_RWops *context)
{
    if (!context) return -1;
    YLVoiceStreamSource *source =
        (YLVoiceStreamSource *)context->hidden.unknown.data1;
    if (!source || !source->reached_eof) return -1;
    return (Sint64)source->cache_length;
}

static Sint64 SDLCALL voice_stream_seek(SDL_RWops *context, Sint64 offset, int whence)
{
    if (!context) return -1;
    YLVoiceStreamSource *source =
        (YLVoiceStreamSource *)context->hidden.unknown.data1;
    if (!source || !source->read_callback) return -1;

    uint64_t base;
    switch (whence) {
        case RW_SEEK_SET:
            base = 0U;
            break;
        case RW_SEEK_CUR:
            base = source->position;
            break;
        case RW_SEEK_END:
            /* Streaming sources have no declared length. Drain once so decoders
               that measure before playback still see a stable end position. */
            while (!source->reached_eof) {
                if (voice_stream_fill(source, VOICE_STREAM_READ_CHUNK) == 0U)
                    break;
            }
            if (!source->reached_eof) return -1;
            base = (uint64_t)source->cache_length;
            break;
        default:
            return -1;
    }

    uint64_t target;
    if (offset < 0) {
        uint64_t magnitude = (uint64_t)(-(offset + 1)) + 1U;
        if (magnitude > base) return -1;
        target = base - magnitude;
    } else {
        uint64_t magnitude = (uint64_t)offset;
        if (magnitude > UINT64_MAX - base) return -1;
        target = base + magnitude;
    }

    if (target > source->cache_length) {
        size_t wanted = (size_t)(target - source->cache_length);
        (void)voice_stream_fill(source, wanted);
        if (target > source->cache_length) return -1;
    }
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
    if (!source || !source->read_callback) return 0U;

    size_t requested = size * maxnum;
    size_t total = 0U;
    uint64_t start = source->position;
    while (total < requested) {
        if (source->position < source->cache_length) {
            size_t available = (size_t)(source->cache_length - source->position);
            size_t take = available < requested - total ? available : requested - total;
            memcpy((unsigned char *)destination + total,
                   source->cache + (size_t)source->position, take);
            source->position += (uint64_t)take;
            total += take;
            continue;
        }
        if (source->reached_eof) break;
        if (voice_stream_fill(source, requested - total) == 0U) break;
    }

    /* SDL_RWread returns whole objects. Rewind any trailing partial object so
       the next call can complete it from the cache. */
    size_t complete = total / size;
    size_t consumed = complete * size;
    source->position = start + (uint64_t)consumed;
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
        free(source->cache);
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

    SDL_RWops *rwops = SDL_AllocRW();
    if (!rwops) {
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
    voice_clear_error();
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
    voice_clear_error();
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
    /* A failed stream callback may provide a more useful transport error than
       SDL_mixer's generic decoder error. Start clean and preserve that detail. */
    voice_clear_error();
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
        voice_set_error_if_empty(Mix_GetError());
        (void)SDL_RWclose(rwops);
        return false;
    }
    Mix_HaltMusic();
    voice_release_music();
    if (Mix_PlayMusic(next, 0) != 0) {
        voice_set_error_if_empty(Mix_GetError());
        Mix_FreeMusic(next);
        (void)SDL_RWclose(rwops);
        return false;
    }
    voice_music = next;
    voice_stream_rwops = rwops;
    voice_track_completed = false;
    /* Do not clear here: the decoder may already have reported a stream
       callback failure while Mix_PlayMusic was starting playback. */
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
    return voice_initialized && voice_music != NULL && Mix_PlayingMusic() != 0;
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
    voice_clear_error();
    return true;
}

const char *ylvoice_last_error(void)
{
    static _Thread_local char snapshot[sizeof(voice_error)];
    SDL_AtomicLock(&voice_error_lock);
    memcpy(snapshot, voice_error, sizeof(snapshot));
    SDL_AtomicUnlock(&voice_error_lock);
    return snapshot[0] ? snapshot : "No error";
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
