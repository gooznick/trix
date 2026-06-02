/*
 * backend_itt.c — Intel VTune ITT backend.
 *
 * Uses the Intel Instrumentation and Tracing Technology API (ittapi),
 * compiled directly from source (third_party/ittapi).
 *
 * Compiled as C++ (set_source_files_properties LANGUAGE CXX) so that
 * std::atomic is available on all MSVC versions supporting C++11.
 *
 * If VTune is not attached at runtime, ittapi is a no-op internally —
 * this backend is safe to use even when VTune is not running.
 *
 * Usage:
 *   Start VTune collection, then: TRIX_BACKEND=itt ./myapp
 */

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <ittnotify.h>

#include "../../trix_internal.h"

static __itt_domain*        g_domain        = nullptr;
static __itt_domain*        g_frame_domain  = nullptr;

/* Lazily creates an ITT string handle — ITT caches them internally.
 * TODO: __itt_string_handle_create() walks a global linked list under an
 * internal mutex on every call.  Add a lock-free cache here (same pattern
 * as get_or_create_counter) to avoid that mutex on the hot path. */
static __itt_string_handle* get_handle(const char* name) {
    return __itt_string_handle_create(name);
}

/* --- Counter cache --------------------------------------------------------
 * __itt_counter_create_typed() creates a NEW counter on every call — it is
 * not deduplicated like string handles.  We maintain a small lookup table so
 * each unique key maps to exactly one counter track in the VTune timeline.
 *
 * Hot path (lookup) is lock-free: readers atomically load the committed count
 * with acquire semantics, then scan entries 0..n-1.  An entry is visible only
 * after both name and handle have been written (release store on the count
 * ensures this).
 *
 * Cold path (creation) uses an atomic_flag spinlock.  It is taken only when a
 * new key is seen for the first time; in steady state every call hits the
 * lock-free scan.  The spinlock is available on all C++11 compilers — no
 * platform headers needed.
 */

#define TRIX_MAX_COUNTERS 64

struct trix_counter_entry_t {
    const char*   name;    /* interned via strdup — never freed */
    __itt_counter handle;
};

static trix_counter_entry_t      s_counters[TRIX_MAX_COUNTERS];
static std::atomic<int>          s_counter_count{0};
static std::atomic_flag          s_counter_lock = ATOMIC_FLAG_INIT;

static __itt_counter get_or_create_counter(const char* key,
                                           __itt_metadata_type type)
{
    /* --- Lock-free hot path --- */
    int n = s_counter_count.load(std::memory_order_acquire);
    for (int i = 0; i < n; i++) {
        if (strcmp(s_counters[i].name, key) == 0)
            return s_counters[i].handle;
    }

    /* --- Cold path: take spinlock, re-scan, then create --- */
    while (s_counter_lock.test_and_set(std::memory_order_acquire))
        ;

    /* Re-read count under lock — another thread may have added it. */
    n = s_counter_count.load(std::memory_order_relaxed);
    for (int i = 0; i < n; i++) {
        if (strcmp(s_counters[i].name, key) == 0) {
            s_counter_lock.clear(std::memory_order_release);
            return s_counters[i].handle;
        }
    }

    __itt_counter h = nullptr;
    if (n < TRIX_MAX_COUNTERS) {
        h = __itt_counter_create_typed(key, "trix", type);
        s_counters[n].name   = strdup(key);
        s_counters[n].handle = h;
        /* Release store: makes the completed entry visible to lock-free readers. */
        s_counter_count.store(n + 1, std::memory_order_release);
    }

    s_counter_lock.clear(std::memory_order_release);
    return h;
}

static void itt_frame_begin(uint64_t frame_num) {
    (void)frame_num;
    __itt_frame_begin_v3(g_frame_domain, nullptr);
}

static void itt_frame_end(uint64_t frame_num) {
    (void)frame_num;
    __itt_frame_end_v3(g_frame_domain, nullptr);
}

static void itt_algo_begin(const char* name) {
    __itt_task_begin(g_domain, __itt_null, __itt_null, get_handle(name));
}

static void itt_algo_end(const char* name) {
    (void)name;
    __itt_task_end(g_domain);
}

static void itt_data_int(const char* key, uint64_t value) {
    __itt_counter h = get_or_create_counter(key, __itt_metadata_u64);
    if (h) __itt_counter_set_value(h, &value);
}

static void itt_data_float(const char* key, float value) {
    /* ITT counters use double for floating-point tracks */
    double dval = (double)value;
    __itt_counter h = get_or_create_counter(key, __itt_metadata_double);
    if (h) __itt_counter_set_value(h, &dval);
}

static void itt_data_string(const char* key, const char* value) {
    __itt_metadata_str_add(g_domain, __itt_null, get_handle(key), value, strlen(value));
}

static const trix_vtable_t s_itt_vtable = {
    itt_frame_begin,
    itt_frame_end,
    itt_algo_begin,
    itt_algo_end,
    itt_data_int,
    itt_data_float,
    itt_data_string,
};

extern "C" const trix_vtable_t* trix_backend_itt_init(void) {
    g_domain       = __itt_domain_create("trix");
    g_frame_domain = __itt_domain_create("trix.frames");
    if (!g_domain || !g_frame_domain) {
        fprintf(stderr, "trix: itt: failed to create domain (Is VTune collector not loaded ?)\n");
        abort();
    }
    return &s_itt_vtable;
}
