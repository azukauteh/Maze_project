# Fuzzing Guide

The Maze project uses LibFuzzer (via ClusterFuzzLite) to find memory-safety
bugs in all parser and loader modules. This document covers how to run
fuzzers locally, what bugs are intentionally seeded, and how to triage
crashes.

## Intentional vulnerabilities (learning targets)

These bugs are deliberately left in the code so fuzzer discovery can be
demonstrated. Each one has a `VULN` comment at the relevant line.

| File | Function | Bug type | Description |
|------|----------|----------|-------------|
| `src/map_parser.c` | `parse_binary_map` | Integer overflow | `width*height` computed in signed int before malloc. Large uint16 values overflow to a tiny allocation; `memcpy` then writes OOB. |
| `src/map_parser.c` | `parse_rle_map` | OOB heap write | Decoded cell count not checked against `W*H` before each write. A long RLE stream writes past end of `RLEMap.cells`. |
| `src/config_parser.c` | `parse_key` | Stack buffer overflow | Key is accumulated into a 64-byte stack buffer with no bounds check on the write index. A key > 63 chars overflows. |
| `src/config_parser.c` | `parse_section` | Stack buffer overflow | Same pattern as `parse_key` but for section headers between `[` and `]`. |
| `src/level_loader.c` | `level_load_from_data` | OOB write (sscanf) | `sscanf(value, "%s", out->name)` has no width limit. A name > 31 chars overflows the 32-byte `out->name` buffer. |
| `src/texture.c` | `texture_load_ppm` | OOB heap write | Write loop reads until end of PPM body, not until `width*height` pixels. Extra body bytes write past the end of `tex->pixels`. |
| `src/savegame.c` | `savegame_load_from_data` | Version bypass | Version field is logged but not gated. Future struct layouts with a larger payload would write OOB through the unchecked `memcpy`. |

## Requirements

```bash
# Clang with sanitizer support (Ubuntu/WSL)
sudo apt install clang

# Or use the system clang if available
clang --version
```

LibFuzzer is built into clang. No separate install needed.

## Building individual fuzzers

All fuzzers live under `fuzz/`. Build each one manually with clang:

### Map fuzzer (3 targets: text, binary, RLE)

```bash
clang -fsanitize=fuzzer,address \
      -g -O1 \
      -I src \
      fuzz/map_fuzzer.c src/map_parser.c \
      -o map_fuzzer
```

### Config fuzzer

```bash
clang -fsanitize=fuzzer,address \
      -g -O1 \
      -I src \
      fuzz/config_fuzzer.c src/config_parser.c \
      -o config_fuzzer
```

### Level fuzzer

```bash
clang -fsanitize=fuzzer,address \
      -g -O1 \
      -I src \
      fuzz/level_fuzzer.c \
      src/level_loader.c src/map_parser.c src/config_parser.c \
      -o level_fuzzer
```

### Texture fuzzer

```bash
clang -fsanitize=fuzzer,address \
      -g -O1 \
      -I src \
      fuzz/texture_fuzzer.c src/texture.c \
      -lm -o texture_fuzzer
```

### Savegame fuzzer

```bash
clang -fsanitize=fuzzer,address \
      -g -O1 \
      -I src \
      fuzz/savegame_fuzzer.c \
      src/savegame.c src/engine.c src/map_parser.c \
      -lm -o savegame_fuzzer
```

## Running fuzzers

```bash
# Create corpus directory if not present
mkdir -p fuzz/corpus/map_fuzzer

# Run until crash or Ctrl-C
./map_fuzzer fuzz/corpus/map_fuzzer/ -max_len=512 -timeout=10

# Run for a fixed number of iterations (useful for CI)
./map_fuzzer fuzz/corpus/map_fuzzer/ -max_len=512 -runs=100000
```

Repeat with `config_fuzzer`, `level_fuzzer`, `texture_fuzzer`,
`savegame_fuzzer` and their respective corpus directories.

## Expected crashes

Running the fuzzers against the intentional vulnerabilities should produce
crashes within seconds to minutes depending on your machine. Example:
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address ...
WRITE of size 4 at ...
#0 parse_binary_map src/map_parser.c:NNN

The crash input is saved automatically as `crash-<hash>` in the working
directory.

## Triaging a crash

1. Reproduce with the saved input:
```bash
   ./map_fuzzer crash-<hash>
```

2. Get a symbolized stack trace:
```bash
   ASAN_OPTIONS=symbolize=1 ./map_fuzzer crash-<hash>
```

3. Find the `VULN` comment nearest the crashing frame in the source.

4. To fix: add the appropriate bounds check. To keep as a demo: document
   the crash and add the input to the corpus so future runs skip it.

## ClusterFuzzLite (CI integration)

`.clusterfuzzlite/project.yaml` configures ClusterFuzzLite to run on every
PR. The build script `.clusterfuzzlite/build.sh` compiles all fuzz targets
with the flags above. Add new fuzzers to `build.sh` as modules are added.

When ClusterFuzzLite finds a crash it opens a GitHub issue with the
reproducer attached. Fix the bug, add the crash input to the corpus, and
close the issue.

## Adding a new fuzzer

1. Create `fuzz/<module>_fuzzer.c` with an `LLVMFuzzerTestOneInput` entry
   point.
2. Add a build line to `.clusterfuzzlite/build.sh`.
3. Create `fuzz/corpus/<module>_fuzzer/` with at least one seed file.
4. Add a build section to this document.

## Corpus management

Seed inputs in `fuzz/corpus/*/` are committed to the repo. LibFuzzer
generates new corpus entries automatically during runs — commit interesting
ones (those that increase coverage) so future CI runs benefit.

Keep corpus entries small (< 1 KB). Large inputs slow mutation and reduce
coverage diversity.

## Coverage report

```bash
# Build with coverage instrumentation
clang -fsanitize=fuzzer,address,coverage \
      -fprofile-instr-generate -fcoverage-mapping \
      -I src fuzz/map_fuzzer.c src/map_parser.c \
      -o map_fuzzer_cov

./map_fuzzer_cov fuzz/corpus/map_fuzzer/ -runs=10000

# Generate report
llvm-profdata merge -sparse *.profraw -o default.profdata
llvm-cov report ./map_fuzzer_cov -instr-profile=default.profdata
llvm-cov show   ./map_fuzzer_cov -instr-profile=default.profdata \
                src/map_parser.c > coverage.txt
```
