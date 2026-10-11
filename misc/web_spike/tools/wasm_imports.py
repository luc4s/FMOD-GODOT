#!/usr/bin/env python3
"""Static check for Phase 0 question 2: will Godot's dlink main module satisfy a side module's imports?

Lists every function/GOT import of the side module and checks it against what the Godot web
template provides: exports of godot.wasm / godot.side.wasm, JS library functions defined in
godot.js, and the side module's own exports (EM_JS / self-defined GOT entries).

Usage: wasm_imports.py <side_module.wasm> <unzipped web_dlink template dir>
Exit code 1 if any import is unresolved. This is a heuristic: the browser run is the real test.
"""
import re
import sys
from pathlib import Path


def leb(data, pos):
    result = shift = 0
    while True:
        b = data[pos]
        pos += 1
        result |= (b & 0x7F) << shift
        shift += 7
        if not b & 0x80:
            return result, pos


def name(data, pos):
    n, pos = leb(data, pos)
    return data[pos : pos + n].decode("utf-8"), pos + n


def parse(path):
    """Returns (imports as [(module, field, kind)], exports as set of names)."""
    data = Path(path).read_bytes()
    assert data[:4] == b"\0asm", f"{path} is not a wasm module"
    pos, imports, exports = 8, [], set()
    while pos < len(data):
        sid = data[pos]
        size, pos = leb(data, pos + 1)
        end = pos + size
        if sid == 2:  # import section
            count, p = leb(data, pos)
            for _ in range(count):
                module, p = name(data, p)
                field, p = name(data, p)
                kind = data[p]
                p += 1
                if kind == 0:  # func: type index
                    _, p = leb(data, p)
                elif kind == 1:  # table: reftype + limits
                    p += 1
                    flags = data[p]
                    _, p = leb(data, p + 1)
                    if flags & 1:
                        _, p = leb(data, p)
                elif kind == 2:  # memory: limits
                    flags = data[p]
                    _, p = leb(data, p + 1)
                    if flags & 1:
                        _, p = leb(data, p)
                elif kind == 3:  # global: valtype + mut
                    p += 2
                elif kind == 4:  # tag
                    p += 1
                    _, p = leb(data, p)
                imports.append((module, field, "func global table memory tag".split()[kind]))
        elif sid == 7:  # export section
            count, p = leb(data, pos)
            for _ in range(count):
                field, p = name(data, p)
                _, p = leb(data, p + 1)
                exports.add(field)
        pos = end
    return imports, exports


# Provided by the dynamic linker / runtime rather than any module export.
RUNTIME = {"memory", "__indirect_function_table", "__memory_base", "__table_base", "__stack_pointer"}


def main():
    side, tpl = Path(sys.argv[1]), Path(sys.argv[2])
    imports, own_exports = parse(side)
    provided = set()
    for mod in ("godot.wasm", "godot.side.wasm"):
        provided |= parse(tpl / mod)[1]
    js = (tpl / "godot.js").read_text(encoding="utf-8", errors="replace")
    js_funcs = set(re.findall(r"\b_([A-Za-z_][A-Za-z0-9_$]*)\s*(?:=|\()", js))

    unresolved = []
    by_module = {}
    for module, field, kind in imports:
        by_module.setdefault(module, 0)
        by_module[module] += 1
        if field in RUNTIME or kind in ("memory", "table"):
            continue
        ok = (
            field in provided
            or field in own_exports
            or f"__em_js__{field}" in own_exports
            or field in js_funcs
        )
        if not ok:
            unresolved.append(f"{module}.{field} ({kind})")

    print(f"side module: {side.name}")
    print("imports per module: " + ", ".join(f"{m}={n}" for m, n in sorted(by_module.items())))
    em_js = sorted(e[len("__em_js__") :] for e in own_exports if e.startswith("__em_js__"))
    print(f"EM_JS functions carried by the side module: {len(em_js)} {em_js[:20]}")
    invokes = sorted(f for _, f, _ in imports if f.startswith("invoke_"))
    if invokes:
        print(f"invoke_* imports (JS-based exceptions/longjmp): {invokes}")
    if unresolved:
        print(f"UNRESOLVED ({len(unresolved)}):")
        for u in sorted(unresolved):
            print("  " + u)
        return 1
    print("all imports resolvable against the template (heuristic)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
