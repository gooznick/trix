/*
 * smoke.cpp — trix API smoke tester
 *
 * Exercises every trix public API call (all 7 tracing functions + version) from
 * multiple C++11 threads with no dependencies other than trix itself.
 *
 * Pattern:
 *   - Main thread owns frames (TRIX_FRAME_SCOPE / trix_frame_begin/end).
 *   - Each frame spawns NUM_WORKERS threads; each thread owns a pre-assigned
 *     chunk of an array and computes a CPU-bound trigonometric reduction.
 *   - Both C API forms and C++ RAII wrappers (TRIX_ALGO_SCOPE, TRIX_FRAME_SCOPE)
 *     are used so every public API path is exercised.
 *   - No mutexes, no condition variables: each thread writes only to its own
 *     slot in the results vector.
 *
 * Run:
 *   TRIX_BACKEND=ftrace ./trix_smoke    # or perf, itt, etw, nop, …
 *
 * Exit code 0 on success, prints "trix smoke: OK" to stdout.
 */

#include <trix/trix.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  Configuration
// ─────────────────────────────────────────────────────────────────────────────

static constexpr int NUM_FRAMES  = 10;
static constexpr int NUM_WORKERS = 4;
static constexpr int CHUNK_SIZE  = 50000;  // elements per worker per frame

// ─────────────────────────────────────────────────────────────────────────────
//  Worker function — called from a dedicated std::thread each frame.
//  Writes its partial result to results[worker_id] (no sharing, no locks).
// ─────────────────────────────────────────────────────────────────────────────

static void worker_fn(int worker_id, int frame, double* out_partial)
{
    // C++ RAII form
    TRIX_ALGO_SCOPE("compute");

    const int base = worker_id * CHUNK_SIZE;
    double sum = 0.0;
    for (int i = 0; i < CHUNK_SIZE; ++i) {
        double x = (base + i + frame) * 0.0001;
        sum += std::sin(x) * std::cos(x * 1.3);
    }
    *out_partial = sum;

    // C API data calls — all three data types
    trix_data_float("partial_sum", static_cast<float>(sum));
    trix_data_int("worker_id",     static_cast<uint64_t>(worker_id));
    trix_data_string("status",     "done");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────────────────────────────────────

int main(void)
{
    // Exercise version / build-info (not part of vtable)
    (void)trix_version();
    (void)trix_build_info();

    std::vector<double>      partial_sums(NUM_WORKERS);
    std::vector<std::thread> workers;
    workers.reserve(NUM_WORKERS);

    for (int f = 0; f < NUM_FRAMES; ++f) {

        // C++ RAII frame scope
        TRIX_FRAME_SCOPE(static_cast<uint64_t>(f));

        // Main-thread data before workers start
        trix_data_int("frame_num", static_cast<uint64_t>(f));
        trix_data_float("progress", static_cast<float>(f + 1) / NUM_FRAMES);

        // Spawn workers — each owns its own slot, no synchronisation needed
        workers.clear();
        for (int w = 0; w < NUM_WORKERS; ++w)
            workers.emplace_back(worker_fn, w, f, &partial_sums[w]);

        // C API algo on main thread while workers run
        trix_algo_begin("main_reduce");

        for (auto& t : workers) t.join();

        // Reduction: sum all partial results
        double total = 0.0;
        for (double s : partial_sums) total += s;

        trix_data_float("total_sum", static_cast<float>(total));
        trix_data_string("phase",    "reduce");

        trix_algo_end("main_reduce");
    }

    std::printf("trix smoke: OK\n");
    return 0;
}
