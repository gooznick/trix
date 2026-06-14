<p align="center">
  <img src="doc/logo.svg" alt="trix logo" width="320"/>
</p>

# trix — TRacers mIX

A unified C/C++ tracing API. Instrument your code once, choose the backend at runtime.

```cpp
#include <trix/trix.h>

{
    uint64_t frame(0);
    while (true)
    {
        TRIX_FRAME_SCOPE(frame);
        {
            myapp::Frame currentFrame;
            myapp::getFrame(currentFrame, frame);
            {
                TRIX_ALGO_SCOPE("encode");
                myapp::encode(currentFrame);
            }
            TRIX_DATA_FLOAT("std", currentFrame->Std());
            TRIX_DATA_INT("features", currentFrame->Features());
        } 
    }
} 
```

```bash
TRIX_BACKEND=ftrace  ./myapp   # Linux kernel tracer
TRIX_BACKEND=perf    ./myapp   # perf SDT probes
TRIX_BACKEND=lttng   ./myapp   # LTTng UST
TRIX_BACKEND=itt     ./myapp   # Intel VTune (Linux/Windows)
TRIX_BACKEND=atrace  ./myapp   # Android/file-based atrace
TRIX_BACKEND=etw     myapp.exe # Windows ETW
```

---

## How it works

- **Zero cost when disabled** — if `TRIX_ENABLED` is not defined, all calls are `static inline` no-ops. No library, no overhead.
- **Single shared library** — when enabled, link against `libtrix.so` / `trix.dll`. All backends are compiled in; the backend is selected at runtime via `TRIX_BACKEND`.
- **No recompile to switch backends** — change the env var, rerun.

---

## API

### Macros (C and C++)

```c
TRIX_FRAME_BEGIN(n)      TRIX_FRAME_END(n)
TRIX_ALGO_BEGIN(name)    TRIX_ALGO_END(name)
TRIX_DATA_INT(k, v)      TRIX_DATA_FLOAT(k, v)    TRIX_DATA_STRING(k, v)
```

### C++ RAII

```cpp
#include <trix/trix.h>

void process(uint64_t frame) {
    TRIX_FRAME_SCOPE(frame);       // calls TRIX_FRAME_BEGIN/END on scope exit

    {
        TRIX_ALGO_SCOPE("decode"); // calls TRIX_ALGO_BEGIN/END on scope exit
        TRIX_DATA_STRING("codec", "h264");
    }
}
```

---

## Backends

| Backend | Platform | Viewer | External dep |
|---------|----------|--------|-------------|
| `ftrace` | Linux | `cat /sys/kernel/tracing/trace`, trace-cmd, kernelshark | none (kernel) |
| `perf`   | Linux | `perf script` | `perf` tool |
| `lttng`  | Linux | `lttng view`, Babeltrace, Trace Compass | `liblttng-ust-dev` (auto-detected at configure time) |
| `itt`    | Linux + Windows | Intel VTune | VTune (optional — no-op if absent) |
| `etw`    | Windows | WPA, logman+tracerpt | none (built into Windows) |
| `atrace` | Linux + Windows | Perfetto UI | none (compiled in by default) |

---

## Build

```bash
cmake -B build
cmake --build build
```

Disable a backend:
```bash
cmake -B build -DTRIX_BACKEND_PERF=OFF
```

Requires CMake 3.16+. ITT backend sources (`third_party/ittapi/`) are bundled
in the repository (from [github.com/intel/ittapi](https://github.com/intel/ittapi))
and compiled directly into the library — no separate install needed.

---

## Install

Install to the system default prefix (`/usr/local` on Linux):

```bash
cmake --install build
```

Or to a custom prefix:

```bash
cmake --install build --prefix /path/to/prefix
```

What gets installed:

```
<prefix>/
  include/trix/trix.h
  include/trix/trix_version.h
  lib/libtrix.so          # Linux (also libtrix.so.1, libtrix.so.1.x.x)
  bin/trix.dll            # Windows
  lib/cmake/trix/
    TrixConfig.cmake
    TrixConfigVersion.cmake
    TrixTargets.cmake
```

---

## Usage in your project

After installing, point CMake at the prefix and use `find_package`:

```cmake
find_package(Trix REQUIRED)
target_link_libraries(myapp PRIVATE Trix::trix)
target_compile_definitions(myapp PRIVATE TRIX_ENABLED)
```

```bash
# If installed to a custom prefix, tell CMake where to look:
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/prefix
```

At runtime, make sure the library is on the dynamic linker path:

```bash
# Linux
export LD_LIBRARY_PATH=/path/to/prefix/lib:$LD_LIBRARY_PATH

# Windows — add the install bin\ directory to PATH
set PATH=C:\path\to\prefix\bin;%PATH%
```

Or manually, without installing:
```cmake
target_include_directories(myapp PRIVATE /path/to/trix/include)
target_link_libraries(myapp PRIVATE /path/to/trix/build/libtrix.so)
target_compile_definitions(myapp PRIVATE TRIX_ENABLED)
```

---

## Documentation

- [doc/tracing.md](doc/tracing.md) — tracing guide: instrument your code, backend comparison (overhead, strings, viewers, privilege), links to per-backend and per-viewer references
- [doc/example.md](doc/example.md) — step-by-step build and run examples for every backend

---

## Smoke tester

`trix_smoke` is a dependency-free C++11 executable that exercises every trix API call from multiple threads. Use it to verify a backend is installed and working before starting a real trace session.

```bash
cmake --build build --target trix_smoke
TRIX_BACKEND=ftrace ./build/smoke/trix_smoke   # prints "trix smoke: OK"
```

Built by default; disable with `-DTRIX_BUILD_SMOKE=OFF`.
