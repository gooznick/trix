<p align="center">
  <img src="doc/logo.svg" alt="trix logo" width="320"/>
</p>

# trix — TRacers mIX

A unified C/C++ tracing API. Instrument your code once, choose the backend at runtime.

```cpp
#include <trix/trix.h>

{
    TRIX_FRAME_SCOPE(42);
    {
        TRIX_ALGO_SCOPE("encode");
        trix_data_int("width", 1920);
        trix_data_float("fps", 29.97f);
    } // trix_algo_end("encode")
} // trix_frame_end(42)
```

```bash
TRIX_BACKEND=ftrace  ./myapp   # Linux kernel tracer
TRIX_BACKEND=perf    ./myapp   # perf SDT probes
TRIX_BACKEND=itt     ./myapp   # Intel VTune
TRIX_BACKEND=etw     myapp.exe # Windows ETW
```

---

## How it works

- **Zero cost when disabled** — if `TRIX_ENABLED` is not defined, all calls are `static inline` no-ops. No library, no overhead.
- **Single shared library** — when enabled, link against `libtrix.so` / `trix.dll`. All backends are compiled in; the backend is selected at runtime via `TRIX_BACKEND`.
- **No recompile to switch backends** — change the env var, rerun.

---

## API

### C

```c
void trix_frame_begin(uint64_t frame_num);
void trix_frame_end  (uint64_t frame_num);
void trix_algo_begin (const char* name);
void trix_algo_end   (const char* name);
void trix_data_int   (const char* key, uint64_t value);
void trix_data_float (const char* key, float value);
void trix_data_string(const char* key, const char* value);
```

### Macros

```c
TRIX_FRAME_BEGIN(n)      TRIX_FRAME_END(n)
TRIX_ALGO_BEGIN(name)    TRIX_ALGO_END(name)
TRIX_DATA_INT(k, v)      TRIX_DATA_FLOAT(k, v)    TRIX_DATA_STRING(k, v)
```

### C++ RAII

```cpp
#include <trix/trix.h>

void process(uint64_t frame) {
    TRIX_FRAME_SCOPE(frame);       // calls trix_frame_begin/end

    {
        TRIX_ALGO_SCOPE("decode"); // calls trix_algo_begin/end on scope exit
        trix_data_string("codec", "h264");
    }
}
```

---

## Backends

| Backend | Platform | Viewer | External dep |
|---------|----------|--------|-------------|
| `ftrace` | Linux | `cat /sys/kernel/tracing/trace`, trace-cmd, kernelshark | none (kernel) |
| `perf`   | Linux | `perf script` | `perf` tool |
| `itt`    | Linux + Windows | Intel VTune | VTune (optional — no-op if absent) |
| `etw`    | Windows | WPA, logman+tracerpt | none (built into Windows) |

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

Requires CMake 3.16+. ITT backend sources are fetched automatically if not present.

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

- [doc/example.md](doc/example.md) — step-by-step guide: build, run, activate each backend, view output, install requirements for VTune and ETW
