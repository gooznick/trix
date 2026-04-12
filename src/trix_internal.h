/*
 * trix_internal.h — internal vtable shared by dispatch and all backends.
 * Not part of the public API. Do not install or expose to users.
 */

#ifndef TRIX_INTERNAL_H
#define TRIX_INTERNAL_H

#include <stdint.h>

typedef struct {
    void (*frame_begin)(uint64_t frame_num);
    void (*frame_end)(uint64_t frame_num);
    void (*algo_begin)(const char* name);
    void (*algo_end)(const char* name);
    void (*data_int)(const char* key, uint64_t value);
    void (*data_float)(const char* key, float value);
    void (*data_string)(const char* key, const char* value);
} trix_vtable_t;

/* Each backend's init function fills and returns a pointer to its static vtable. */
typedef const trix_vtable_t* (*trix_backend_init_fn)(void);

/* The live vtable written once by the library constructor, read-only thereafter. */
extern trix_vtable_t g_trix_vtable;

#endif /* TRIX_INTERNAL_H */
