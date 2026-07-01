/*
 * Maze_game.c — OpenGL/GLUT reference implementation
 *
 * This file is the original raycasting prototype from which the SDL2 engine
 * in src/ was derived. It uses OpenGL immediate mode (glBegin/glEnd) and GLUT
 * for windowing. It is kept here as a reference and is NOT part of the CMake
 * build. The SDL2 port in src/engine.c preserves the same DDA ray-grid
 * intersection algorithm and fisheye correction but replaces all GL calls with
 * a software framebuffer written to an SDL2 streaming texture.
 *
 * Algorithm overview
 * ------------------
 * The renderer casts 60 rays spanning a 60-degree horizontal FOV (30 degrees
 * either side of the player's facing angle). For each ray it finds the nearest
 * wall hit using two separate DDA passes:
 *
 *   1. Vertical pass — steps along vertical grid lines (x = n*64).
 *      Given ray angle ra and player position (px,py):
 *        rx = next vertical grid line
 *        ry = px * tan(ra) + py          (intersection y on that line)
 *        step xo=64, yo=-xo*tan(ra)      (step to next vertical line)
 *
 *   2. Horizontal pass — steps along horizontal grid lines (y = n*64).
 *        ry = next horizontal grid line
 *        rx = py * (1/tan(ra)) + px
 *        step yo=64, xo=-yo*(1/tan(ra))
 *
 * The closer of the two hits is used. Projected distance:
 *   dist = cos(ra)*(rx-px) - sin(ra)*(ry-py)
 *
 * Fisheye correction multiplies by cos(player_angle - ray_angle) so that
 * rays at the edge of the FOV are not stretched.
 *
 * Wall slice height:
 *   lineH = (MAP_SCALE * screen_height) / corrected_dist
 *
 * Original source: youtube.com/3DSage (used as learning reference).
 * All subsequent development is in src/.
 */

#include <stdlib.h>
#include <GL/glut.h>
#include <math.h>

/* ----------------------------- MAP ---------------------------------------- */
#define mapX  8      /* map width  in tiles */
#define mapY  8      /* map height in tiles */
#define mapS 64      /* tile side length in pixels */

/*
 * Map array. 1 = wall, 0 = open floor.
 * Outer ring must remain walls; inner cells are the maze.
 * Edit inner cells to create different layouts. Indexing: map[row*mapX+col].
 */
int map[] = {
    1,1,1,1,1,1,1,1,
    1,0,1,0,0,0,0,1,
    1,0,1,0,0,0,0,1,
    1,0,1,0,0,0,0,1,
    1,0,0,0,0,0,0,1,
    1,0,0,0,0,1,0,1,
    1,0,0,0,0,0,0,1,
    1,1,1,1,1,1,1,1,
};

/*
 * drawMap2D — renders the top-down 2D tile grid in the left panel.
 * Each tile is drawn as an axis-aligned quad with a 1-pixel inset border so
 * adjacent tiles are visually separated.
 */
void drawMap2D(void)
{
    int x, y, xo, yo;
    for (y = 0; y < mapY; y++) {
        for (x = 0; x < mapX; x++) {
            /* White for wall tile, black for open tile */
            if (map[y * mapX + x] == 1)
                glColor3f(1, 1, 1);
            else
                glColor3f(0, 0, 0);

            xo = x * mapS;
            yo = y * mapS;
            glBegin(GL_QUADS);
            glVertex2i(0    + xo + 1, 0    + yo + 1);
            glVertex2i(0    + xo + 1, mapS + yo - 1);
            glVertex2i(mapS + xo - 1, mapS + yo - 1);
            glVertex2i(mapS + xo - 1, 0    + yo + 1);
            glEnd();
        }
    }
}

/* ----------------------------- PLAYER ------------------------------------- */

float degToRad(int a) { return a * M_PI / 180.0; }

/*
 * FixAng — wraps an integer angle into the range [0, 359].
 * Used after every rotation to avoid negative or >360 angles.
 */
int FixAng(int a)
{
    if (a > 359) a -= 360;
    if (a < 0)   a += 360;
    return a;
}

float px, py;   /* player world-space position (pixels) */
float pdx, pdy; /* unit direction vector derived from pa */
float pa;       /* player angle in integer degrees */

