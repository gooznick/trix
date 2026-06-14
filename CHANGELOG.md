# Changelog

All notable changes to trix are documented here.

## [1.4.2] — 2026-06-14

### Added

- `smoke/` — dependency-free C++11 smoke tester (`trix_smoke`). Spawns worker
  threads that call every trix API function; use it to verify a backend is
  compiled in and dispatching correctly. Built by default (`TRIX_BUILD_SMOKE=ON`).

## [1.4.1] — 2026-06-02

### Added

- `trix_build_info()`: new API function returning a greppable string with the
  version and compiled-in backends, e.g. `trix/1.4.1 backends=[ftrace perf nop]`.
  Inspect without running the library: `strings libtrix.so | grep "^trix/"`

## [1.4.0] — 2026-06-02

### Changed

- **ITT backend**: `trix_data_int` / `trix_data_float` now use a lock-free
  counter cache — each unique key maps to exactly one counter track in the
  VTune timeline. Previously a new counter was created on every call, producing
  duplicate tracks. Hot path (cache hit) is mutex-free.
- **atrace backend**: all writes are now lock-free (uses `O_APPEND` atomic
  kernel writes); removed the write mutex from the hot path.

### Build

- New `TRIX_BUILD_TESTS` CMake option (default `ON`) — set to `OFF` to skip
  test compilation when embedding trix as a subdirectory.

## [1.3.1] — 2026-06-01

### Added

- `doc/docker.md`: new guide for using trix in Docker / containerised environments, covering all backends and the recommended ftrace-on-host approach for microservices.

## [1.3.0] — 2026-05-31

### Changed

- perf backend: string args now packed as uint64 (first 8 chars, little-endian) instead of raw pointers — gives readable names in traces.
- `capture_perf_pre.sh`: now starts system-wide `perf record -a` in background; user runs app separately (like ftrace pattern).
- `capture_perf_post.sh`: now stops recording, exports, and runs as sudo.

### Added

- perf: `data_string` events recorded and shown as a dedicated "Data" track in Perfetto.
- `perf_to_perfetto.py`: `unpack_str8`, `bits_to_float`, `data_string` handler, "Data" process track.
- demo: `TRIX_DATA_INT`, `TRIX_DATA_FLOAT`, `TRIX_DATA_STRING` per frame; sub-pixel NCC refinement for float translation estimates.
- `doc/backends/perf.md`: full perf/SDT backend guide with Perfetto screenshot.

## [1.2.0] — 2026-05-31

### Documentation

- `doc/backends/vtune.md`: new VTune/ITT backend guide with screenshots.

### Backends

- `src/backends/itt/backend_itt.c`: `trix_data_int` / `trix_data_float` now use
  `__itt_counter_create_typed` + `__itt_counter_set_value` — visible as counter
  tracks in the VTune Timeline.
- `src/trix_dispatch.c`: startup line now prints `TRIX_BACKEND=` instead of `backend=`.

### Scripts

- `scripts/capture_vtune.sh`: replaces the old pre/post pair — launches VTune + app
  via `vtune -- <cmd>` and returns immediately.
- `scripts/capture_vtune_pre.sh`, `scripts/capture_vtune_post.sh`: removed.

### Build

- `demo/CMakeLists.txt`: always emit debug info (`-g` / `/Zi`) regardless of build type.

---

## [1.1.3] — 2026-05-31

### Tools

- `tools/perfetto-ui/Dockerfile`: remove password-protected test zip files
  (`Different Encryptions.zip`, `test_encrypted.zip`) left over from zlib test data.

---

## [1.1.2] — 2026-05-24

### Documentation

- `doc/viewers/perfetto.md`: internal network section rewritten — provides
  `tools/perfetto-ui/Dockerfile` that builds the Perfetto v55.3 web UI;
  run with `docker run --network=host perfetto-ui:latest`, open `http://localhost:10000`.

### Tools

- `tools/perfetto-ui/Dockerfile`: self-contained Docker build for the Perfetto UI.

---

## [1.1.1] — 2026-05-24

### Startup diagnostics

- trix prints a one-line summary to stderr on init:
  `trix 1.1.1  backend=ftrace    available=[ftrace perf lttng ]`
- `TRIX_QUIET=1` suppresses the print.
- Unknown-backend error now includes the available backends list.

### Demo

- `--cpus N` flag: restrict all threads to CPUs 0 through N−1 (default: 3; 0 = no restriction).
  Cross-platform: Linux uses `pthread_setaffinity_np`, Windows uses `SetThreadAffinityMask`.

### Scripts

