# Web export – Phase 0 probe (go/no-go)

Throwaway test build for [issue #1](https://github.com/luc4s/FMOD-GODOT/issues/1), Phase 0.
It answers whether FMOD's prebuilt HTML5 static libraries can be linked into a Godot GDExtension
side module and play audio. Nothing here is part of the plugin build.

> Web support has since been implemented in the plugin itself (see `docs/tutorials/basic/exporting_for_web.rst`).
> This probe is kept for reference; where this page and the plugin differ, the plugin is current.

**Result: GO.** Single-threaded and threaded both work with the official Godot 4.6 web templates
(no custom templates needed). One small JS shim is required, see [Finding 3](#3-required-shim-fmod-js-mixer-exports).

## Setup that was tested

| Item | Version |
| --- | --- |
| FMOD Engine | 2.03.15 HTML5 SDK (`fmodstudioapi20315html5.zip`) |
| Godot | 4.6-stable editor and official export templates (`web_dlink_*`) |
| Emscripten | 4.0.11 (what Godot 4.6 uses; FMOD itself is built with 3.1.67) |
| godot-cpp | submodule commit `507ed9d`, `api_version=4.6` |
| Browser | Playwright Chromium 1194, headless, `--autoplay-policy=user-gesture-required` |

The probe (`src/`) mirrors what `FmodAudioServer` does: it creates and initializes the Studio system,
loads `Master`, `Master.strings` and `SFX` banks from `res://` through Godot `FileAccess` callbacks
(`FMOD_System_SetFileSystem` + `LoadBankFile`), starts `event:/Ambience/Country`, and calls
`FMOD_Studio_System_Update` from `_process` on the main thread. It meters the master channel group
to tell whether the mixer is actually producing signal.

## Results

| Variant | FMOD lib | Links | Loads | Plays after click | Plays after key only |
| --- | --- | --- | --- | --- | --- |
| `threads=no`, debug | `fmodstudioL_wasm.a` | yes | yes | yes (RMS ≈ 0.016) | **no** |
| `threads=no`, release | `fmodstudio_wasm.a` | yes | yes | yes (RMS ≈ 0.015) | not tested |
| `threads=yes`, debug | `fmodstudioPL_wasm.a` | yes | yes¹ | yes (RMS ≈ 0.017) | not tested |
| `threads=yes`, debug | `fmodstudioL_wasm.a` (non-pthread) | **no** | – | – | – |

¹ Only with `threads/emscripten_pool_size` raised to 16. At the default of 8 the page hangs
during `Studio_System_Initialize` (reproduced twice), see [Finding 4](#4-threaded-builds-need-a-bigger-emscripten-pthread-pool).

In every working variant, RMS was 0 and the timeline stayed at 0 before the gesture.
After a click the event timeline advanced in real time (about 1000 ms per second) and RMS rose to the values above.
Output type is `FMOD_OUTPUTTYPE_AUDIOWORKLET` in all cases. There were no page errors.

"Plays" means the FMOD mixer runs and produces non-zero signal on the master bus, paced by the browser's
audio clock. The container has no sound card, so nobody actually listened to it.

## Answers to the Phase 0 questions

1. **Does it link?** Yes. All FMOD objects in the `w32` archives are wasm object files (not LLVM bitcode)
   compiled as PIC: only `*_REL_SLEB`, `GLOBAL_INDEX` (GOT) and data `*_I32` relocations, with no absolute
   code relocations. The Emscripten 3.1.67 objects link fine with 4.0.11.
2. **Do FMOD's JS imports resolve?** Yes. FMOD's browser code is entirely `EM_ASM`, with no JS library
   functions. After linking, the side module needs only 74 symbols from the main module: standard
   libc/libc++ plus `emscripten_asm_const_int`, `emscripten_get_now` and `emscripten_log`. All of them
   are in Godot 4.6's `web_dlink_nothreads_debug` main module. However, one runtime fix-up is needed
   (Finding 3).
3. **Is a threaded variant possible?** Yes, which contradicts the plan's expectation. FMOD ships pthread (`P`) variants
   built with `+atomics,+bulk-memory`, and they link into `threads=yes` side modules. The non-`P` libs mark
   objects `-shared-mem`, so they cannot (`wasm-ld: --shared-memory is disallowed by fmod_eventmodel.o`).
4. **Does sound play after a user gesture?** Yes after a **click**. Not after a key press only (see Finding 5).

## Findings for the next phases

### 1. SDK layout and download (Phase 1, `fmod_installer.py`)

- Download: `path=files/fmodstudio/api/HTML5/`, `filename=fmodstudioapi{version}html5.zip` (zip, not tar).
- Static libs are in `api/{core,studio}/lib/w32/`, **not** `upstream/w32` as the plan says.
  The Studio archives already contain the core, so link only one Studio archive:

  | | non-threaded | threaded (pthread) |
  | --- | --- | --- |
  | debug / editor | `fmodstudioL_wasm.a` | `fmodstudioPL_wasm.a` |
  | release | `fmodstudio_wasm.a` | `fmodstudioP_wasm.a` |

  `*_js.a` is byte-identical to `*_wasm.a`. The `*_bindings.a` archives are the JS (embind) API and are not needed.
- Headers are in `api/core/inc` and `api/studio/inc`, as on other platforms.
- Example banks (used by this probe) are in `api/studio/examples/public/js/`.

### 2. Release library is the *reduced* core

`fmodstudio_wasm.a` / `fmodstudioP_wasm.a` include the reduced core. That core drops WAV/OGG/MP3/FLAC/…
loading via the Core API (only FSB in banks), envelope follower DSP, and non-linear interpolation.
To get the full core, FMOD's own docs say to ship the `L` build with logging turned off. The plugin
doesn't create Core sounds from files today, so the reduced build is probably fine. That's still a
product decision (open question 2).

### 3. Required shim: FMOD JS mixer exports

Without it, init succeeds but every audio callback throws
`TypeError: Module.FMOD_JS_MixerSlowpathFunction is not a function`, and nothing is mixed.

Cause: FMOD's `EM_ASM` code does `Module["cwrap"]('FMOD_JS_MixerSlowpathFunction', ...)`. For numeric
signatures, `cwrap` returns `Module["_FMOD_JS_..."]`. Godot's dynamic loader merges side-module exports into
its internal `wasmImports` table but never onto `Module`, so `cwrap` returns `undefined`.

Fix (see `publish_fmod_js_exports()` in `src/fmod_web_probe.cpp`): before `FMOD_Studio_System_Initialize`, run an
`EM_ASM` that copies `FMOD_JS_MixFunction`, `FMOD_JS_MixerSlowpathFunction` and
`FMOD_JS_MixerFastpathFunction` from `wasmImports` onto `Module` as `_FMOD_JS_*`. Side-module `EM_ASM`
bodies are `eval`ed inside the loader scope, so `wasmImports` is visible there. This relies on an Emscripten
internal name and should be re-checked when Godot changes its Emscripten version. The shim returns how many
exports it couldn't find, so a breakage can be detected and reported.

### 4. Threaded builds need a bigger Emscripten pthread pool

At init, FMOD's pthread build starts three threads (stream, Studio update, sample load). Godot's default
`threads/emscripten_pool_size` is 8, and Godot already uses part of that pool. With 8, `Studio_System_Initialize`
deadlocks on the main thread. With 16, everything works. A threaded variant should document this (and
COOP/COEP), or the export plugin should raise the pool size for threaded web exports. Single-threaded
stays the simpler default, since it's Godot's default and needs no special headers.

### 5. Audio unlock only listens for click/touchend

FMOD registers `click` and `touchend` listeners on `window` to resume its `AudioContext`. A key press alone
leaves audio suspended, which was verified. That confirms the Phase 2 item: on web, resume the mixer on the first
input event of *any* kind, or at least document that keyboard-only games need a click first.

### 6. Non-threaded behaviour and log noise

- In the non-threaded build FMOD forces `FMOD_STUDIO_INIT_SYNCHRONOUS_UPDATE`, `LOAD_FROM_UPDATE`,
  `FMOD_INIT_MIX_FROM_UPDATE` and `STREAM_FROM_UPDATE`. Calling `Studio_System_Update` once per frame from
  `_process` was enough. This matches the Phase 2 plan (no update thread on web).
- The non-threaded **logging** library prints `FMOD_OS_CriticalSection_Enter/Leave: cannot enter NULL
  critical section` warnings, about 350 per second while playing. The plugin routes FMOD warnings into
  Godot's error output, so web debug builds would be flooded. Phase 2 should filter this message or lower
  the debug level on web. The release and pthread builds don't print it.
- FMOD logs `Module.window = no` and falls back to the global `window`. That's fine on the main thread,
  so no override is needed.

### 7. Toolchain notes (Phase 1/4)

- `em-version` must be 4.0.11 to match Godot 4.6. Linking against FMOD's 3.1.67 objects caused no problems.
- `threads=yes` prints `em++: warning: -sSIDE_MODULE + pthreads is experimental`. That warning applies to
  every threaded GDExtension and isn't specific to FMOD.
- Side module sizes (probe plus godot-cpp plus FMOD): 6.9 MB debug, 5.0 MB release, 7.0 MB threaded debug.
- The `.gdextension` needs web entries pointing at the `.nothreads.wasm` files (the plugin uses
  `web.debug.nothreads.wasm32` / `web.release.nothreads.wasm32`, so a threaded export doesn't pick them up),
  plus `web.*.threads.wasm32` for threaded builds. Banks need an `include_filter` (`*.bank`) in the export preset.

## Reproducing

```sh
# 1. FMOD SDK: download fmodstudioapi20315html5.zip (FMOD account needed) and extract it.
# 2. Emscripten 4.0.11 (emsdk install/activate 4.0.11, then source emsdk_env.sh).
# 3. godot-cpp submodule initialized: git submodule update --init godot-cpp
cd misc/web_phase0
SDK=/path/to/fmodstudioapi20315html5
mkdir -p project/banks && cp $SDK/api/studio/examples/public/js/{Master.bank,Master.strings.bank,SFX.bank} project/banks/
scons platform=web threads=no  target=template_debug   api_version=4.6 fmod_sdk=$SDK
scons platform=web threads=no  target=template_release api_version=4.6 fmod_sdk=$SDK
scons platform=web threads=yes target=template_debug   api_version=4.6 fmod_sdk=$SDK
# 4. Export with Godot 4.6 (export templates installed). The "No GDExtension library found for
#    linux.x86_64" errors during export are expected: the probe is built for web only.
mkdir -p build/nothreads build/nothreads_release build/threads
godot --headless --path project --import
godot --headless --path project --export-debug Web
godot --headless --path project --export-release Web $PWD/build/nothreads_release/index.html
godot --headless --path project --export-debug WebThreads
# 5. Run in headless Chromium (Playwright).
NODE_PATH=$(npm root -g) node run_probe.cjs build/nothreads
NODE_PATH=$(npm root -g) node run_probe.cjs build/nothreads_release
NODE_PATH=$(npm root -g) node run_probe.cjs build/threads --threads
NODE_PATH=$(npm root -g) node run_probe.cjs build/nothreads --gesture=key
```

## Not covered

- Firefox and Safari. Only Chromium was available here, and Phase 5 covers the other browsers.
- Listening to the output, since there was no audio device. The evidence is the master-bus meter and the event timeline.
- The real plugin code path: the update thread, `FmodEventAudioStream`, 2D/3D emitters. That's Phase 2.
