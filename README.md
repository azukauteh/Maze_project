
```markdown
# Maze Raycaster

A first-person 3D maze game built with C and SDL2, using a classic raycasting engine.

## Features

- Raycasting renderer with fisheye correction
- 3 levels, each with a distinct layout and color palette
- Distance-based fog — walls fade to black at range
- Gradient sky and floor per level
- Corner minimap with player position and facing direction
- Wall collision detection
- Green exit wall triggers level transition

## Project layout

```
.
├── src/
│   ├── main.c          — SDL2 window, event loop, level transitions
│   ├── engine.c        — raycasting, rendering, input, level data
│   ├── engine.h        — GameState, Player, public API
│   ├── map_parser.c    — fuzzing-safe text map parser
│   ├── map_parser.h
│   ├── Walls.cpp       — reference stub
│   └── CMakeLists.txt
├── tests/
│   ├── test_level.c    — regression tests (no SDL2 required)
│   └── CMakeLists.txt
├── fuzz/
│   └── map_fuzzer.c
├── .clusterfuzzlite/
└── CMakeLists.txt
```

## Build

### Linux / WSL (Debian/Ubuntu)

```bash
sudo apt update
sudo apt install build-essential cmake libsdl2-dev

mkdir build && cd build
cmake ..
cmake --build .
```

### macOS (Homebrew)

```bash
brew install sdl2 cmake

mkdir build && cd build
cmake ..
cmake --build .
```

### Windows (vcpkg)

```powershell
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install sdl2:x64-windows

mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=..\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build . --config Release
```

## Run

```bash
./build/src/maze
# Windows: .\build\src\Release\maze.exe
```

## Controls

| Key | Action       |
|-----|--------------|
| W   | Move forward |
| S   | Move backward|
| A   | Turn left    |
| D   | Turn right   |
| ESC | Quit         |

## Levels

| Level | Sky/floor tint | Wall color | Notes                  |
|-------|---------------|------------|------------------------|
| 1     | Blue          | Grey       | Open layout, intro     |
| 2     | Green         | Green-grey | Denser corridors       |
| 3     | Red           | Red-grey   | Tight dead-ends, final |

Find the **green wall** in each level to advance. Reaching it after level 3 ends the game.

## Tests

```bash
cd build
ctest --output-on-failure
# or directly:
./tests/test_level
```

Five regression tests cover: initial state, wall collision, exit detection, level 2 load, and map diff between levels.

## Possible next steps

- Texture-mapped walls
- Sprite rendering (enemies, pickups)
- Map loader from text files via `map_parser`
- Sound effects with SDL2_mixer
- Level timer / scoring
- itch.io packaging
```
