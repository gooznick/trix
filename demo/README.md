# trix demo — multithreaded image tracking

A self-contained C++17 demo that instruments a realistic multithreaded pipeline
with trix, letting you profile and trace it with any supported backend.

## What it does

```
main thread                    worker threads (×4)
───────────────────────────────────────────────────────
TRIX_FRAME_SCOPE(N)
  generate   ← apply_warp()
  dispatch   ── push 70 tasks ──► correlate  (NCC patch search)
  wait       ────────────────────────────────────────────────────
  estimate   ← median translation
  sleep until next 50 Hz tick
```

Each frame the demo:
1. **generate** — warps the previous frame by a small random translation
   (±4 px), rotation (±0.5°), and Gaussian noise — mimicking a moving camera.
2. **dispatch** — submits 70 patch-search tasks to a thread pool.
3. **correlate** *(worker threads)* — for each patch, slides a 32×32 template
   over a ±8 px search window in the new frame using
   `cv::matchTemplate(TM_CCOEFF_NORMED)`.
4. **wait** — main thread waits for all workers.
5. **estimate** — median translation over high-confidence patches (NCC ≥ 0.5).

## Dependencies

| Package | Why |
|---------|-----|
| OpenCV ≥ 4 (`core`, `imgproc`) | Image warp + template matching |

```bash
# Debian / Ubuntu
apt install libopencv-dev
```

No other dependencies beyond trix itself.

## Build

### As part of the trix source tree

```bash
cmake -B build -DTRIX_BUILD_DEMO=ON
cmake --build build --target trix_demo
```

### Standalone (after `cmake --install`)

```bash
cmake -S demo -B demo/build -DCMAKE_PREFIX_PATH=/path/to/trix/install
cmake --build demo/build
```

## Run

```bash
export LD_LIBRARY_PATH=build   # or wherever libtrix.so lives

# No backend — all trix calls are no-ops, pure timing baseline
./build/demo/trix_demo

# ftrace + Perfetto (recommended — see "Capture and visualise" below)
TRIX_BACKEND=ftrace ./build/demo/trix_demo

# Intel VTune
TRIX_BACKEND=itt ./build/demo/trix_demo

# ETW (Windows)
set TRIX_BACKEND=etw
trix_demo.exe
```

---

## Capture and visualise with Perfetto

Perfetto (https://ui.perfetto.dev) renders the ftrace atrace format as nested
duration spans on a per-thread / per-CPU timeline — showing threads, cores,
context switches, and trix spans on the same graph.

### On the target (embedded or development machine)

```bash
# All-in-one — requires root
sudo sh ./scripts/capture_ftrace.sh ./build/demo/trix_demo
# produces: trix_ftrace_YYYYMMDD_HHMMSS.txt

# Or: start tracing, Ctrl+C when done, then save separately
sudo sh ./scripts/capture_ftrace_pre.sh ./build/demo/trix_demo
sudo sh ./scripts/capture_ftrace_post.sh
```

The scripts:
1. Mount tracefs if not already mounted
2. Enable `sched_switch`, `sched_wakeup`, `sched_migrate_task`
3. Run the command with `TRIX_BACKEND=ftrace`
4. Save `cat /sys/kernel/tracing/trace` → `trix_ftrace_*.txt`

### On the host (view)

```bash
scp target:~/trix_trace.txt .
# open https://ui.perfetto.dev → drag and drop trix_trace.txt
```

Perfetto works offline once loaded (PWA with service-worker cache).

### What you see in Perfetto

| Track | Content |
|-------|---------|
| CPU 0..N | Which thread ran on each core (context switches) |
| `trix_demo` | `frame_0`..`frame_N` spans with nested `generate`, `dispatch`, `wait`, `estimate` |
| `trix_worker_0..3` | Parallel `correlate` spans (70 patches per frame across 4 threads) |
| Counters | `est_tx`, `est_ty` per-frame estimated translation |

---

## Expected output

```
trix demo  —  image tracking pipeline
  640x480 image, 70 patches, 4 workers, 50 Hz, 200 frames

  frame    0  est_t = (+2.00, +4.00) px
  frame   50  est_t = (+2.00, +2.00) px
  frame  100  est_t = (+1.00, -2.00) px
  frame  150  est_t = (+1.00, +4.00) px

Processed 200 frames in 4.02 s  (49.8 Hz avg)
Mean estimated translation: (0.050, 0.065) px
```

The mean translation should converge near zero — accumulated drift is not
modelled, so each frame's estimate is independent.

## What to look for in a trace

| trix event | What it shows |
|------------|---------------|
| `trix_frame_begin/end` | Per-frame budget (20 ms at 50 Hz) |
| `trix_algo_begin("generate")` | Cost of warpAffine + noise |
| `trix_algo_begin("dispatch")` | Task submission overhead |
| `trix_algo_begin("correlate")` | **Parallel** — 70 overlapping spans across 4 threads |
| `trix_algo_begin("wait")` | Main-thread idle waiting for workers |
| `trix_algo_begin("estimate")` | Median computation cost |
| `trix_data_float("est_tx/ty")` | Per-frame estimated displacement |
