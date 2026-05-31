/*
 * backend_itt.c — Intel VTune ITT backend.
 *
 * Uses the Intel Instrumentation and Tracing Technology API (ittapi),
 * compiled directly from source (third_party/ittapi).
 *
 * If VTune is not attached at runtime, ittapi is a no-op internally —
 * this backend is safe to use even when VTune is not running.
 *
 * Usage:
 *   Start VTune collection, then: TRIX_BACKEND=itt ./myapp
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <ittnotify.h>

#include "../../trix_internal.h"

static __itt_domain*        g_domain        = NULL;
static __itt_domain*        g_frame_domain  = NULL;

/* Lazily creates an ITT string handle — ITT caches them internally. */
static __itt_string_handle* get_handle(const char* name) {
    return __itt_string_handle_create(name);
}

/* --- Counter cache --------------------------------------------------------
 * __itt_counter_create_typed() creates a NEW counter on every call — it is
 * not deduplicated like string handles.  We maintain a small lookup table so
 * each unique key maps to exactly one counter track in the VTune timeline.
 * The table is protected by a mutex; lookups are O(n) but the number of
 * distinct counter names is expected to be small (< 64).
 */

#define TRIX_MAX_COUNTERS 64

typedef struct {
    const char*   name;    /* interned via strdup — never freed */
    __itt_counter handle;
} trix_counter_entry_t;

static trix_counter_entry_t  s_counters[TRIX_MAX_COUNTERS];
static int                   s_counter_count = 0;
static pthread_mutex_t       s_counter_mutex = PTHREAD_MUTEX_INITIALIZER;

static __itt_counter get_or_create_counter(const char* key,
                                           __itt_metadata_type type)
{
    pthread_mutex_lock(&s_counter_mutex);

    for (int i = 0; i < s_counter_count; i++) {
        if (strcmp(s_counters[i].name, key) == 0) {
            __itt_counter h = s_counters[i].handle;
            pthread_mutex_unlock(&s_counter_mutex);
            return h;
        }
    }

    __itt_counter h = NULL;
    if (s_counter_count < TRIX_MAX_COUNTERS) {
        h = __itt_counter_create_typed(key, "trix", type);
        s_counters[s_counter_count].name   = strdup(key);
        s_counters[s_counter_count].handle = h;
        s_counter_count++;
    }

    pthread_mutex_unlock(&s_counter_mutex);
    return h;
}

static void itt_frame_begin(uint64_t frame_num) {
    (void)frame_num;
    __itt_frame_begin_v3(g_frame_domain, NULL);
}

static void itt_frame_end(uint64_t frame_num) {
    (void)frame_num;
    __itt_frame_end_v3(g_frame_domain, NULL);
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

const trix_vtable_t* trix_backend_itt_init(void) {
    g_domain       = __itt_domain_create("trix");
    g_frame_domain = __itt_domain_create("trix.frames");
    if (!g_domain || !g_frame_domain) {
        fprintf(stderr, "trix: itt: failed to create domain (Is VTune collector not loaded ?) \n");
        abort();
    }
    return &s_itt_vtable;
}
