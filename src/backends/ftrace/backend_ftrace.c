/*
 * backend_ftrace.c — ftrace backend.
 *
 * Writes events to the kernel's trace_marker file. Readable via:
 *   cat /sys/kernel/tracing/trace
 *   trace-cmd record / kernelshark
 *   perf record -e ftrace:print
 *
 * Requires read/write access to tracefs. On most systems either root or:
 *   echo 0 > /sys/kernel/tracing/tracing_on  (to not flood the buffer)
 *   chmod a+w /sys/kernel/tracing/trace_marker
 */

#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../trix_internal.h"

static int g_trace_fd = -1;

static void write_marker(const char* buf, int len) {
    if (g_trace_fd < 0) return;
    (void)write(g_trace_fd, buf, (size_t)len);
}

static void ftrace_frame_begin(uint64_t frame_num) {
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "trix_frame_begin %" PRIu64 "\n", frame_num);
    write_marker(buf, n);
}

static void ftrace_frame_end(uint64_t frame_num) {
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "trix_frame_end %" PRIu64 "\n", frame_num);
    write_marker(buf, n);
}

static void ftrace_algo_begin(const char* name) {
    char buf[256];
    int n = snprintf(buf, sizeof(buf), "trix_algo_begin %s\n", name);
    write_marker(buf, n);
}

static void ftrace_algo_end(const char* name) {
    char buf[256];
    int n = snprintf(buf, sizeof(buf), "trix_algo_end %s\n", name);
    write_marker(buf, n);
}

static void ftrace_data_int(const char* key, uint64_t value) {
    char buf[256];
    int n = snprintf(buf, sizeof(buf), "trix_data_int %s=%" PRIu64 "\n", key, value);
    write_marker(buf, n);
}

static void ftrace_data_float(const char* key, float value) {
    char buf[256];
    int n = snprintf(buf, sizeof(buf), "trix_data_float %s=%f\n", key, (double)value);
    write_marker(buf, n);
}

static void ftrace_data_string(const char* key, const char* value) {
    char buf[512];
    int n = snprintf(buf, sizeof(buf), "trix_data_string %s=%s\n", key, value);
    write_marker(buf, n);
}

static const trix_vtable_t s_ftrace_vtable = {
    ftrace_frame_begin,
    ftrace_frame_end,
    ftrace_algo_begin,
    ftrace_algo_end,
    ftrace_data_int,
    ftrace_data_float,
    ftrace_data_string,
};

const trix_vtable_t* trix_backend_ftrace_init(void) {
    /* Try modern tracefs path first, fall back to legacy debugfs path */
    static const char* paths[] = {
        "/sys/kernel/tracing/trace_marker",
        "/sys/kernel/debug/tracing/trace_marker",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        g_trace_fd = open(paths[i], O_WRONLY);
        if (g_trace_fd >= 0) break;
    }
    if (g_trace_fd < 0) {
        perror("trix: ftrace: cannot open trace_marker");
        abort();
    }
    return &s_ftrace_vtable;
}
