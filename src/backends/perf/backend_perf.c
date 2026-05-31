/*
 * backend_perf.c — perf backend using SystemTap SDT (Statically Defined Tracing) probes.
 *
 * SDT probes compile to NOP instructions that perf/SystemTap/DTrace can
 * dynamically patch at runtime with zero overhead when not active.
 *
 * String arguments are packed as little-endian uint64 (first 8 chars).
 * perf_to_perfetto.py decodes them back to readable names.
 *
 * Usage:
 *   perf record -e 'sdt_trix:algo_begin' -e 'sdt_trix:frame_begin' ./myapp
 *   perf script
 *
 * Requires: systemtap-sdt-dev (Ubuntu/Debian) or systemtap-devel (RHEL/Fedora)
 */

#include <stdint.h>
#include <string.h>

#include "trix_sdt.h"
#include "../../trix_internal.h"

/* Pack the first 8 chars of a string into a uint64 (little-endian). */
static uint64_t trix_str_pack(const char *s) {
    uint64_t v = 0;
    int i;
    for (i = 0; i < 8 && s[i]; i++)
        v |= (uint64_t)(unsigned char)s[i] << (i * 8);
    return v;
}

static void perf_frame_begin(uint64_t frame_num) {
    TRIX_PROBE1(trix, frame_begin, frame_num);
}

static void perf_frame_end(uint64_t frame_num) {
    TRIX_PROBE1(trix, frame_end, frame_num);
}

static void perf_algo_begin(const char* name) {
    TRIX_PROBE1(trix, algo_begin, trix_str_pack(name));
}

static void perf_algo_end(const char* name) {
    TRIX_PROBE1(trix, algo_end, trix_str_pack(name));
}

static void perf_data_int(const char* key, uint64_t value) {
    TRIX_PROBE2(trix, data_int, trix_str_pack(key), value);
}

static void perf_data_float(const char* key, float value) {
    uint64_t bits = 0;
    memcpy(&bits, &value, sizeof(value));
    TRIX_PROBE2(trix, data_float, trix_str_pack(key), bits);
}

static void perf_data_string(const char* key, const char* value) {
    TRIX_PROBE2(trix, data_string, trix_str_pack(key), trix_str_pack(value));
}

static const trix_vtable_t s_perf_vtable = {
    perf_frame_begin,
    perf_frame_end,
    perf_algo_begin,
    perf_algo_end,
    perf_data_int,
    perf_data_float,
    perf_data_string,
};

const trix_vtable_t* trix_backend_perf_init(void) {
    return &s_perf_vtable;
}
