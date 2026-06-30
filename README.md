# Maze Project

This repository contains a raycasting 3D game engine built with SDL2.

## What is included

- `Maze_game.c` (reference) and `Walls.cpp` (reference) at repo root.
- `src/` contains the working implementation:
  - `main.c` — SDL2 window and event loop
  - `engine.c` / `engine.h` — raycasting core
  - `CMakeLists.txt` — build configuration
- `CMakeLists.txt` at root level configures the build.

## Build

### Windows (with vcpkg)

Install SDL2:

```powershell
# Clone and bootstrap vcpkg once
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat

# Install SDL2
.\vcpkg\vcpkg install sdl2:x64-windows
```

Configure and build:

```powershell
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=..\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build . --config Release
```

### Linux (Debian/Ubuntu)

Install dependencies:

```bash
sudo apt update
sudo apt install build-essential cmake libsdl2-dev
```

Build:

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### macOS (with Homebrew)

```bash
brew install sdl2 cmake
mkdir build && cd build
cmake ..
cmake --build .
```

## Run

```bash
./build/src/maze
# or on Windows: .\build\src\Release\maze.exe
```

Controls:
- **W** — move forward
- **A** — turn left
- **D** — turn right
- **S** — move backward
- **ESC** — quit

## Next steps

- Add sprite rendering (enemies, items)
- Implement texture-mapped walls
- Add map loader (JSON/binary format)
- Add sound effects and music
- Unit tests and CI pipeline
- Cross-platform packaging and release to itch.io / Steam
# Maze project
# COMING SOON!
