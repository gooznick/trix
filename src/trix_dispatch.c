/*
 * trix_dispatch.c — library constructor, vtable dispatch, public C symbols.
 *
 * At library load time (before main), the constructor reads TRIX_BACKEND and
 * installs the chosen backend's vtable into g_trix_vtable. All public trix_*
 * functions then dispatch through that vtable with no per-call branching.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trix_internal.h"
#include "trix/trix_version.h"

/* Forward declarations for compiled-in backends */
const trix_vtable_t* trix_backend_nop_init(void);

#ifdef TRIX_BACKEND_FTRACE
const trix_vtable_t* trix_backend_ftrace_init(void);
#endif

#ifdef TRIX_BACKEND_PERF
const trix_vtable_t* trix_backend_perf_init(void);
#endif

#ifdef TRIX_BACKEND_ITT
const trix_vtable_t* trix_backend_itt_init(void);
#endif

#ifdef TRIX_BACKEND_ETW
const trix_vtable_t* trix_backend_etw_init(void);
void                 trix_backend_etw_shutdown(void);
#endif

#ifdef TRIX_BACKEND_LTTNG
const trix_vtable_t* trix_backend_lttng_init(void);
#endif

#ifdef TRIX_BACKEND_ATRACE
const trix_vtable_t* trix_backend_atrace_init(void);
#endif

/* --- Global vtable — written once in constructor, read-only after --- */
trix_vtable_t g_trix_vtable;

/* --- Available backends — built at compile time --- */
static const char* const s_trix_available =
#ifdef TRIX_BACKEND_FTRACE
    "ftrace "
#endif
#ifdef TRIX_BACKEND_PERF
    "perf "
#endif
#ifdef TRIX_BACKEND_ITT
    "itt "
#endif
#ifdef TRIX_BACKEND_ETW
    "etw "
#endif
#ifdef TRIX_BACKEND_LTTNG
    "lttng "
#endif
#ifdef TRIX_BACKEND_ATRACE
    "atrace "
#endif
    "";

/* --- Startup diagnostic print --- */
static void trix_print_info(const char* active)
{
    if (getenv("TRIX_QUIET")) return;
    fprintf(stderr, "trix %s  backend=%-8s  available=[%s]\n",
            TRIX_VERSION_STRING, active ? active : "none", s_trix_available);
}

/* --- Backend selection — shared between constructor implementations --- */
static void trix_select_backend(void) {
    const char* backend = getenv("TRIX_BACKEND");
    const trix_vtable_t* vt = NULL;

    if (!backend || backend[0] == '\0') {
        vt = trix_backend_nop_init();
    }
#ifdef TRIX_BACKEND_FTRACE
    else if (strcmp(backend, "ftrace") == 0) {
        vt = trix_backend_ftrace_init();
    }
#endif
#ifdef TRIX_BACKEND_PERF
    else if (strcmp(backend, "perf") == 0) {
        vt = trix_backend_perf_init();
    }
#endif
#ifdef TRIX_BACKEND_ITT
    else if (strcmp(backend, "itt") == 0) {
        vt = trix_backend_itt_init();
    }
#endif
#ifdef TRIX_BACKEND_ETW
    else if (strcmp(backend, "etw") == 0) {
        vt = trix_backend_etw_init();
    }
#endif
#ifdef TRIX_BACKEND_LTTNG
    else if (strcmp(backend, "lttng") == 0) {
        vt = trix_backend_lttng_init();
    }
#endif
#ifdef TRIX_BACKEND_ATRACE
    else if (strcmp(backend, "atrace") == 0) {
        vt = trix_backend_atrace_init();
    }
#endif
    else {
        fprintf(stderr, "trix: unknown backend '%s'  available=[%s]\n",
                backend, s_trix_available);
        abort();
    }

    g_trix_vtable = *vt;
    trix_print_info(backend);
}

/* --- Platform-specific library init/fini --- */
#ifdef _WIN32

#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL; (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        trix_select_backend();
    } else if (fdwReason == DLL_PROCESS_DETACH) {
#ifdef TRIX_BACKEND_ETW
        trix_backend_etw_shutdown();
#endif
    }
    return TRUE;
}

#else /* Linux / POSIX */

__attribute__((constructor))
static void trix_init(void) {
    trix_select_backend();
}

#endif /* _WIN32 */

/* --- Public C API — thin dispatch through vtable --- */

void trix_frame_begin(uint64_t frame_num)                { g_trix_vtable.frame_begin(frame_num); }
void trix_frame_end(uint64_t frame_num)                  { g_trix_vtable.frame_end(frame_num); }
void trix_algo_begin(const char* name)                   { g_trix_vtable.algo_begin(name); }
void trix_algo_end(const char* name)                     { g_trix_vtable.algo_end(name); }
void trix_data_int(const char* key, uint64_t value)      { g_trix_vtable.data_int(key, value); }
void trix_data_float(const char* key, float value)       { g_trix_vtable.data_float(key, value); }
void trix_data_string(const char* key, const char* value){ g_trix_vtable.data_string(key, value); }
const char* trix_version(void)                           { return TRIX_VERSION_STRING; }
