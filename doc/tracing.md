# trix Tracing Guide

- [Instrumenting your code](#instrumenting-your-code)
- [Enabling tracing at compile time](#enabling-tracing-at-compile-time)
- [Choosing a backend](#choosing-a-backend)
- [Backend reference](#backend-reference)
- [Viewers](#viewers)

---

## Instrumenting your code

Include `trix/trix.h` and use the macros anywhere in your C or C++ code:

```cpp
#include <trix/trix.h>

void process(uint64_t frame) {
    TRIX_FRAME_BEGIN(frame);

    TRIX_ALGO_BEGIN("decode");
    TRIX_DATA_STRING("codec", "h264");
    TRIX_DATA_INT("width", 1920);
    TRIX_DATA_FLOAT("fps", 29.97f);
    TRIX_ALGO_END("decode");

    TRIX_FRAME_END(frame);
}
```

In C++ prefer the RAII scope helpers — they call begin/end even if an exception is thrown:

```cpp
void process(uint64_t frame) {
    TRIX_FRAME_SCOPE(frame);        // frame_begin on entry, frame_end on exit

    {
        TRIX_ALGO_SCOPE("decode");  // algo_begin on entry, algo_end on exit
        TRIX_DATA_STRING("codec", "h264");
    }
}
```

### Full macro reference

| Macro | Meaning |
|-------|---------|
| `TRIX_FRAME_BEGIN(n)` / `TRIX_FRAME_END(n)` | Mark the start and end of frame `n` |
| `TRIX_ALGO_BEGIN(name)` / `TRIX_ALGO_END(name)` | Mark the start and end of a named algorithm step |
| `TRIX_FRAME_SCOPE(n)` | RAII — calls `TRIX_FRAME_BEGIN` / `TRIX_FRAME_END` |
| `TRIX_ALGO_SCOPE(name)` | RAII — calls `TRIX_ALGO_BEGIN` / `TRIX_ALGO_END` |
| `TRIX_DATA_INT(key, value)` | Emit an integer data point |
| `TRIX_DATA_FLOAT(key, value)` | Emit a float data point |
| `TRIX_DATA_STRING(key, value)` | Emit a string data point |

---

## Enabling tracing at compile time

When `TRIX_ENABLED` is **not** defined (the default), every macro is a `static inline` no-op — zero overhead, no library needed.

To enable tracing:

1. Define `TRIX_ENABLED` when compiling your application.
2. Link against `libtrix.so` (Linux) or `trix.dll` (Windows).

With CMake and `find_package`:

```cmake
find_package(Trix REQUIRED)
target_link_libraries(myapp PRIVATE Trix::trix)
target_compile_definitions(myapp PRIVATE TRIX_ENABLED)
```

At runtime, select the backend via the `TRIX_BACKEND` environment variable:

```bash
TRIX_BACKEND=ftrace  ./myapp   # Linux kernel tracer
TRIX_BACKEND=perf    ./myapp   # perf SDT probes
TRIX_BACKEND=lttng   ./myapp   # LTTng-UST
TRIX_BACKEND=itt     ./myapp   # Intel VTune
TRIX_BACKEND=etw     myapp.exe # Windows ETW
```

If `TRIX_BACKEND` is unset, all calls are silent no-ops at runtime (library is loaded but does nothing). An unknown value causes an immediate `abort()` — this is intentional.

---

## Choosing a backend

### Comparison

| | ftrace | perf | lttng | itt | etw |
|---|---|---|---|---|---|
| **Platform** | Linux | Linux | Linux | Linux + Windows | Windows |
| **Build dep** | none | none | `liblttng-ust-dev` | ittapi (auto-fetched) | Windows SDK |
| **Runtime dep** | kernel tracefs | `perf` tool | `lttng-tools` | VTune (optional) | none |
| **Root / privilege** | yes (tracefs write) | yes (or `perf_event_paranoid=1`) | no (userspace only) | no | no |
| **Overhead** | ~1 µs per event (write syscall) | near zero when idle; syscall when recording | sub-µs (lock-free ring buffer) | near zero when VTune absent | low (ETW kernel ring buffer) |
| **String names** | full | pointers only | full | full | full |
| **Best viewer** | Perfetto | Perfetto | Trace Compass, Perfetto | VTune | WPA |
| **Scheduling / CPU lanes** | yes (sched events) | yes (sched events) | yes (with kernel module) | yes (VTune built-in) | yes (ETW built-in) |

### When to pick each backend

**ftrace** — the default choice on Linux. No installation, native Perfetto support, full string names, scheduling events. Needs root.

**perf** — prefer when the target has `perf` but tracefs is unavailable or inconvenient. Near-zero idle overhead (NOP-patched probes). Downside: algo names are recorded as pointers — a converter script resolves them using the binary's string table.

**lttng** — best when root is unavailable. Userspace tracing runs without any special privilege. Structured CTF binary format with nanosecond timestamps; Trace Compass gives the richest GUI for CTF data. Requires `liblttng-ust-dev` at build time and `lttng-tools` at runtime.

**itt** — best when you already use Intel VTune. The library is a no-op if VTune is not attached, so you can leave it enabled permanently in development builds. Works on both Linux and Windows. Provides the richest analysis: call stacks, thread states, CPU utilisation, and hotspot identification alongside the trix spans.

**etw** — the Windows equivalent of ftrace. No installation, built into the OS, viewers range from the free `logman`/`tracerpt` command-line tools to the full Windows Performance Analyzer (WPA) GUI.

---

## Backend reference

Each backend has its own setup, capture, and output reference:

- [ftrace](backends/ftrace.md) — Linux kernel tracer, Perfetto viewer
- [perf](backends/perf.md) — Linux perf SDT probes, Perfetto viewer
- [lttng](backends/lttng.md) — LTTng-UST userspace, Trace Compass / Perfetto viewer
- [itt](backends/itt.md) — Intel VTune ITT tasks, Linux and Windows
- [etw](backends/etw.md) — Windows Event Tracing, WPA viewer

---

## Viewers

- [Perfetto](viewers/perfetto.md) — browser-based trace viewer, works with ftrace, perf, and LTTng traces
- [VTune](viewers/vtune.md) — Intel profiler GUI, used with the `itt` backend
- [Trace Compass](viewers/trace-compass.md) — Eclipse-based CTF viewer, used with the `lttng` backend
