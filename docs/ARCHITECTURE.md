# Architecture

## Overview
The engine is a classic 1990s-style raycaster: no 3D geometry, no depth buffer,
no perspective projection matrix. The world is a 2D integer grid. Each frame,
60 rays are cast from the player position into the grid. The distance to the
nearest wall along each ray determines the height of a vertical screen column.
Repeat for every column and you have a 3D-looking scene.
Player position (px, py)
Player angle   pa  (0–359 degrees, integer)
Direction vec  (pdx, pdy) = (cos(pa), -sin(pa))
FOV = 60 degrees
Ray 0  starts at pa + 30   (leftmost)
Ray 59 ends   at pa - 30   (rightmost)

## Coordinate system
- Origin is top-left of the world.
- X increases right, Y increases down.
- Angles increase counter-clockwise (standard math convention).
- One tile = 64 × 64 pixels in world space (MAP_SCALE = 64).
- An 8×8 tile map = 512×512 world pixels.

## DDA ray-grid intersection
For each ray at angle `ra`, two independent DDA (Digital Differential Analyzer)
passes find the nearest wall intersection:

### Vertical pass (hits vertical grid lines x = n*64)
if cos(ra) > 0:          # ray faces right
rx = floor(px/64)*64 + 64    # first vertical line to the right
xo = +64
else:
rx = floor(px/64)*64 - ε     # first vertical line to the left
xo = -64
ry = (px - rx) * tan(ra) + py
yo = -xo * tan(ra)              # maintain the slope
repeat up to 8 times:
tile = map[floor(ry/64)][floor(rx/64)]
if tile == WALL: record hit distance, stop
rx += xo; ry += yo

### Horizontal pass (hits horizontal grid lines y = n*64)
if sin(ra) > 0:          # ray faces up (y decreases visually)
ry = floor(py/64)*64 - ε
yo = -64
else:
ry = floor(py/64)*64 + 64
yo = +64
rx = (py - ry) / tan(ra) + px
xo = -yo / tan(ra)
repeat up to 8 times:
tile = map[floor(ry/64)][floor(rx/64)]
if tile == WALL: record hit distance, stop
rx += xo; ry += yo


The closer of the two hit distances is used. Vertical-face hits are shaded
brighter than horizontal-face hits to simulate a light from above-left.

## Projected distance and fisheye correction
Raw distance along the ray produces a fisheye effect: corners of the FOV
appear stretched. The fix multiplies by the cosine of the angle between the
ray and the player's facing direction:

ca   = player_angle - ray_angle          (angle delta, wrapped 0–359)
dist = raw_dist * cos(ca)


This projects the hit point onto the view plane perpendicular to `pa`,
giving a flat screen rather than a sphere.

## Wall slice height
lineH   = (MAP_SCALE * screen_height) / dist
lineOff = (screen_height - lineH) / 2     # centre on screen

Larger distance → smaller `lineH` → wall slice appears farther away.

## Fog
Linear fade to black based on corrected distance:

t     = clamp(dist / FOG_MAX, 0.0, 1.0)
color = base_color * (1.0 - t

`FOG_MAX` is 400 world pixels (~6 tile-lengths). Beyond that walls are fully
black, hiding the hard 8-step DDA limit.

## Sky and floor gradient
Instead of a flat clear color, the framebuffer is pre-filled each frame:

for y in 0..height/2:          # sky
t = y / (height/2)         # 0=top, 1=horizon
color = lerp(sky_top, sky_horizon, t)
for y in height/2..height:     # floor
t = (y - height/2) / (height/2)   # 0=horizon, 1=feet
color = lerp(floor_horizon, floor_near, t)


Sky and floor base colors change per level (blue → green → red) to signal
which level the player is on without any HUD text.

## Collision detection
Before updating player position, the destination tile is checked:

nx = px + pdx * speed
ny = py + pdy * speed
cell = map[floor(ny/64)][floor(nx/64)]
if cell == WALL:  discard move
if cell == EXIT:  set level_complete = 1
else:             accept move

This is point-collision (no radius). The player can clip into wall corners at
oblique angles. Proper AABB collision would check four corner offsets.

## Level transition
`current_level` and `level_complete` live in `GameState`. When
`level_complete` is set, `main.c` calls `engine_next_level()` which:

1. Increments `current_level`.
2. Copies the next level's map array into `state->map`.
3. Resets player position and angle to that level's spawn point.

All level data is statically compiled into `engine.c` as `const int[]` arrays.

## Minimap

Drawn last so it appears on top of the 3D view. Each tile maps to an 8×8 pixel
block at top-left of the framebuffer. The player is a 3×3 red dot. A 5-pixel
yellow line shows facing direction using the precomputed `(pdx, pdy)` vector
scaled from world space to minimap space:


minimap_x = MINI_PAD + (world_x / 64.0) * MINI_SCALE
minimap_y = MINI_PAD + (world_y / 64.0) * MINI_SCALE

## File map

| File | Responsibility |
|------|---------------|
| `src/main.c` | SDL2 init, event loop, level transition logic |
| `src/engine.c` | Raycasting, rendering, input, level data |
| `src/engine.h` | `GameState`, `Player`, public function signatures |
| `src/map_parser.c` | Text-format map parser (used by fuzzer) |
| `src/map_parser.h` | `MazeMap` struct, `parse_maze_map()` declaration |
| `tests/test_level.c` | Headless regression tests (no SDL2) |
| `Maze_game.c` | Original OpenGL/GLUT prototype (not in CMake build) |
| `Walls.cpp` | CMake placeholder stub |
