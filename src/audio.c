/*
 * src/audio.c — SDL2_mixer audio subsystem scaffold.
 *
 * When AUDIO_ENABLED is not defined, every function is a documented no-op.
 * When AUDIO_ENABLED is defined, SDL2_mixer is used for real playback.
 *
 * The step-sound accumulator (audio_on_player_move) is always active
 * regardless of AUDIO_ENABLED so callers don't need conditional logic.
 *
 * Thread safety: not thread-safe. All calls must come from the main thread.
 */

#include "audio.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Step accumulator (always active)                                    */
/* ------------------------------------------------------------------ */

static float step_accumulator = 0.0f;

void audio_on_player_move(AudioState *audio, float dist_moved) {
    if (!audio || dist_moved <= 0.0f) return;
    step_accumulator += dist_moved;
    if (step_accumulator >= AUDIO_STEP_INTERVAL) {
        step_accumulator -= AUDIO_STEP_INTERVAL;
        audio_play_sfx(audio, SOUND_STEP);
    }
}

/* ------------------------------------------------------------------ */
/* No-op stub implementation (AUDIO_ENABLED not defined)               */
/* ------------------------------------------------------------------ */

#ifndef AUDIO_ENABLED

int audio_init(AudioState *audio) {
    if (!audio) return 0;
    memset(audio, 0, sizeof(*audio));
    audio->master_vol = 100;
    audio->music_vol  = 80;
    audio->sfx_vol    = 100;
    fprintf(stdout, "audio: compiled without SDL2_mixer — running silent\n");
    return 1; /* "success" — silent mode */
}

void audio_shutdown(AudioState *audio) {
    if (!audio) return;
    memset(audio, 0, sizeof(*audio));
}

int  audio_play_music(AudioState *audio, int level, const char *asset_dir) {
    (void)audio; (void)level; (void)asset_dir; return 0;
}
void audio_stop_music (AudioState *audio) { (void)audio; }
void audio_pause_music(AudioState *audio) { (void)audio; }

int  audio_load_sfx(AudioState *audio, SoundID id, const char *path) {
    (void)audio; (void)id; (void)path; return 0;
}
void audio_play_sfx(AudioState *audio, SoundID id) { (void)audio; (void)id; }

void audio_set_master_vol(AudioState *audio, int vol) {
    if (audio) audio->master_vol = vol;
}
void audio_set_music_vol(AudioState *audio, int vol) {
    if (audio) audio->music_vol = vol;
}
void audio_set_sfx_vol(AudioState *audio, int vol) {
    if (audio) audio->sfx_vol = vol;
}

int audio_is_music_playing(const AudioState *audio) {
    (void)audio; return 0;
}
int audio_is_available(void) { return 0; }

#else /* AUDIO_ENABLED */

/* ------------------------------------------------------------------ */
/* SDL2_mixer implementation                                           */
/* ------------------------------------------------------------------ */

#include <SDL2/SDL_mixer.h>

/* One chunk per SoundID */
static Mix_Chunk *sfx_chunks[SOUND_COUNT];
static Mix_Music *current_music = NULL;

int audio_init(AudioState *audio) {
    if (!audio) return 0;
    memset(audio, 0, sizeof(*audio));
    memset(sfx_chunks, 0, sizeof(sfx_chunks));

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "audio: SDL_InitSubSystem(AUDIO): %s\n",
                SDL_GetError());
        return 0;
    }

    int freq     = 44100;
    int channels = 2;
    int chunksize = 2048;
    if (Mix_OpenAudio(freq, MIX_DEFAULT_FORMAT, channels, chunksize) < 0) {
        fprintf(stderr, "audio: Mix_OpenAudio: %s\n", Mix_GetError());
        return 0;
    }

    Mix_AllocateChannels(16);

    audio->master_vol = 100;
    audio->music_vol  = 80;
    audio->sfx_vol    = 100;
    audio->initialized = 1;
    return 1;
}

