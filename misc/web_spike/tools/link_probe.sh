#!/usr/bin/env bash
# Phase 0 questions 1 and 3: can FMOD's HTML5 static libraries be linked into a Godot-style
# side module, without threads (q1) and with threads (q3)? Also reports which archive defines
# the core API, to settle which libraries must be linked together.
#
# Usage: link_probe.sh <FMOD html5 'api' dir> <out dir> [unzipped web_dlink template dir]
# (emcc must be on PATH). With a template dir, each linked probe's imports are checked too:
# a side module links even with undefined symbols, so "LINK OK" alone proves little.
# Note: wasm-ld accepts objects built without atomics in a -pthread side module, so a threads
# "LINK OK" does not settle question 3 either; the threaded browser run in run.sh does.
set -u
API="$(realpath "$1")"
OUT="$(realpath -m "$2")"
TPL="${3:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"
LIBDIRS=("$API/core/lib/upstream/w32" "$API/studio/lib/upstream/w32")
NM="$(dirname "$(command -v emcc)")/../bin/llvm-nm"
[ -x "$NM" ] || NM="$(dirname "$(command -v emcc)")/../../bin/llvm-nm"

echo "== FMOD HTML5 libraries"
for d in "${LIBDIRS[@]}"; do ls -la "$d"; done

echo
echo "== Which archives define core and studio entry points"
for d in "${LIBDIRS[@]}"; do
    for a in "$d"/*.a; do
        defs="$("$NM" --defined-only "$a" 2>/dev/null)"
        core=$(grep -c " T FMOD_System_Create$" <<<"$defs")
        studio=$(grep -c " T FMOD_Studio_System_Create$" <<<"$defs")
        objs=$("$NM" -A "$a" 2>/dev/null | cut -d: -f2 | sort -u | wc -l)
        echo "$(basename "$a"): defines FMOD_System_Create=$core FMOD_Studio_System_Create=$studio, members=$objs"
    done
done

echo
echo "== Target features of the first object in each archive (atomics/bulk-memory matter for threads)"
for d in "${LIBDIRS[@]}"; do
    for a in "$d"/*.a; do
        tmp="$(mktemp -d)"
        (cd "$tmp" && emar x "$a" >/dev/null 2>&1)
        first="$(ls "$tmp" | head -1)"
        if [ -n "$first" ]; then
            feats="$(strings -n 3 "$tmp/$first" | grep -A30 target_features | head -30 | tr '\n' ' ')"
            echo "$(basename "$a") [$first]: ${feats:-<no target_features section found>}"
        fi
        rm -rf -- "${tmp:?}"
    done
done

cat >"$OUT/probe.c" <<'EOF'
#include <fmod_studio.h>
#include <fmod.h>
__attribute__((used)) int probe(void) {
    FMOD_STUDIO_SYSTEM *s = 0;
    FMOD_SYSTEM *c = 0;
    FMOD_Studio_System_Create(&s, FMOD_VERSION);
    FMOD_Studio_System_GetCoreSystem(s, &c);
    FMOD_Studio_System_Initialize(s, 32, 0, 0, 0);
    return FMOD_System_MixerResume(c);
}
EOF

# Same side-module flags godot-cpp uses for platform=web (tools/web.py).
FLAGS=(-O2 -fPIC -sSIDE_MODULE=1 -sWASM_BIGINT -sSUPPORT_LONGJMP=wasm -I"$API/core/inc" -I"$API/studio/inc")
probe() {
    local label="$1"
    shift
    local libs=() l d found
    for l in "$@"; do
        found=""
        for d in "${LIBDIRS[@]}"; do [ -f "$d/$l" ] && libs+=("$d/$l") && found=1; done
        if [ -z "$found" ]; then
            echo "SKIP       [$label]: $l not in the SDK"
            return
        fi
    done
    for threads in no yes; do
        local extra=() tag="nothreads"
        [ "$threads" = yes ] && extra=(-pthread) && tag="threads"
        local wasm="$OUT/probe_${label}_${tag}.wasm"
        if emcc "${FLAGS[@]}" "${extra[@]}" "$OUT/probe.c" "${libs[@]}" -o "$wasm" >"$wasm.log" 2>&1; then
            echo "LINK OK    [$label, $tag] -> $(basename "$wasm") ($(stat -c %s "$wasm") bytes)"
            if [ -n "$TPL" ]; then
                python3 -I "$HERE/wasm_imports.py" "$wasm" "$TPL" | sed 's/^/           /'
            fi
        else
            echo "LINK FAIL  [$label, $tag]: $(grep -m3 -i "error" "$wasm.log" | cut -c1-300)"
        fi
    done
}

echo
echo "== Side-module link probes (q1 = nothreads, q3 = threads)"
probe logging fmodstudioL_wasm.a fmodL_wasm.a
probe logging_studio_only fmodstudioL_wasm.a
probe release fmodstudio_wasm.a
probe release_plus_core fmodstudio_wasm.a fmod_wasm.a
probe release_plus_reduced fmodstudio_wasm.a fmod_reduced_wasm.a
