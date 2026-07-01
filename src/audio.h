#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Audio subsystem scaffold — wraps SDL2_mixer for future sound support.
 *
 * Design decisions:
 *   - SDL2_mixer is an OPTIONAL dependency. If it is not present at
 *     compile time (AUDIO_ENABLED not defined), all functions are no-ops
 *     that return 0/NULL. The game compiles and runs silently.
 *   - Sound events are identified by SoundID enum, not by filename strings,
 *     so callers are decoupled from the file layout.
 *   - Music is streamed; effects are short one-shot clips.
 *   - Volume is per-category (master, music, sfx) in range [0, 128].
 *
 * To enable audio at build time, pass -DAUDIO_ENABLED to the compiler and
 * link against SDL2_mixer:
 *   cmake -DAUDIO_ENABLED=ON ..
 *
 * File layout expected under assets/audio/:
 *   music/level1.ogg   music/level2.ogg   ...
 *   sfx/step.wav       sfx/exit.wav       sfx/wall.wav
 */

typedef enum {
    SOUND_STEP      = 0,  /* player footstep */
    SOUND_WALL_HIT  = 1,  /* player walked into a wall */
    SOUND_EXIT      = 2,  /* player reached the exit tile */
    SOUND_LEVEL_UP  = 3,  /* level transition fanfare */
    SOUND_WIN       = 4,  /* all levels complete */
    SOUND_COUNT     = 5
} SoundID;

typedef struct {
    int  master_vol;   /* 0-128 */
    int  music_vol;    /* 0-128 */
    int  sfx_vol;      /* 0-128 */
    int  initialized;
    int  music_playing;
    int  current_music_level;
} AudioState;

/* ---- Lifecycle ---- */
int  audio_init   (AudioState *audio);
void audio_shutdown(AudioState *audio);

/* ---- Music ---- */
int  audio_play_music (AudioState *audio, int level, const char *asset_dir);
void audio_stop_music (AudioState *audio);
void audio_pause_music(AudioState *audio);

/* ---- SFX ---- */
int  audio_load_sfx(AudioState *audio, SoundID id, const char *path);
void audio_play_sfx(AudioState *audio, SoundID id);

/* ---- Volume ---- */
void audio_set_master_vol(AudioState *audio, int vol);
void audio_set_music_vol (AudioState *audio, int vol);
void audio_set_sfx_vol   (AudioState *audio, int vol);

/* ---- Query ---- */
int audio_is_music_playing(const AudioState *audio);
int audio_is_available    (void); /* returns 1 if AUDIO_ENABLED at compile time */

/* ---- Step trigger ---- */
/*
 * audio_on_player_move — call once per frame with the player's moved
 * distance (pixels). Triggers a step sound every AUDIO_STEP_INTERVAL pixels.
 */
#define AUDIO_STEP_INTERVAL 64.0f
void audio_on_player_move(AudioState *audio, float dist_moved);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_H */
