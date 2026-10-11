# Web export feasibility spike (Phase 0)

Throwaway harness for the go/no-go questions in [issue #1](https://github.com/luc4s/FMOD-GODOT/issues/1).
It is independent of the plugin sources and can be deleted once Phase 0 is decided.

| Question | Checked by |
|----------|-----------|
| Q1: do FMOD's HTML5 `.a` libraries link into a Godot side module (`-fPIC`)? | `tools/link_probe.sh` |
| Q2: does Godot's dlink main module provide everything FMOD imports? | `tools/wasm_imports.py` (static), browser run (runtime) |
| Q3: is a threaded build possible? | `tools/link_probe.sh` + threaded browser run (`SPIKE_THREADS=1`) |
| Q4: does FMOD produce sound in the browser, and does suspend/resume (the gesture unlock) work? | `tools/browser_test.cjs` |

## Running it

Requirements: Emscripten **4.0.11** (the version Godot 4.6 is built with), SCons, Node with
Playwright + Chromium, Python 3, `unzip`. The Godot 4.6 editor and the official `web_dlink_*`
templates are downloaded automatically (`tools/fetch_godot.sh`) unless `GODOT_DIR` is set.

```sh
git submodule update --init godot-cpp
cd misc/web_spike
EMSDK_ENV=~/emsdk/emsdk_env.sh SPIKE_THREADS=1 ./run.sh ~/Downloads/fmodstudioapi20315html5.zip
```

`run.sh` takes the FMOD Studio API **HTML5** download (archive or extracted folder). By default it
links `fmodstudioL_wasm.a,fmodL_wasm.a` (override with `FMOD_LIBS`) and tests with the `.bank`
files shipped in the FMOD archive (override with `BANKS_DIR`). Banks built for desktop may use
codecs the web build lacks, so prefer banks built for the HTML5 platform.

Results go to `build/report/`:

- `link_probe.txt`: which archives define the core/studio API, their wasm target features, and a
  side-module link (with and without threads) for each library combination, with import checks.
- `<variant>/imports.txt`: side-module imports not provided by the Godot template (heuristic).
- `<variant>/browser.json`: every step's FMOD result, master-bus RMS before and after the click,
  AudioContext states, console errors, and a `verdict` block.

## Caveats

- Headless Chromium auto-allows audio, so the real "blocked until user gesture" behaviour can't be
  reproduced there. `project/main.gd` emulates it by suspending the FMOD mixer after starting
  playback and resuming on the first input. Check the real autoplay behaviour manually by serving
  `build/web_nothreads/` and opening it in a normal browser.
- wasm-ld accepts objects without atomics in a `-pthread` side module, so a threaded "LINK OK"
  alone doesn't answer Q3; the threaded browser run does.

## Harness validation (`mock/`)

`mock/` contains a stand-in FMOD (same C API subset, same SDK folder layout) used only to prove
the harness works before the real SDK is available. It reads banks through the registered file
callbacks and plays a tone through Web Audio from `EM_JS`, like a JS-backed library would.
`mock/build_mock.sh` also builds a non-PIC variant (`mock/api_nopic`) to check that Q1 failures
are detected.

```sh
./mock/build_mock.sh
BANKS_DIR=<any folder with *.bank files> SPIKE_THREADS=1 ./run.sh mock/api
```

Mock results say nothing about FMOD itself.
