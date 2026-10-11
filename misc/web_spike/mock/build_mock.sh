#!/usr/bin/env bash
# HARNESS VALIDATION ONLY. Builds mock FMOD archives laid out like the FMOD HTML5 SDK:
#   api/        position-independent objects (should link into a side module)
#   api_nopic/  -fno-PIC objects (must fail to link, exercising probe question 1)
# Usage: build_mock.sh   (emcc must be on PATH)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$(mktemp -d)"

build() { # <api dir> <cflags...>
    local api="$1"
    shift
    local tag
    tag="$(basename "$api")"
    mkdir -p "$api/core/lib/upstream/w32" "$api/studio/lib/upstream/w32" "$api/core/inc" "$api/studio/inc"
    cp "$HERE"/api/core/inc/*.h "$api/core/inc/" 2>/dev/null || true
    cp "$HERE"/api/studio/inc/*.h "$api/studio/inc/" 2>/dev/null || true
    emcc -O2 "$@" -I"$HERE/api/core/inc" -c "$HERE/mock_fmod.c" -o "$WORK/$tag.o"
    echo "int mock_fmod_core_marker;" >"$WORK/core.c"
    emcc -O2 "$@" -c "$WORK/core.c" -o "$WORK/${tag}_core.o"
    # Archive into fresh temp files, then move into place (replaces any previous build).
    emar rcs "$WORK/${tag}_studio.a" "$WORK/$tag.o"
    emar rcs "$WORK/${tag}_corelib.a" "$WORK/${tag}_core.o"
    mv -f "$WORK/${tag}_studio.a" "$api/studio/lib/upstream/w32/fmodstudioL_wasm.a"
    mv -f "$WORK/${tag}_corelib.a" "$api/core/lib/upstream/w32/fmodL_wasm.a"
}

build "$HERE/api" -fPIC
build "$HERE/api_nopic" -fno-PIC
echo "mock archives:"
find "$HERE/api" "$HERE/api_nopic" -name '*.a' -exec ls -la {} \;
