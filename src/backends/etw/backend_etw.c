/*
 * backend_etw.c — Windows ETW (Event Tracing for Windows) backend.
 *
 * Uses the TraceLogging API — self-describing events, no XML manifest needed.
 * Pure Win32 API, no external dependencies beyond the Windows SDK.
 *
 * To collect events:
 *   wpr -start MyProfile.wprp    (or xperf / logman / PerfView)
 *   set TRIX_BACKEND=etw && myapp.exe
 *   wpr -stop trace.etl
 *   wpa trace.etl
 *
 * Provider GUID: {A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
 */

#ifdef _WIN32

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include <TraceLoggingProvider.h>

#include "../../trix_internal.h"

TRACELOGGING_DEFINE_PROVIDER(
    g_trix_provider,
    "TrixTracing",
    /* {A1B2C3D4-E5F6-7890-ABCD-EF1234567890} */
    (0xa1b2c3d4, 0xe5f6, 0x7890, 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x90));

static void etw_frame_begin(uint64_t frame_num) {
    TraceLoggingWrite(g_trix_provider, "FrameBegin",
        TraceLoggingUInt64(frame_num, "FrameNum"));
}

static void etw_frame_end(uint64_t frame_num) {
    TraceLoggingWrite(g_trix_provider, "FrameEnd",
        TraceLoggingUInt64(frame_num, "FrameNum"));
}

static void etw_algo_begin(const char* name) {
    TraceLoggingWrite(g_trix_provider, "AlgoBegin",
        TraceLoggingString(name, "Name"));
}

static void etw_algo_end(const char* name) {
    TraceLoggingWrite(g_trix_provider, "AlgoEnd",
        TraceLoggingString(name, "Name"));
}

static void etw_data_int(const char* key, uint64_t value) {
    TraceLoggingWrite(g_trix_provider, "DataInt",
        TraceLoggingString(key, "Key"),
        TraceLoggingUInt64(value, "Value"));
}

static void etw_data_float(const char* key, float value) {
    TraceLoggingWrite(g_trix_provider, "DataFloat",
        TraceLoggingString(key, "Key"),
        TraceLoggingFloat32(value, "Value"));
}

static void etw_data_string(const char* key, const char* value) {
    TraceLoggingWrite(g_trix_provider, "DataString",
        TraceLoggingString(key, "Key"),
        TraceLoggingString(value, "Value"));
}

static const trix_vtable_t s_etw_vtable = {
    etw_frame_begin,
    etw_frame_end,
    etw_algo_begin,
    etw_algo_end,
    etw_data_int,
    etw_data_float,
    etw_data_string,
};

extern "C" const trix_vtable_t* trix_backend_etw_init(void) {
    HRESULT hr = TraceLoggingRegister(g_trix_provider);
    if (FAILED(hr)) {
        fprintf(stderr, "trix: etw: TraceLoggingRegister failed: 0x%lx\n", hr);
        abort();
    }
    return &s_etw_vtable;
}

/* Unregister on DLL unload — called from DllMain in trix_dispatch.c */
extern "C" void trix_backend_etw_shutdown(void) {
    TraceLoggingUnregister(g_trix_provider);
}

#endif /* _WIN32 */
