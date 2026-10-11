#!/usr/bin/env bash
# Phase 0 web feasibility spike: runs every check end to end and writes build/report/.
#
# Usage: ./run.sh <FMOD Studio API HTML5 archive (.zip/.tar.gz) or its extracted folder>
# Optional env:
#   EMSDK_ENV   path to emsdk_env.sh (else emcc must already be on PATH; Godot 4.6 uses 4.0.11)
#   GODOT_DIR   dir with 'godot' editor binary and templates/ (else fetched into build/godot)
#   FMOD_LIBS   libraries for the spike build (default: fmodstudioL_wasm.a,fmodL_wasm.a)
#   BANKS_DIR   .bank files to test with (default: the *.bank files shipped in the FMOD archive)
#   SPIKE_THREADS=1  also build, export and run the threaded variant (question 3)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$(realpath "$1")"
BUILD="$HERE/build"
REPORT="$BUILD/report"
mkdir -p "$BUILD" "$REPORT"
[ -n "${EMSDK_ENV:-}" ] && source "$EMSDK_ENV" >/dev/null 2>&1
command -v emcc >/dev/null || { echo "emcc not found: set EMSDK_ENV"; exit 1; }
emcc --version | head -1 | tee "$REPORT/emcc_version.txt"

echo "== Extracting FMOD"
FMOD="$BUILD/fmod"
if [ -d "$SRC" ]; then
    FMOD="$SRC"
else
    rm -rf -- "${BUILD:?}/fmod" && mkdir -p "$FMOD"
    case "$SRC" in
        *.zip) unzip -q "$SRC" -d "$FMOD" ;;
        *) tar -xf "$SRC" -C "$FMOD" ;;
    esac
fi
API="$(dirname "$(find "$FMOD" -type d -path '*/studio/inc' | head -1)")"
API="$(dirname "$API")"
[ -d "$API/core/lib/upstream/w32" ] || { echo "no api/core/lib/upstream/w32 under $FMOD"; find "$FMOD" -maxdepth 4 -type d; exit 1; }
echo "api dir: $API"
find "$FMOD" -maxdepth 6 | sed "s|$FMOD/||" | sort > "$REPORT/fmod_tree.txt"

echo "== Godot editor and web templates"
GODOT_DIR="${GODOT_DIR:-$BUILD/godot}"
[ -x "$GODOT_DIR/godot" ] && [ -f "$GODOT_DIR/templates/web_dlink_nothreads_debug.zip" ] || "$HERE/tools/fetch_godot.sh" "$GODOT_DIR"
TPL="$BUILD/template_nothreads"
rm -rf -- "${BUILD:?}/template_nothreads" && unzip -q "$GODOT_DIR/templates/web_dlink_nothreads_debug.zip" -d "$TPL"

echo "== Link probes (q1, q3)"
"$HERE/tools/link_probe.sh" "$API" "$BUILD/probe" "$TPL" | tee "$REPORT/link_probe.txt"

echo "== Banks"
BANKS_DIR="${BANKS_DIR:-}"
find "$HERE/project/banks" -name '*.bank' -delete
if [ -n "$BANKS_DIR" ]; then
    cp "$BANKS_DIR"/*.bank "$HERE/project/banks/"
else
    find "$FMOD" -name '*.bank' -exec cp {} "$HERE/project/banks/" \;
fi
ls -la "$HERE/project/banks" | tee "$REPORT/banks.txt"

run_variant() { # <nothreads|threads>
    local variant="$1" threads=no tpl_name=web_dlink_nothreads suffix=.nothreads port=8765
    if [ "$variant" = threads ]; then threads=yes; tpl_name=web_dlink; suffix=""; port=8766; fi
    local out="$REPORT/$variant"
    mkdir -p "$out"

    echo "== [$variant] Building spike side module"
    cd "$HERE"
    scons -j"$(nproc)" platform=web threads=$threads target=template_debug api_version=4.6 \
        build_profile=../../build_profile.json fmod_api="$API" \
        fmod_libs="${FMOD_LIBS:-fmodstudioL_wasm.a,fmodL_wasm.a}" >"$out/spike_build.txt" 2>&1 || true
    local side="$HERE/project/bin/libfmod_web_spike.web.template_debug.wasm32$suffix.wasm"
    if [ ! -f "$side" ] || ! grep -q "done building targets" "$out/spike_build.txt"; then
        echo "spike build failed:"; grep -m10 -i "error" "$out/spike_build.txt"; return 1
    fi

    echo "== [$variant] Static import check (q2)"
    local tpl="$BUILD/template_$variant"
    rm -rf -- "${BUILD:?}/template_$variant" && unzip -q "$GODOT_DIR/templates/${tpl_name}_debug.zip" -d "$tpl"
    python3 -I "$HERE/tools/wasm_imports.py" "$side" "$tpl" | tee "$out/imports.txt" || true

    echo "== [$variant] Exporting with Godot"
    sed -e "s|__TEMPLATE_DEBUG__|$GODOT_DIR/templates/${tpl_name}_debug.zip|" \
        -e "s|__TEMPLATE_RELEASE__|$GODOT_DIR/templates/${tpl_name}_release.zip|" \
        -e "s|variant/thread_support=false|variant/thread_support=$([ $threads = yes ] && echo true || echo false)|" \
        "$HERE/project/export_presets.cfg.in" >"$HERE/project/export_presets.cfg"
    local web="$BUILD/web_$variant"
    rm -rf -- "${BUILD:?}/web_$variant" && mkdir -p "$web"
    "$GODOT_DIR/godot" --headless --path "$HERE/project" --import >"$out/godot_import.txt" 2>&1 || true
    "$GODOT_DIR/godot" --headless --path "$HERE/project" --export-debug Web "$web/index.html" \
        >"$out/godot_export.txt" 2>&1 || { tail -20 "$out/godot_export.txt"; return 1; }

    echo "== [$variant] Browser run (q2 runtime, q4$([ $threads = yes ] && echo ", q3"))"
    python3 -I "$HERE/tools/serve.py" "$web" "$port" &
    local server=$!
    sleep 1
    NODE_PATH="$(npm root -g)" node "$HERE/tools/browser_test.cjs" "http://127.0.0.1:$port/index.html" \
        "$out/browser.json" | tee "$out/browser_summary.json"
    kill "$server" 2>/dev/null || true
}

run_variant nothreads
[ "${SPIKE_THREADS:-0}" = 1 ] && run_variant threads
echo "Report written to $REPORT"
