#!/bin/bash -eu

SRC_DIR="${SRC:-$(pwd)}"
OUT_DIR="${OUT:-$SRC_DIR/out}"
CXX_BIN="${CXX:-clang++}"
CXXFLAGS_BIN="${CXXFLAGS:-}"
CC_BIN="${CC:-clang}"
CFLAGS_BIN="${CFLAGS:-}"

cd "$SRC_DIR"
mkdir -p "$OUT_DIR"

if ! command -v "$CXX_BIN" >/dev/null 2>&1; then
  if command -v clang++ >/dev/null 2>&1; then
    CXX_BIN=clang++
  elif command -v g++ >/dev/null 2>&1; then
    CXX_BIN=g++
  else
    # No C++ compiler available; continue and fall back to the C linker where possible
    CXX_BIN=""
  fi
fi

# Ensure a C compiler is available for compiling .c files
if ! command -v "$CC_BIN" >/dev/null 2>&1; then
  if command -v clang >/dev/null 2>&1; then
    CC_BIN=clang
  elif command -v gcc >/dev/null 2>&1; then
    CC_BIN=gcc
  else
    echo "No C compiler found (tried $CC_BIN, clang, gcc)" >&2
    exit 1
  fi
fi

"$CC_BIN" $CFLAGS_BIN -std=c11 -I"$SRC_DIR/src" -c "$SRC_DIR/src/map_parser.c" -o "$OUT_DIR/map_parser.o"
"$CC_BIN" $CFLAGS_BIN -std=c11 -I"$SRC_DIR/src" -c "$SRC_DIR/fuzz/map_fuzzer.c" -o "$OUT_DIR/map_fuzzer.o"

LINKER_BIN="${CXX_BIN:-$CC_BIN}"

if [ -n "${LIB_FUZZING_ENGINE:-}" ]; then
  "$LINKER_BIN" ${CXXFLAGS_BIN:-$CFLAGS_BIN} "$OUT_DIR/map_fuzzer.o" "$OUT_DIR/map_parser.o" "$LIB_FUZZING_ENGINE" -o "$OUT_DIR/map_fuzzer"
elif [ -n "$CXX_BIN" ] && "$CXX_BIN" --help 2>&1 | grep -q -- '-fsanitize=fuzzer'; then
  "$CXX_BIN" ${CXXFLAGS_BIN:-$CFLAGS_BIN} -fsanitize=fuzzer,address "$OUT_DIR/map_fuzzer.o" "$OUT_DIR/map_parser.o" -o "$OUT_DIR/map_fuzzer"
else
  "$LINKER_BIN" ${CXXFLAGS_BIN:-$CFLAGS_BIN} -Dmain=LLVMFuzzerTestOneInput "$OUT_DIR/map_fuzzer.o" "$OUT_DIR/map_parser.o" -o "$OUT_DIR/map_fuzzer"
fi
