# Changelog

All notable changes to trix are documented here.

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