/*
 * drawPlayer2D — draws the player as a yellow dot with a direction line
 * in the 2D top-down view.
 */
void drawPlayer2D(void)
{
    glColor3f(1, 1, 0);
    glPointSize(8);
    glLineWidth(4);
    glBegin(GL_POINTS); glVertex2i(px, py); glEnd();
    glBegin(GL_LINES);
    glVertex2i(px, py);
    glVertex2i(px + pdx * 20, py + pdy * 20);
    glEnd();
}

/*
 * Buttons — GLUT keyboard callback.
 * a/d rotate the player by 5 degrees and recompute direction vector.
 * w/s move forward/backward along that vector.
 * glutPostRedisplay schedules a re-render on the next event loop tick.
 *
 * Note: no collision detection in this reference. The SDL2 port in
 * src/engine.c adds tile-boundary checking before updating position.
 */
void Buttons(unsigned char key, int x, int y)
{
    if (key == 'a') { pa += 5; pa = FixAng(pa); pdx = cos(degToRad(pa)); pdy = -sin(degToRad(pa)); }
    if (key == 'd') { pa -= 5; pa = FixAng(pa); pdx = cos(degToRad(pa)); pdy = -sin(degToRad(pa)); }
    if (key == 'w') { px += pdx * 5; py += pdy * 5; }
    if (key == 's') { px -= pdx * 5; py -= pdy * 5; }
    glutPostRedisplay();
}

/* ----------------------------- RAYCASTING --------------------------------- */

/*
 * distance — projected distance from point (ax,ay) to point (bx,by) along
 * ray direction ang (degrees). This is the perpendicular (eye-plane) distance,
 * not the Euclidean distance, so it already accounts for the viewing projection.
 *
 *   d = cos(ang)*(bx-ax) - sin(ang)*(by-ay)
 *
 * Equivalent to the dot product of the displacement vector with the ray
 * direction unit vector.
 */
float distance(float ax, float ay, float bx, float by, int ang)
{
    return cos(degToRad(ang)) * (bx - ax) - sin(degToRad(ang)) * (by - ay);
}

/*
 * drawRays2D — the core raycasting function.
 *
 * Renders two things:
 *   (a) The 2D ray lines in the top-down panel (left side of the window).
 *   (b) The 3D wall slices in the right panel (columns of GL_LINES).
 *
 * Sky (cyan) and floor (blue) quads are drawn first as the 3D background.
 * Then for each of 60 rays the nearest wall hit is found and a vertical line
 * is drawn whose height is inversely proportional to corrected distance.
 * Darker green = vertical wall face. Brighter green = horizontal wall face.
 *
 * The right panel starts at x=526 (pixel) and each ray column is 8px wide,
 * giving 60*8=480px wide 3D view on a 1024-wide window.
 */
