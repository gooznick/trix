# trix — Build and Run Examples

## Prerequisites

### Build tools (required)
```
apt install cmake gcc g++ make
```

### Per-backend (optional — only needed at runtime, not to compile)
| Backend | Build dependency | Runtime requirement |
|---------|-----------------|---------------------|
| ftrace  | none            | Root or write access to tracefs |
| perf    | none            | `perf` tool (`apt install linux-tools-$(uname -r)`) |
| itt     | none (fetched)  | Intel VTune installed and running |
| etw     | Windows only — built into the OS | — |
| lttng   | `liblttng-ust-dev` | `lttng-tools` (`apt install lttng-tools`) |

---

## Build

```bash
cmake -B build
cmake --build build
```

To enable the LTTng backend (requires `liblttng-ust-dev`):
```bash
apt install liblttng-ust-dev
cmake -B build -DTRIX_BACKEND_LTTNG=ON
cmake --build build
```

Example output:
```
-- Configuring done
-- Generating done
-- Build files have been written to: /path/to/trix/build
[ 53%] Linking C shared library libtrix.so
[100%] Built target trix
```

To disable a backend:
```bash
cmake -B build -DTRIX_BACKEND_FTRACE=OFF -DTRIX_BACKEND_PERF=OFF
```

---

## Running tests

```bash
ctest --test-dir build -V
```

```
test 1 - basic ...... Passed    0.00 sec
test 2 - cpp ........ Passed    0.00 sec
test 3 - nop ........ Passed    0.00 sec

100% tests passed, 0 tests failed out of 3
```

The library must be on the linker path:
```bash
export LD_LIBRARY_PATH=$PWD/build
```

---

## Backend selection

Set `TRIX_BACKEND` before running your application:

```bash
# No backend — silent no-op (default when unset)
./build/tests/test_basic

# ftrace backend
TRIX_BACKEND=ftrace ./build/tests/test_basic

# perf backend
TRIX_BACKEND=perf ./build/tests/test_basic

# itt backend (VTune must be running)
TRIX_BACKEND=itt ./build/tests/test_basic

# lttng backend (session must be running, see below)
TRIX_BACKEND=lttng ./build/tests/test_basic

# Unknown backend — crashes immediately with a message
TRIX_BACKEND=wrong ./build/tests/test_basic
# trix: unknown backend 'wrong'
# Aborted (core dumped)
```

---

## ftrace backend

The ftrace backend writes events to `trace_marker` using the **atrace/systrace
wire format**, which Perfetto renders as properly nested duration spans.

### Activate and capture

Requires root (or write access to `/sys/kernel/tracing/trace_marker`).

```bash
# Enable scheduling events
sudo sh -c 'echo 1 > /sys/kernel/tracing/events/sched/sched_switch/enable'
sudo sh -c 'echo 1 > /sys/kernel/tracing/events/sched/sched_wakeup/enable'
sudo sh -c 'echo 1 > /sys/kernel/tracing/events/sched/sched_migrate_task/enable'

# Clear and start
sudo sh -c 'echo > /sys/kernel/tracing/trace'
sudo sh -c 'echo 1 > /sys/kernel/tracing/tracing_on'

# Run your application
sudo env TRIX_BACKEND=ftrace ./test_basic

# Stop and save
sudo sh -c 'echo 0 > /sys/kernel/tracing/tracing_on'
sudo cat /sys/kernel/tracing/trace > trace.txt
```

### Raw output

```
# tracer: nop
#
#           TASK-PID     CPU#  |||||  TIMESTAMP  FUNCTION
#              | |         |   |||||     |         |
  test_basic-297239  [001] ..... 295851.622946: tracing_mark_write: B|297239|frame_0
  test_basic-297239  [001] ..... 295851.622952: tracing_mark_write: B|297239|encode
  test_basic-297239  [001] ..... 295851.622962: tracing_mark_write: C|297239|width|1920
  test_basic-297239  [001] ..... 295851.622964: tracing_mark_write: C|297239|height|1080
  test_basic-297239  [001] ..... 295851.623008: tracing_mark_write: C|297239|fps|29.97
  test_basic-297239  [001] ..... 295851.623011: tracing_mark_write: B|297239|codec=h264
  test_basic-297239  [001] ..... 295851.623011: tracing_mark_write: E|297239
  test_basic-297239  [001] ..... 295851.623013: tracing_mark_write: E|297239
  test_basic-297239  [001] ..... 295851.623016: tracing_mark_write: E|297239
  test_basic-297239  [001] ..... 295851.623018: tracing_mark_write: B|297239|frame_1
  test_basic-297239  [001] ..... 295851.623021: tracing_mark_write: B|297239|decode
  test_basic-297239  [001] ..... 295851.623023: tracing_mark_write: E|297239
  test_basic-297239  [001] ..... 295851.623030: tracing_mark_write: E|297239
```