- `capture_ftrace_pre.sh` / `capture_ftrace_post.sh` simplified: no command argument;
  pre sets up tracefs and starts tracing, post stops and saves `trix_ftrace_YYYYMMDD_HHMMSS.txt`.
- `capture_ftrace.sh` removed.

### Documentation

- New `doc/backends/ftrace.md`: ftrace guide — kernel config (Ubuntu, Yocto), Docker,
  capture with/without trix, Perfetto visualisation, wire format reference.
- New `doc/viewers/perfetto.md`: Perfetto guide — navigation, supported formats,
  self-hosting on internal networks via perfetto-compose.
- New `doc/tracing.md`: main tracing guide with backend comparison table.

---

## [1.1.0] — 2026-05-17

### New backends

- **LTTng-UST** (`TRIX_BACKEND=lttng`) — userspace tracing via LTTng.
  Full string names in traces (values copied at record time).
  No root required for UST events; kernel `sched_switch` available with
  `lttng-modules-dkms` + root.

- **atrace/file** (`TRIX_BACKEND=atrace`) — writes the atrace/systrace wire
  format directly to a regular file (`trix_YYYYMMDD_HHMMSS.atrace`).
  No root, no kernel modules, no tools required.
  Output can be opened directly in Perfetto (drag and drop).
  Output path override: `TRIX_FILE_OUT=/path/to/file.atrace`.

### New tooling — Perfetto converters (`scripts/`)

- **`perf_to_perfetto.py`** — converts `perf script` text output to Perfetto
  Chrome trace JSON.  Reconstructs LIFO spans per TID, generates CPU lane
  tracks from `sched:sched_switch`, handles `data_int`/`data_float` counters.
  Algo names are stable short labels (`algo_0`, `algo_1`, …) with a pointer
  legend printed to stdout.

- **`lttng_to_perfetto.py`** — converts `babeltrace2` text output to Perfetto
  Chrome trace JSON.  Full string names (no pointer resolution needed).
  Handles `sched_switch` CPU lanes if kernel events were captured.

### New capture scripts (`scripts/`) — pre/post split pattern

All backends now follow the same three-script pattern:

| Script | Does |
|--------|------|
| `capture_X_pre.sh`  | Sets up backend, runs command, stays active after exit |
| `capture_X_post.sh` | Stops collection, saves output, prints converter hint |
| `capture_X.sh`      | All-in-one wrapper calling pre then post |

- **`capture_ftrace_{pre,post,}.sh`** — ftrace capture with pre/post split.
  `pre` survives Ctrl+C (tracing continues); `post` saves the trace file.

- **`capture_perf_{pre,post,}.sh`** — perf capture.
  `pre` registers SDT probes and runs `perf record`.
  `post` exports `.data` → `.txt` and prints the converter hint.

- **`capture_lttng_{pre,post,}.sh`** — LTTng-UST capture.
  `pre` creates session, enables `trix:*` events + optional kernel
  `sched_switch`, runs command.
  `post` stops session, exports CTF → text via `babeltrace2`, prints hint.

- **`capture_vtune_{pre,post,}.sh`** — VTune threading analysis capture.
  Collects ITT tasks, context switches, CPU samples and call stacks.
  `post` prints `vtune-gui` open command.
  Requires `ptrace_scope=0`: `echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope`.

### Documentation

- `doc/demo_walkthrough.md` — full end-to-end guide covering all backends,
  Perfetto visualisation, air-gapped usage, and troubleshooting.
- `doc/screenshots/` — Perfetto screenshots for ftrace and perf outputs.

### Bug fixes / improvements

- `trix_version.h`: fix `TRIX_VERSION_STRING` to match `MAJOR.MINOR.PATCH`.
- `capture_vtune_pre.sh`: fail early if `libtrix.so` not found or
  `ptrace_scope > 0`, with clear fix instructions.
- `capture_lttng.sh`: gracefully skips kernel `sched_switch` if
  `lttng-modules-dkms` is not installed.
- `.gitignore`: exclude trace output files (`*.atrace`, `*.data`, `*.txt`,
  `*.pftrace`, `trix_vtune_*/`).

---

## [1.0.0] — initial release

- Core `trix.h` API: `trix_frame_begin/end`, `trix_algo_begin/end`,
  `trix_data_int/float/string`, C++ `trix::Scoped<>` wrapper.
- Backends: **ftrace**, **perf** (SDT probes), **ITT** (VTune), **ETW** (Windows).
- Single shared library (`libtrix.so` / `trix.dll`) with all backends compiled in.
- `TRIX_ENABLED` compile guard: zero-overhead no-op when disabled.
- CMake build with per-backend options.
- Multithreaded image-pipeline demo (`demo/trix_demo`).
