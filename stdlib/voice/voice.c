#include "ylang/voice.h"

#include <SDL.h>
#include <SDL_mixer.h>
#include <stdio.h>
#include <string.h>

static bool voice_initialized;
static Mix_Music *voice_music;
static bool voice_track_completed;
static char voice_error[256];

static void voice_set_error(const char *message)
{
    (void)snprintf(voice_error, sizeof(voice_error), "%s",
                   message && *message ? message : "unknown audio error");
}

bool ylvoice_init(void)
{
    if (voice_initialized) return true;
    voice_error[0] = '\0';
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        voice_set_error(SDL_GetError());
        return false;
    }
    /* SDL_mixer initializes only codecs that are available in this build. */
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

static void voice_release_if_finished(void)
{
    if (voice_music && Mix_PlayingMusic() == 0 && Mix_PausedMusic() == 0) {
        Mix_FreeMusic(voice_music);
        voice_music = NULL;
        voice_track_completed = true;
    }
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
    if (Mix_PlayMusic(next, 0) != 0) {
        voice_set_error(Mix_GetError());
        Mix_FreeMusic(next);
        return false;
    }
    if (voice_music) Mix_FreeMusic(voice_music);
    voice_music = next;
    voice_error[0] = '\0';
    return true;
}

void ylvoice_pause(void) { if (voice_initialized) Mix_PauseMusic(); }
void ylvoice_resume(void) { if (voice_initialized) Mix_ResumeMusic(); }

void ylvoice_stop(void)
{
    if (!voice_initialized) return;
    Mix_HaltMusic();
    if (voice_music) {
        Mix_FreeMusic(voice_music);
        voice_music = NULL;
    }
    voice_track_completed = false;
}

bool ylvoice_is_playing(void)
{
    voice_release_if_finished();
    return voice_initialized && voice_music != NULL && Mix_PlayingMusic() != 0;
}

bool ylvoice_is_paused(void)
{
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
