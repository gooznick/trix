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

# ftrace — kernel ring buffer
TRIX_BACKEND=ftrace ./build/demo/trix_demo
sudo cat /sys/kernel/tracing/trace | grep trix | head -20

# perf SDT probes
TRIX_BACKEND=perf ./build/demo/trix_demo

# Intel VTune
TRIX_BACKEND=itt ./build/demo/trix_demo

# ETW (Windows)
set TRIX_BACKEND=etw
trix_demo.exe
```

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