void drawRays2D(void)
{
    /* Sky and floor background quads */
    glColor3f(0, 1, 1);  /* cyan sky */
    glBegin(GL_QUADS);
    glVertex2i(526,   0); glVertex2i(1006,   0);
    glVertex2i(1006, 160); glVertex2i(526, 160);
    glEnd();

    glColor3f(0, 0, 1);  /* blue floor */
    glBegin(GL_QUADS);
    glVertex2i(526, 160); glVertex2i(1006, 160);
    glVertex2i(1006, 320); glVertex2i(526, 320);
    glEnd();

    int r, mx, my, mp, dof;
    float vx, vy, rx, ry, ra, xo, yo, disV, disH;

    ra = FixAng(pa + 30);  /* start ray 30 degrees left of player facing */

    for (r = 0; r < 60; r++) {

        /* ---- Vertical grid intersection pass ---- */
        dof = 0; disV = 100000;
        float Tan = tan(degToRad(ra));

        if (cos(degToRad(ra)) > 0.001f) {
            /* Ray faces right — first vertical line is one tile to the right */
            rx = (((int)px >> 6) << 6) + 64;
            ry = (px - rx) * Tan + py;
            xo =  64; yo = -xo * Tan;
        } else if (cos(degToRad(ra)) < -0.001f) {
            /* Ray faces left — step back inside current tile */
            rx = (((int)px >> 6) << 6) - 0.0001f;
            ry = (px - rx) * Tan + py;
            xo = -64; yo = -xo * Tan;
        } else {
            /* Ray is exactly vertical — no vertical grid hits possible */
            rx = px; ry = py; dof = 8;
        }

        while (dof < 8) {
            mx = (int)rx >> 6;
            my = (int)ry >> 6;
            mp = my * mapX + mx;
            if (mp > 0 && mp < mapX * mapY && map[mp] == 1) {
                dof = 8;
                disV = distance(px, py, rx, ry, ra);
            } else {
                rx += xo; ry += yo; dof++;
            }
        }
        vx = rx; vy = ry;

        /* ---- Horizontal grid intersection pass ---- */
        dof = 0; disH = 100000;
        Tan = 1.0f / Tan;

        if (sin(degToRad(ra)) > 0.001f) {
            /* Ray faces up */
            ry = (((int)py >> 6) << 6) - 0.0001f;
            rx = (py - ry) * Tan + px;
            yo = -64; xo = -yo * Tan;
        } else if (sin(degToRad(ra)) < -0.001f) {
            /* Ray faces down */
            ry = (((int)py >> 6) << 6) + 64;
            rx = (py - ry) * Tan + px;
            yo =  64; xo = -yo * Tan;
        } else {
            rx = px; ry = py; dof = 8;
        }

        while (dof < 8) {
            mx = (int)rx >> 6;
            my = (int)ry >> 6;
            mp = my * mapX + mx;
            if (mp > 0 && mp < mapX * mapY && map[mp] == 1) {
                dof = 8;
                disH = distance(px, py, rx, ry, ra);
            } else {
                rx += xo; ry += yo; dof++;
            }
        }

        /* ---- Pick closest hit and shade ---- */
        glColor3f(0, 0.8f, 0);           /* bright green = horizontal face */
        if (disV < disH) {
            rx = vx; ry = vy;
            disH = disV;
            glColor3f(0, 0.6f, 0);       /* darker green = vertical face */
        }

        /* Draw 2D ray line in top-down panel */
        glLineWidth(2);
        glBegin(GL_LINES);
        glVertex2i(px, py);
        glVertex2i(rx, ry);
        glEnd();

        /* Fisheye correction: multiply by cos of angle delta */
        int ca = FixAng(pa - ra);
        disH = disH * cos(degToRad(ca));

        /* Wall slice height inversely proportional to corrected distance */
        int lineH   = (mapS * 320) / (int)disH;
        if (lineH > 320) lineH = 320;
        int lineOff = 160 - (lineH >> 1);  /* centre slice vertically */

        /* Draw 3D wall slice as a thick vertical line */
        glLineWidth(8);
        glBegin(GL_LINES);
        glVertex2i(r * 8 + 530, lineOff);
        glVertex2i(r * 8 + 530, lineOff + lineH);
        glEnd();

        ra = FixAng(ra - 1);  /* advance to next ray */
    }
}

/* ----------------------------- GLUT SETUP --------------------------------- */

void init(void)
{
    glClearColor(0.3f, 0.3f, 0.3f, 0);
    /* Orthographic projection matching window pixel dimensions */
    gluOrtho2D(0, 1024, 510, 0);
    /* Spawn position and angle — same as the SDL2 port */
    px = 150; py = 400; pa = 90;
    pdx = cos(degToRad(pa));
    pdy = -sin(degToRad(pa));
}

void display(void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawMap2D();
    drawPlayer2D();
    drawRays2D();
    glutSwapBuffers();
}

/*
 * main — entry point for the OpenGL reference build.
 * NOT compiled by the CMake project. To build manually (Linux):
 *
 *   gcc Maze_game.c -o maze_gl -lGL -lglut -lm
 *
 * This version has no collision detection, no level system, and no fog.
 * Those features are implemented in src/engine.c.
 */
int main(int argc, char *argv[])
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(1024, 510);
    glutCreateWindow("Maze Raycaster — OpenGL Reference");
    init();
    glutDisplayFunc(display);
    glutKeyboardFunc(Buttons);
    glutMainLoop();
    return 0;
}
