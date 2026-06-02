# trix — Copilot Instructions

## Project overview

trix (TRacers mIX) is a C/C++ unified tracing library. It provides a single `trix.h` header and a single shared library (`libtrix.so` / `trix.dll`) that compiles in all supported backends. The library selects its backend at runtime via an environment variable.

Two distinct artifacts:
- **`trix.h`** — public header used by end-user code. When `TRIX_ENABLED` is not defined, every call is a `static inline` no-op and no library is needed.
- **`libtrix.so` / `trix.dll`** — contains all backends compiled in. Used only when `TRIX_ENABLED` is defined.

## Build

```bash
# Standard configure + build
cmake -B build
cmake --build build

# With a specific backend disabled
cmake -B build -DTRIX_BACKEND_LTTNG=OFF
cmake --build build

# Build and run tests
cmake --build build && ctest --test-dir build

# Run a single test
ctest --test-dir build -R basic          # runs test_basic
ctest --test-dir build -R cpp            # runs test_cpp
ctest --test-dir build -R version        # runs test_version
ctest --test-dir build -R nop            # runs test_nop
```

Tests are in `tests/`. They link against `libtrix` with `TRIX_ENABLED` defined, except `test_nop.c` which tests the no-op path (no `TRIX_ENABLED`, no library link).

## Architecture

### Dispatch flow

`trix_dispatch.c` is the core of the library. At load time (Linux `__attribute__((constructor))`, Windows `DllMain`), it reads `TRIX_BACKEND`, calls the matching `trix_backend_<name>_init()` function, and copies the returned vtable into the single global `g_trix_vtable`. All public `trix_*` functions are thin one-liners that call through `g_trix_vtable`. There is no per-call branching after startup.

### Backend contract

Each backend lives in `src/backends/<name>/backend_<name>.c` and must implement one function:

```c
const trix_vtable_t* trix_backend_<name>_init(void);
```

It returns a pointer to a static `trix_vtable_t` with all 7 function pointers filled (frame_begin, frame_end, algo_begin, algo_end, data_int, data_float, data_string). `trix_internal.h` defines `trix_vtable_t` and `trix_backend_init_fn` — this header is the only internal header shared between dispatch and backends.

`trix_version()` is an 8th public symbol but is **not** part of the vtable — it's implemented directly in `trix_dispatch.c` returning `TRIX_VERSION_STRING`.

### Adding a new backend

1. Create `src/backends/<name>/backend_<name>.c` implementing all 7 vtable slots.
2. Add a CMake option `TRIX_BACKEND_<NAME>` in `CMakeLists.txt`, guard it behind the right platform check, and append source + define to `TRIX_SOURCES` / `TRIX_DEFINES`.
3. Add a forward declaration and `else if` branch in `trix_dispatch.c`.
4. The backend string used in `TRIX_BACKEND=<name>` must match the `strcmp` in `trix_dispatch.c`.

### Version

Version is defined solely in `include/trix/trix_version.h`. When bumping, update all four items in that file: `TRIX_VERSION_MAJOR`, `TRIX_VERSION_MINOR`, `TRIX_VERSION_PATCH`, and `TRIX_VERSION_STRING` (the hardcoded string must stay in sync). `CMakeLists.txt` reads the three numeric defines at configure time — **never set the version elsewhere**.

### Changelog

`CHANGELOG.md` follows [Keep a Changelog](https://keepachangelog.com) conventions. Rules:

- **Only user-facing changes** — skip internal refactors (file renames, `.c`→`.cpp`, test rewrites, comment-only edits) unless they affect users.
- **Sections**: use `### Added`, `### Changed`, `### Fixed`, `### Removed` — only include sections that apply.
- **Version bump policy**: patch (`1.x.Y`) for additions/fixes that don't change existing behaviour; minor (`1.X.0`) for new backends, new public API, or behaviour changes.
- **Date**: use the actual commit date (`YYYY-MM-DD`).
- Add the new entry at the **top** of the file, immediately after the intro line.
- Each entry should be 1–3 concise bullet points. Mention the affected file or API only when it helps the user find it.

## Key conventions

- **No `TRIX_ENABLED` in the library itself** — the library always defines all 7 public symbols. `TRIX_ENABLED` is only for end-user code.
- **`TRIX_BUILDING_DLL`** is defined when compiling the library (controls `TRIX_API` dllexport/dllimport on Windows).
- **Unknown `TRIX_BACKEND` values abort()** — this is intentional (fail fast). Do not silently fall back.
- **`TRIX_QUIET` env var** suppresses the startup diagnostic line printed to stderr.
- **LTTng API version** — lttng-ust ≥ 2.14 uses a different API; the backend guards on `TRIX_LTTNG_UST_NEW_API` (set by CMake).
- **ETW backend** is compiled as C++ (`set_source_files_properties ... LANGUAGE CXX`) because some Windows SDK headers require it.
- **ITT** — ittapi source (`third_party/ittapi/src/ittnotify/ittnotify_static.c`) is compiled directly into `libtrix`. If the submodule is absent, CMake fetches it from GitHub automatically.
- **C++ wrappers** (`trix::Scoped<>`, `ScopedAlgo`, `ScopedFrame`) and the `TRIX_ALGO_SCOPE` / `TRIX_FRAME_SCOPE` macros live entirely in `trix.h` — no `.cpp` files needed.

## Backends

| Name     | Platform      | Notes |
|----------|---------------|-------|
| `ftrace` | Linux         | Writes to tracefs (`/sys/kernel/tracing` or `/sys/kernel/debug/tracing`) |
| `perf`   | Linux         | Uses SDT probes |
| `itt`    | Linux/Windows | Intel VTune; no-op if VTune not running |
| `etw`    | Windows       | TraceLogging API — no XML manifest needed |
| `lttng`  | Linux         | Requires `liblttng-ust-dev` at build time |
| `atrace` | Linux/Windows | File-based |
| `nop`    | All           | Always compiled in; used when `TRIX_BACKEND` is unset |

## Capture scripts

`scripts/` contains paired `capture_<backend>_pre.sh` / `capture_<backend>_post.sh` shell scripts for ftrace, perf, lttng, and vtune. Conversion scripts `perf_to_perfetto.py` and `lttng_to_perfetto.py` produce Perfetto-compatible traces. `perfetto-compose/` contains a local Perfetto UI setup.
