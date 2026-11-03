#ifndef YLANG_VOICE_H
#define YLANG_VOICE_H

#include <stdbool.h>
#include <stdint.h>

/* Optional SDL2_mixer-backed voice/audio playback runtime.
 * The runtime owns the loaded music object until stop/shutdown or next play.
 */
bool ylvoice_init(void);
bool ylvoice_play(const char *path);
void ylvoice_pause(void);
void ylvoice_resume(void);
void ylvoice_stop(void);
bool ylvoice_is_playing(void);
bool ylvoice_set_volume(int64_t percent);
const char *ylvoice_last_error(void);
void ylvoice_shutdown(void);

#endif
