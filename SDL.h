#ifndef SDL_H
#define SDL_H

#include <string.h>

/* Define constants and types */
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define BITMAP_WIDTH 32
#define BITMAP_HEIGHT 32
#define COLOR_WHITE 0xFFFFFF
#define COLOR_BLACK 0x000000

/* @typedef struct: A custom structure that holds window and renderer.
 * @SDL_Window: *window
 * @SDL_Renderer: *renderer
 *
 */
typedef struct 
{
SDL_Window *window;
SDL_Renderer *renderer;
} SDLContext;

/* Function prototypes */
SDL_Surface *create_screen(void);
SDL_Surface *create_bitmap(void);
void draw_bitmap(SDL_Surface *, SDL_Surface*);
void draw_text(SDL_Surface *, char*, int, int);
void drawMap2D();
float degToRad(int a);
int FixAng(int a);
void drawPlayer2D();
void Buttons(unsigned char key, int x, int y);
float distance(ax, ay, bx, by, ang);
void drawRays2D();
void init();
void display();
int main(int argc, char* argv[]);

#endif /* SDL_H */
