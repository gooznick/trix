/*
 * backend_nop.c — nop backend.
 * Used when TRIX_BACKEND is unset. All calls do nothing.
 */

#include "../../trix_internal.h"

static void nop_frame_begin(uint64_t frame_num)                 { (void)frame_num; }
static void nop_frame_end(uint64_t frame_num)                   { (void)frame_num; }
static void nop_algo_begin(const char* name)                    { (void)name; }
static void nop_algo_end(const char* name)                      { (void)name; }
static void nop_data_int(const char* key, uint64_t value)       { (void)key; (void)value; }
static void nop_data_float(const char* key, float value)        { (void)key; (void)value; }
static void nop_data_string(const char* key, const char* value) { (void)key; (void)value; }

static const trix_vtable_t s_nop_vtable = {
    nop_frame_begin,
    nop_frame_end,
    nop_algo_begin,
    nop_algo_end,
    nop_data_int,
    nop_data_float,
    nop_data_string,
};

const trix_vtable_t* trix_backend_nop_init(void) {
    return &s_nop_vtable;
}
