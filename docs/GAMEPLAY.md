# Gameplay Guide

## Objective

Navigate each maze and walk into the **green wall** (exit) to advance.
Complete all 6 levels to win.

## Controls

| Key | Action        |
|-----|---------------|
| W   | Move forward  |
| S   | Move backward |
| A   | Turn left     |
| D   | Turn right    |
| ESC | Quit          |

## Minimap (top-left)

| Color      | Meaning            |
|------------|--------------------|
| Light grey | Wall               |
| Green tile | Exit               |
| Red dot    | Your position      |
| Yellow bar | Facing direction   |

Exit is always at tile (col 6, row 6) — bottom-right interior corner.

## Levels

| # | Name          | Sky/walls   | Spawn dir | Difficulty |
|---|---------------|-------------|-----------|------------|
| 1 | The Corridor  | Blue/grey   | East      | Easy       |
| 2 | The Grid      | Green       | East      | Medium     |
| 3 | Dead Ends     | Red         | South     | Medium     |
| 4 | The Spiral    | Purple      | West      | Hard       |
| 5 | Broken Grid   | Orange      | North     | Hard       |
| 6 | The Labyrinth | Cyan (FINAL)| East      | Very hard  |

### Level 1 — The Corridor (blue)
Open layout. Wide corridors, single internal wall column.
**Strategy:** turn right from spawn, follow south wall east.

### Level 2 — The Grid (green)
Denser intersections. Several wrong turns available.
**Strategy:** hug the right wall (right-hand rule navigates this layout).

### Level 3 — Dead Ends (red)
Multiple dead-ends. Spawn faces south so the exit is behind you.
**Strategy:** turn around immediately, take first left, follow south wall.

### Level 4 — The Spiral (purple)
A central pillar forces a counter-clockwise loop. Spawn faces west
(the wrong direction). Turning right leads to a dead-end immediately.
**Strategy:** turn left at spawn, follow the outer wall all the way around.

### Level 5 — Broken Grid (orange)
Alternating blocked intersections create a broken checkerboard.
Spawn is bottom-right facing north — the exit is also bottom-right
but you need to go south first to reach the correct corridor.
**Strategy:** turn around at spawn, take the bottom corridor west then
north, then loop east along the open row to the exit.

### Level 6 — The Labyrinth (cyan — FINAL)
Narrowest corridors. S-shaped path. Fog is the main hazard — walls
appear and disappear suddenly in the tight passages.
**Strategy:** from spawn (east-facing), go straight until the first
junction, turn south, follow the S-bend east at the bottom.

## Visual cues

| Cue                        | Meaning                           |
|----------------------------|-----------------------------------|
| Walls fade to black        | You are far away (fog)            |
| Walls fill the screen      | You are very close to a wall      |
| Green column visible ahead | Exit is in front of you           |
| Sky/floor color            | Current level palette             |
| Window title               | Level number and name             |

## Known limitations

- Collision is point-based — fast diagonal movement can clip corners.
- No HUD text on screen. Level info is in the window title bar.
- After level 6 the process exits cleanly. No main menu or restart.
