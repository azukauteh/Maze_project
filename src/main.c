#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "engine.h"

#define WINDOW_WIDTH  1024
#define WINDOW_HEIGHT 768

static const char *level_titles[] = {
    "",
    "Maze Raycaster — Level 1: The Corridor  [blue]",
    "Maze Raycaster — Level 2: The Grid      [green]",
    "Maze Raycaster — Level 3: Dead Ends     [red]",
    "Maze Raycaster — Level 4: The Spiral    [purple]",
    "Maze Raycaster — Level 5: Broken Grid   [orange]",
    "Maze Raycaster — Level 6: The Labyrinth [cyan]  FINAL",
};

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        level_titles[1],
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) { SDL_Quit(); return 1; }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) { SDL_DestroyWindow(window); SDL_Quit(); return 1; }

    SDL_Texture *texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING,
        WINDOW_WIDTH, WINDOW_HEIGHT);
    if (!texture) {
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }

    GameState state;
    memset(&state, 0, sizeof(state));
    state.width  = WINDOW_WIDTH;
    state.height = WINDOW_HEIGHT;
    engine_init(&state);

    int max_levels = engine_num_levels();
    printf("W/S move  A/D turn  ESC quit\n");
    printf("Reach the GREEN wall to advance. %d levels total.\n", max_levels);

    bool running = true;
    SDL_Event event;
    const uint8_t *keys = SDL_GetKeyboardState(NULL);

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT: running = false; break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
                    engine_input(&state, event.key.keysym.sym, 1);
                    break;
                case SDL_KEYUP:
                    engine_input(&state, event.key.keysym.sym, 0);
                    break;
            }
        }

        if (keys[SDL_SCANCODE_W]) engine_input(&state, 'W', 1);
        if (keys[SDL_SCANCODE_A]) engine_input(&state, 'A', 1);
        if (keys[SDL_SCANCODE_S]) engine_input(&state, 'S', 1);
        if (keys[SDL_SCANCODE_D]) engine_input(&state, 'D', 1);

        if (state.level_complete) {
            if (state.current_level < max_levels) {
                engine_next_level(&state);
                int lv = state.current_level;
                printf("=== LEVEL %d ===\n", lv);
                if (lv >= 1 && lv <= max_levels)
                    SDL_SetWindowTitle(window, level_titles[lv]);
            } else {
                printf("=== YOU WIN! All %d levels complete. ===\n", max_levels);
                running = false;
            }
        }

        engine_update(&state);
        engine_render(&state);

        void *pixels; int pitch;
        SDL_LockTexture(texture, NULL, &pixels, &pitch);
        memcpy(pixels, state.pixels,
               (size_t)(state.width * state.height) * sizeof(uint32_t));
        SDL_UnlockTexture(texture);

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    if (state.pixels) free(state.pixels);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