`B|<pid>|<name>` = begin span, `E|<pid>` = end span (LIFO per thread), `C|<pid>|<key>|<value>` = counter.

### View with Perfetto (recommended)

Transfer `trace.txt` to any machine with a browser:

```
https://ui.perfetto.dev  →  Open trace file  →  drag and drop trace.txt
```

Perfetto shows:
- Per-CPU lanes with thread scheduling
- Per-thread lanes with nested trix spans (frame → algo → ...)
- Context switches (`sched_switch`), wakeups, CPU migrations

### View with trace-cmd / KernelShark (optional)

```bash
apt install trace-cmd kernelshark
sudo trace-cmd record -e 'ftrace:print' -e 'sched:sched_switch' \
    env TRIX_BACKEND=ftrace ./test_basic
kernelshark trace.dat
```

---

## perf backend

The perf backend uses SDT (Statically Defined Tracing) probes embedded in `libtrix.so`. No packages are needed to compile. At runtime you need the `perf` tool.

### Verify the probes are embedded in the library

```bash
readelf -n build/libtrix.so | grep -A4 stapsdt
```

```
  stapsdt              0x00000031	NT_STAPSDT (SystemTap probe descriptors)
    Provider: trix
    Name: frame_begin
    Location: 0x0000000000009b9c, Base: 0x00000000000150ff, Semaphore: 0x0000000000000000
    Arguments: -8@%rax
  stapsdt              0x00000030	NT_STAPSDT (SystemTap probe descriptors)
    Provider: trix
    Name: algo_begin
    ...
```

### Activate and capture

```bash
# Step 1 — register the library with perf's build-id cache
sudo perf buildid-cache --add build/libtrix.so

# Step 2 — register the SDT probes as traceable perf events
sudo perf probe --add 'sdt_trix:frame_begin'
sudo perf probe --add 'sdt_trix:frame_end'
sudo perf probe --add 'sdt_trix:algo_begin'
sudo perf probe --add 'sdt_trix:algo_end'
```

```
Added new event:
  sdt_trix:algo_begin  (on %algo_begin in /path/to/build/libtrix.so)
Added new event:
  sdt_trix:frame_begin (on %frame_begin in /path/to/build/libtrix.so)
...
```

```bash
# Step 3 — record
sudo env TRIX_BACKEND=perf LD_LIBRARY_PATH=$PWD/build \
    perf record -e 'sdt_trix:algo_begin' -e 'sdt_trix:algo_end' \
                -e 'sdt_trix:frame_begin' -e 'sdt_trix:frame_end' \
    -o /tmp/trix.data \
    ./build/tests/test_basic

# Step 4 — display
sudo perf script -i /tmp/trix.data
```

### Output

```
[ perf record: Captured and wrote 0.058 MB /tmp/trix.data (12 samples) ]

  test_basic  297388 [010] 295897.132370: sdt_trix:frame_begin: (7f...9b9c) arg1=0
  test_basic  297388 [010] 295897.132375:  sdt_trix:algo_begin: (7f...9bc4) arg1=98053797838874
  test_basic  297388 [010] 295897.132377:    sdt_trix:algo_end: (7f...9bd8) arg1=98053797838874
  test_basic  297388 [010] 295897.132379:   sdt_trix:frame_end: (7f...9bb0) arg1=0
  test_basic  297388 [010] 295897.132381: sdt_trix:frame_begin: (7f...9b9c) arg1=1
  test_basic  297388 [010] 295897.132383:  sdt_trix:algo_begin: (7f...9bc4) arg1=98053797838909
  test_basic  297388 [010] 295897.132385:    sdt_trix:algo_end: (7f...9bd8) arg1=98053797838909
  test_basic  297388 [010] 295897.132386:   sdt_trix:frame_end: (7f...9bb0) arg1=1
  test_basic  297388 [010] 295897.132389: sdt_trix:frame_begin: (7f...9b9c) arg1=2
  test_basic  297388 [010] 295897.132390:  sdt_trix:algo_begin: (7f...9bc4) arg1=98053797838916
  test_basic  297388 [010] 295897.132392:    sdt_trix:algo_end: (7f...9bd8) arg1=98053797838916
  test_basic  297388 [010] 295897.132393:   sdt_trix:frame_end: (7f...9bb0) arg1=2
```

