#ifndef YLANG_VOICE_H
#define YLANG_VOICE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* Optional SDL2_mixer-backed voice/audio playback runtime.
 * The runtime owns the loaded music object until stop/shutdown or next play.
 * Serialize all lifecycle/playback calls on one owner thread. Stream callbacks
 * may run on SDL_mixer's audio thread; they must be thread-safe and must not
 * call Voice lifecycle/playback functions. ylvoice_last_error() is thread-safe.
 */
bool ylvoice_init(void);
bool ylvoice_play(const char *path);
typedef int64_t (*YLVoiceStreamRead)(int64_t stream, void *destination, size_t max_bytes);
typedef const char *(*YLVoiceStreamError)(int64_t stream);
typedef void (*YLVoiceStreamClose)(int64_t stream);
/* Takes ownership of stream_id; the callbacks must remain valid until close. */
bool ylvoice_play_stream(int64_t stream_id, YLVoiceStreamRead read_callback,
                         YLVoiceStreamError error_callback,
                         YLVoiceStreamClose close_callback);
void ylvoice_pause(void);
void ylvoice_resume(void);
void ylvoice_stop(void);
bool ylvoice_is_playing(void);
bool ylvoice_is_paused(void);
bool ylvoice_is_finished(void);
bool ylvoice_set_volume(int64_t percent);
/* Returns a per-thread snapshot, valid until the next call on the same thread. */
const char *ylvoice_last_error(void);
void ylvoice_shutdown(void);

#endif
