# Contributing

## Project structure

One concern per directory. New features go in `src/`. New tests go in
`tests/`. Do not add business logic to `main.c` — it is a thin event loop only.

## Building

```bash
sudo apt install build-essential cmake libsdl2-dev  # Ubuntu/WSL
mkdir build && cd build
cmake ..
cmake --build .
```

## Running tests

```bash
cd build
ctest --output-on-failure
# or directly:
./tests/test_level
```

All tests must pass before submitting a PR. Tests have no SDL2 dependency and
run headlessly — they link only against `engine.c`, `map_parser.c`, and `libm`.

## Adding a level

1. Add a `const int level_N_map[MAP_WIDTH * MAP_HEIGHT]` array in `src/engine.c`.
2. Add a `case N:` branch in `engine_next_level()`.
3. Update `MAX_LEVELS` in `src/main.c`.
4. Add a row to the Levels table in `README.md`.
5. Add invariant checks for the new level in `tests/test_level.c`
   (`count_cell`, `outer_walls_intact`).

Map rules:
- Outer ring must be all `1` (wall) tiles.
- Exactly one `2` (exit) tile per map.
- No value other than `0`, `1`, or `2`.
- 8×8 grid — `MAP_WIDTH` and `MAP_HEIGHT` are both 8.

## Adding visual features

All rendering is in `engine_render()` in `src/engine.c`, split into three
helpers called in order:
render_sky_floor()   — fills framebuffer with gradient background
render_walls()       — raycasting pass, writes wall slices
render_minimap()     — overlays minimap on top-left corneri

Add a new helper and call it from `engine_render()`. Keep each helper under
~80 lines. Do not add SDL2 calls inside the engine — the engine writes only
to `state->pixels`. SDL2 interaction stays in `main.c`.

## Fuzzing

The map parser is fuzz-tested via LibFuzzer and ClusterFuzzLite.

Manual fuzz run (requires clang):

```bash
clang -fsanitize=fuzzer,address \
      -I src \
      fuzz/map_fuzzer.c src/map_parser.c \
      -o map_fuzzer
./map_fuzzer fuzz/corpus/map_fuzzer/ -max_len=256 -runs=100000
```

If the fuzzer finds a crash, add the crashing input to
`fuzz/corpus/map_fuzzer/` and fix the parser before merging.

## Code style

- C11, no compiler extensions.
- `snake_case` for functions and variables.
- `UPPER_CASE` for `#define` constants.
- No dynamic allocation outside `engine_init()`. The framebuffer is allocated
  once and reused across frames.
- No `goto` except the two `skip_vert` / `skip_horiz` labels in the
  raycasting loop (unavoidable given the loop structure).
- Every public function declared in a header. No `extern` across `.c` files.
- New files need a top comment explaining purpose and what they do NOT do.

## Commit message format

<type>(<scope>): <short summary>
<body — what changed and why, not how>
<footer — references, breaking changes>
````
Types: feat, fix, docs, test, refactor, chore.

Examples:
feat(engine): add level 3 with red palette and tight dead-ends
fix(engine): wall collision now blocks diagonal corner clipping
docs(arch): document DDA algorithm and fisheye correction formula
test(level): add parser integration tests and map invariant checks
feat(engine): add level 3 with red palette and tight dead-ends
fix(engine): wall collision now blocks diagonal corner clipping
docs(arch): document DDA algorithm and fisheye correction formula
test(level): add parser integration tests and map invariant checks
