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

#include <ittnotify.h>

#include "../../trix_internal.h"

static __itt_domain*        g_domain        = NULL;
static __itt_domain*        g_frame_domain  = NULL;

/* Lazily creates an ITT string handle — ITT caches them internally. */
static __itt_string_handle* get_handle(const char* name) {
    return __itt_string_handle_create(name);
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
    __itt_metadata_add(g_domain, __itt_null, get_handle(key),
                       __itt_metadata_u64, 1, &value);
}

static void itt_data_float(const char* key, float value) {
    __itt_metadata_add(g_domain, __itt_null, get_handle(key),
                       __itt_metadata_float, 1, &value);
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
        fprintf(stderr, "trix: itt: failed to create domain\n");
        abort();
    }
    return &s_itt_vtable;
}
