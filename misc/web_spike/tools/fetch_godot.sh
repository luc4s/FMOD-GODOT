#!/usr/bin/env bash
# Downloads the Godot 4.6 Linux editor and extracts only the web dlink templates from the
# official export templates (via HTTP range requests, instead of the full ~1 GB archive).
# Usage: fetch_godot.sh <out dir>
set -euo pipefail
OUT="$1"
VER="${GODOT_VERSION:-4.6-stable}"
BASE="https://github.com/godotengine/godot/releases/download/$VER"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT/templates"
if [ ! -x "$OUT/godot" ]; then
    curl -sSL -o "$OUT/godot.zip" "$BASE/Godot_v${VER}_linux.x86_64.zip"
    unzip -o -q "$OUT/godot.zip" -d "$OUT"
    mv "$OUT/Godot_v${VER}_linux.x86_64" "$OUT/godot"
    rm -f -- "${OUT:?}/godot.zip"
fi
for t in web_dlink_nothreads_debug web_dlink_nothreads_release web_dlink_debug web_dlink_release; do
    [ -f "$OUT/templates/$t.zip" ] || python3 -I "$HERE/remote_zip_extract.py" \
        "$BASE/Godot_v${VER}_export_templates.tpz" "templates/$t.zip" "$OUT/templates/$t.zip"
done
