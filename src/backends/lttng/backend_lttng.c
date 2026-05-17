/*
 * backend_lttng.c — LTTng-UST backend.
 *
 * Emits structured CTF (Common Trace Format) events to an LTTng session via
 * user-space tracepoints (lttng-ust).  Events land in a per-process shared
 * memory ring buffer and are consumed by the LTTng session daemon
 * (lttng-sessiond) with nanosecond timestamps.
 *
 * Tracepoints defined (provider "trix"):
 *   frame_begin(frame_num)        frame_end(frame_num)
 *   algo_begin(name)              algo_end(name)
 *   data_int(key, value)          data_float(key, value)
 *   data_string(key, value)
 *
 * Usage:
 *   lttng create my-session
 *   lttng enable-event -u 'trix:*'
 *   lttng start
 *   TRIX_BACKEND=lttng ./myapp
 *   lttng stop && lttng view
 *
 * Build dependency: liblttng-ust-dev
 * Runtime dependency: lttng-tools (lttng-sessiond, lttng CLI)
 * Supports lttng-ust < 2.14 (old API) and >= 2.14 (new API).
 */

/* Instantiate the tracepoint provider in this translation unit. */
#ifdef TRIX_LTTNG_UST_NEW_API
#  define LTTNG_UST_TRACEPOINT_CREATE_PROBES
#  define LTTNG_UST_TRACEPOINT_DEFINE
#else
#  define TRACEPOINT_CREATE_PROBES
#  define TRACEPOINT_DEFINE
#endif

#include "trix_lttng_tp.h"

#include "../../trix_internal.h"

/* Version-neutral tracepoint call helper. */
#ifdef TRIX_LTTNG_UST_NEW_API
#  define TRIX_TP(event, ...) lttng_ust_tracepoint(trix, event, ##__VA_ARGS__)
#else
#  define TRIX_TP(event, ...) tracepoint(trix, event, ##__VA_ARGS__)
#endif

static void lttng_frame_begin(uint64_t frame_num)  { TRIX_TP(frame_begin, frame_num); }
static void lttng_frame_end(uint64_t frame_num)    { TRIX_TP(frame_end,   frame_num); }
static void lttng_algo_begin(const char *name)     { TRIX_TP(algo_begin,  name); }
static void lttng_algo_end(const char *name)       { TRIX_TP(algo_end,    name); }

static void lttng_data_int(const char *key, uint64_t value) {
    TRIX_TP(data_int, key, value);
}
static void lttng_data_float(const char *key, float value) {
    TRIX_TP(data_float, key, value);
}
static void lttng_data_string(const char *key, const char *value) {
    TRIX_TP(data_string, key, value);
}

static const trix_vtable_t s_lttng_vtable = {
    lttng_frame_begin,
    lttng_frame_end,
    lttng_algo_begin,
    lttng_algo_end,
    lttng_data_int,
    lttng_data_float,
    lttng_data_string,
};

const trix_vtable_t* trix_backend_lttng_init(void) {
    return &s_lttng_vtable;
}
