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
typedef struct {
    int64_t stream_id;
    YLVoiceStreamRead read_callback;
    YLVoiceStreamError error_callback;
    YLVoiceStreamClose close_callback;
    unsigned char *pending;
    size_t pending_length;
    size_t pending_capacity;
    uint64_t position;
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

static Sint64 SDLCALL voice_stream_size(SDL_RWops *context)
{
    (void)context;
    return -1;
}

static Sint64 SDLCALL voice_stream_seek(SDL_RWops *context, Sint64 offset, int whence)
{
    if (!context) return -1;
    YLVoiceStreamSource *source =
        (YLVoiceStreamSource *)context->hidden.unknown.data1;
    if (!source) return -1;
    if ((whence == RW_SEEK_CUR && offset == 0) ||
        (whence == RW_SEEK_SET && offset >= 0 &&
         (uint64_t)offset == source->position))
        return (Sint64)source->position;
    return -1;
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
    if (source->pending_length) {
        size_t take = source->pending_length < requested
            ? source->pending_length : requested;
        memcpy(destination, source->pending, take);
        total += take;
        source->pending_length -= take;
        if (source->pending_length)
            memmove(source->pending, source->pending + take, source->pending_length);
    }

    if (size == 1U) {
        if (total < requested) {
            int64_t received = source->read_callback(
                source->stream_id, (unsigned char *)destination + total, requested - total);
            if (received < 0) {
                const char *detail = source->error_callback
                    ? source->error_callback(source->stream_id) : NULL;
                voice_set_error(detail && *detail ? detail : "HTTP audio stream read failed");
            } else if ((uint64_t)received > (uint64_t)(requested - total)) {
                voice_set_error("HTTP audio stream returned an invalid byte count");
            } else {
                total += (size_t)received;
            }
        }
        source->position += (uint64_t)total;
        return total;
    }

    while (total < requested) {
        int64_t received = source->read_callback(
            source->stream_id, (unsigned char *)destination + total, requested - total);
        if (received < 0) {
            const char *detail = source->error_callback
                ? source->error_callback(source->stream_id) : NULL;
            voice_set_error(detail && *detail ? detail : "HTTP audio stream read failed");
            break;
        }
        if (received == 0) break;
        if ((uint64_t)received > (uint64_t)(requested - total)) {
            voice_set_error("HTTP audio stream returned an invalid byte count");
            break;
        }
        total += (size_t)received;
    }

    size_t complete = total / size;
    size_t consumed = complete * size;
    source->position += (uint64_t)consumed;
    size_t remainder = total - consumed;
    if (remainder) {
        if (source->pending_capacity < remainder) {
            unsigned char *grown = realloc(source->pending, remainder);
            if (!grown) {
                voice_set_error("out of memory buffering an audio stream read");
                return complete;
            }
            source->pending = grown;
            source->pending_capacity = remainder;
        }
        memcpy(source->pending, (unsigned char *)destination + consumed, remainder);
        source->pending_length = remainder;
    }
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
        free(source->pending);
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
        voice_set_error(Mix_GetError());
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
