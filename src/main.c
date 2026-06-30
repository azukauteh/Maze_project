#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include "engine.h"

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768

int main(int argc, char *argv[]) {
    /* Initialize SDL */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    
    /* Create window */
    SDL_Window *window = SDL_CreateWindow(
        "Maze Raycaster",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    /* Create renderer */
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    /* Create texture for framebuffer */
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, WINDOW_WIDTH, WINDOW_HEIGHT);
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    /* Initialize game state */
    GameState state;
    state.width = WINDOW_WIDTH;
    state.height = WINDOW_HEIGHT;
    state.pixels = NULL;
    engine_init(&state);
    
    /* Main loop */
    bool running = true;
    SDL_Event event;
    const uint8_t *keys = SDL_GetKeyboardState(NULL);
    
    printf("Maze Raycaster - Use WASD to move, A/D to turn, ESC to quit\n");
    
    while (running) {
        /* Handle events */
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = false;
                    }
                    engine_input(&state, event.key.keysym.sym, 1);
                    break;
                case SDL_KEYUP:
                    engine_input(&state, event.key.keysym.sym, 0);
                    break;
            }
        }
        
        /* Handle continuous key presses */
        if (keys[SDL_SCANCODE_W]) engine_input(&state, 'W', 1);
        if (keys[SDL_SCANCODE_A]) engine_input(&state, 'A', 1);
        if (keys[SDL_SCANCODE_S]) engine_input(&state, 'S', 1);
        if (keys[SDL_SCANCODE_D]) engine_input(&state, 'D', 1);
        
        /* Update and render */
        engine_update(&state);
        engine_render(&state);
        
        /* Copy framebuffer to texture and render */
        void *pixels;
        int pitch;
        SDL_LockTexture(texture, NULL, &pixels, &pitch);
        memcpy(pixels, state.pixels, state.width * state.height * sizeof(uint32_t));
        SDL_UnlockTexture(texture);
        
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }
    
    /* Cleanup */
    if (state.pixels) free(state.pixels);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