Each line: process, PID, CPU, timestamp, probe name, probe location, `arg1` value.
For `frame_begin`/`frame_end`, `arg1` is the frame number.
For `algo_begin`/`algo_end`, `arg1` is the raw pointer to the name string.

### Clean up probes

```bash
sudo perf probe --del 'sdt_trix:*'
```

---

## ITT (VTune) backend

If VTune is not running, all ITT calls are no-ops — the application runs normally with no error.

### Source the VTune environment

```bash
source /home/user/intel/oneapi/vtune/2025.10/env/vars.sh
# or add to ~/.bashrc for permanent access
```

### Collect a threading analysis (ITT task/frame grouping)

```bash
vtune -collect threading \
      -knob sampling-and-waits=sw \
      -result-dir /tmp/trix_vtune \
      -- env TRIX_BACKEND=itt LD_LIBRARY_PATH=$PWD/build ./your_app
```

### Show summary in terminal

```bash
vtune -report summary -result-dir /tmp/trix_vtune
```

Example output:
```
vtune: Using result path `/tmp/trix_vtune'
vtune: Executing actions 75 % Generating a report
    Elapsed Time: 0.001s
    Paused Time: 0s
Effective CPU Utilization: 0.0% (0.000 out of 16 logical CPUs)
    Total Thread Count

Collection and Platform Info
    Application Command Line: env "TRIX_BACKEND=itt" "LD_LIBRARY_PATH=..." "./test_basic"
    Operating System: 6.8.0-90-generic Ubuntu 22.04.5 LTS
    Collection start time: 17:31:17 12/04/2026 UTC
    Collection stop time:  17:31:17 12/04/2026 UTC
    Collector Type: User-mode sampling and tracing
    CPU: Intel(R) microarchitecture code named Tigerlake H
    Logical CPU Count: 16
vtune: Executing actions 100 % done
```

### Show top-down by function

```bash
vtune -report top-down -result-dir /tmp/trix_vtune -group-by function
```

### CSV output

```bash
vtune -report summary -format=csv -result-dir /tmp/trix_vtune
```

### Open in VTune GUI

```bash
vtune-gui /tmp/trix_vtune &
```

---

## LTTng backend (Linux only)

Records structured **CTF** (Common Trace Format) binary events via LTTng-UST
user-space tracepoints. Events are stored to disk by `lttng-sessiond` with
nanosecond timestamps and can be inspected with `babeltrace2` or
**Trace Compass** (Eclipse-based GUI).

Provider name: `trix`  
Events: `frame_begin`, `frame_end`, `algo_begin`, `algo_end`,
        `data_int`, `data_float`, `data_string`

### Install

```bash
# Build dependency
apt install liblttng-ust-dev

# Runtime tools
apt install lttng-tools babeltrace2
```

### Build with LTTng support

```bash
cmake -B build -DTRIX_BACKEND_LTTNG=ON
cmake --build build
```

### Capture

```bash
# Create and configure a session
lttng create trix-session
lttng enable-event --userspace 'trix:*'
lttng start

# Run your application
TRIX_BACKEND=lttng LD_LIBRARY_PATH=$PWD/build ./build/tests/test_basic