void audio_shutdown(AudioState *audio) {
    for (int i = 0; i < SOUND_COUNT; i++) {
        if (sfx_chunks[i]) { Mix_FreeChunk(sfx_chunks[i]); sfx_chunks[i] = NULL; }
    }
    if (current_music) { Mix_FreeMusic(current_music); current_music = NULL; }
    Mix_CloseAudio();
    if (audio) {
        audio->initialized    = 0;
        audio->music_playing  = 0;
    }
}

int audio_play_music(AudioState *audio, int level, const char *asset_dir) {
    if (!audio || !audio->initialized) return 0;
    if (audio->current_music_level == level && audio->music_playing) return 1;

    if (current_music) { Mix_FreeMusic(current_music); current_music = NULL; }

    char path[256];
    snprintf(path, sizeof(path), "%s/music/level%d.ogg",
             asset_dir ? asset_dir : "assets/audio", level);

    current_music = Mix_LoadMUS(path);
    if (!current_music) {
        fprintf(stderr, "audio: Mix_LoadMUS(%s): %s\n", path, Mix_GetError());
        return 0;
    }

    int vol = audio->music_vol * audio->master_vol / 100;
    Mix_VolumeMusic(vol);
    if (Mix_PlayMusic(current_music, -1) < 0) {
        fprintf(stderr, "audio: Mix_PlayMusic: %s\n", Mix_GetError());
        return 0;
    }

    audio->music_playing       = 1;
    audio->current_music_level = level;
    return 1;
}

void audio_stop_music(AudioState *audio) {
    Mix_HaltMusic();
    if (audio) audio->music_playing = 0;
}

void audio_pause_music(AudioState *audio) {
    if (Mix_PlayingMusic()) {
        if (Mix_PausedMusic()) Mix_ResumeMusic();
        else                   Mix_PauseMusic();
    }
    (void)audio;
}

int audio_load_sfx(AudioState *audio, SoundID id, const char *path) {
    if (!audio || !audio->initialized) return 0;
    if (id < 0 || id >= SOUND_COUNT || !path) return 0;

    if (sfx_chunks[id]) { Mix_FreeChunk(sfx_chunks[id]); sfx_chunks[id] = NULL; }
    sfx_chunks[id] = Mix_LoadWAV(path);
    if (!sfx_chunks[id]) {
        fprintf(stderr, "audio: Mix_LoadWAV(%s): %s\n", path, Mix_GetError());
        return 0;
    }
    return 1;
}

void audio_play_sfx(AudioState *audio, SoundID id) {
    if (!audio || !audio->initialized) return;
    if (id < 0 || id >= SOUND_COUNT) return;
    if (!sfx_chunks[id]) return;

    int vol = audio->sfx_vol * audio->master_vol / 100;
    Mix_VolumeChunk(sfx_chunks[id], vol);
    Mix_PlayChannel(-1, sfx_chunks[id], 0);
}

void audio_set_master_vol(AudioState *audio, int vol) {
    if (!audio) return;
    audio->master_vol = vol < 0 ? 0 : vol > 128 ? 128 : vol;
}
void audio_set_music_vol(AudioState *audio, int vol) {
    if (!audio) return;
    audio->music_vol = vol < 0 ? 0 : vol > 128 ? 128 : vol;
    if (audio->initialized && Mix_PlayingMusic())
        Mix_VolumeMusic(audio->music_vol * audio->master_vol / 100);
}
void audio_set_sfx_vol(AudioState *audio, int vol) {
    if (!audio) return;
    audio->sfx_vol = vol < 0 ? 0 : vol > 128 ? 128 : vol;
}

int audio_is_music_playing(const AudioState *audio) {
    (void)audio;
    return Mix_PlayingMusic() && !Mix_PausedMusic();
}
int audio_is_available(void) { return 1; }

#endif /* AUDIO_ENABLED */