# Stop the session
lttng stop
lttng destroy
```

### View with babeltrace2 (terminal)

```bash
babeltrace2 ~/lttng-traces/trix-session*
```

Example output:
```
[15:01:00.123456789] (+0.000000001) hostname trix:frame_begin: { frame_num = 0 }
[15:01:00.123456820] (+0.000000031) hostname trix:algo_begin: { name = "encode" }
[15:01:00.123456900] (+0.000000080) hostname trix:data_int: { key = "width", value = 1920 }
[15:01:00.123456910] (+0.000000010) hostname trix:data_int: { key = "height", value = 1080 }
[15:01:00.123456950] (+0.000000040) hostname trix:data_float: { key = "fps", value = 29.97 }
[15:01:00.123456970] (+0.000000020) hostname trix:data_string: { key = "codec", value = "h264" }
[15:01:00.123457010] (+0.000000040) hostname trix:algo_end: { name = "encode" }
[15:01:00.123457020] (+0.000000010) hostname trix:frame_end: { frame_num = 0 }
```

### View with Trace Compass (GUI)

1. Install [Trace Compass](https://www.eclipse.org/tracecompass/)
2. Open the trace directory: `~/lttng-traces/trix-session*/`
3. The **LTTng-UST** analysis view shows per-thread event timelines

### Live view during capture

```bash
lttng create trix-live --live
lttng enable-event --userspace 'trix:*'
lttng start
# In another terminal:
babeltrace2 --input-format=lttng-live net://localhost/host/$(hostname)/trix-live
```

---

## ETW backend (Windows only)

### What to install

| Tool | Purpose | Installation |
|------|---------|-------------|
| WPR (`wpr.exe`) | Collect ETL traces | **Built into Windows** — no install needed |
| `tracerpt.exe` | Convert ETL to text/XML | **Built into Windows** — no install needed |
| `logman.exe` | Start/stop trace sessions | **Built into Windows** — no install needed |
| WPA (Windows Performance Analyzer) | Rich GUI viewer for ETL files | Windows ADK — [download from Microsoft](https://learn.microsoft.com/en-us/windows-hardware/get-started/adk-install) |
| xperf | Command-line ETL analysis | Part of Windows ADK |

**You can collect and inspect ETW events with zero installation** using the built-in tools below.

Provider name: `TrixTracing`
Provider GUID: `{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}`

---

### Option A — No installation required (logman + tracerpt)

```cmd
:: Start a trace session for the trix provider
logman start trix-trace ^
  -p {A1B2C3D4-E5F6-7890-ABCD-EF1234567890} ^
  -o C:\tmp\trix.etl -ets

:: Run your app
set TRIX_BACKEND=etw
your_app.exe

:: Stop and save
logman stop trix-trace -ets

:: Convert to XML
tracerpt C:\tmp\trix.etl -o C:\tmp\trix.xml -of XML -y
type C:\tmp\trix.xml

:: Or convert to CSV
tracerpt C:\tmp\trix.etl -o C:\tmp\trix.csv -of CSV -y
type C:\tmp\trix.csv
```

`tracerpt` also supports XML output (`-of XML`) for structured inspection.

---

### Option B — WPA viewer (requires Windows ADK)

Use `logman` to collect (no install needed), then open the result in WPA for a rich timeline view:

```cmd
logman start trix-trace ^
  -p {A1B2C3D4-E5F6-7890-ABCD-EF1234567890} ^
  -o C:\tmp\trix.etl -ets

set TRIX_BACKEND=etw
your_app.exe

logman stop trix-trace -ets

:: Open in WPA (requires ADK)
wpa C:\tmp\trix.etl
```

In WPA: open the **Generic Events** panel and filter by provider `TrixTracing` to see all trix events.

---

### VTune installation

VTune supports ITT on Windows as well — install it from the Intel oneAPI toolkit:

**Option A — Full oneAPI installer** (includes VTune + other tools):
```
https://www.intel.com/content/www/us/en/developer/tools/oneapi/base-toolkit-download.html
```

**Option B — VTune standalone**:
```
https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler-download.html
```

After install, the CLI is at:
```
C:\Program Files (x86)\Intel\oneAPI\vtune\latest\bin64\vtune.exe
```

Add it to PATH by sourcing the environment script:
```cmd
"C:\Program Files (x86)\Intel\oneAPI\setvars.bat"
```

Then use the same `vtune` commands as in the Linux section above.