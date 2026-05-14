/*
 * backend_ftrace.c — ftrace backend.
 *
 * Writes events to the kernel's trace_marker file using the atrace/systrace
 * wire format, which Perfetto and other modern tools interpret as properly
 * nested duration spans:
 *
 *   B|<tgid>|<name>        — begin span  (algo_begin, frame_begin)
 *   E|<tgid>               — end span    (algo_end,   frame_end)
 *   C|<tgid>|<key>|<value> — counter     (data_int, data_float)
 *
 * The kernel automatically stamps each write with the writing thread's TID
 * and a high-resolution timestamp.  Perfetto matches B/E pairs per TID to
 * produce per-thread span tracks.
 *
 * Capture on target (no tools required beyond a mounted tracefs):
 *   see demo/capture.sh
 *
 * View on host:
 *   https://ui.perfetto.dev  →  Open trace file
 */

#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../trix_internal.h"

static int  g_trace_fd  = -1;
static char g_tgid[12]  = "";   /* cached process-id string, set at init */
static int  g_tgid_len  = 0;

static void write_marker(const char* buf, int len)
{
    if (g_trace_fd < 0) return;
    (void)write(g_trace_fd, buf, (size_t)len);
}

static void ftrace_frame_begin(uint64_t frame_num)
{
    char buf[128];
    int n = snprintf(buf, sizeof(buf),
                     "B|%s|frame_%" PRIu64 "\n", g_tgid, frame_num);
    write_marker(buf, n);
}

static void ftrace_frame_end(uint64_t frame_num)
{
    (void)frame_num;
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "E|%s\n", g_tgid);
    write_marker(buf, n);
}

static void ftrace_algo_begin(const char* name)
{
    char buf[256];
    int n = snprintf(buf, sizeof(buf), "B|%s|%s\n", g_tgid, name);
    write_marker(buf, n);
}

static void ftrace_algo_end(const char* name)
{
    (void)name;
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "E|%s\n", g_tgid);
    write_marker(buf, n);
}

static void ftrace_data_int(const char* key, uint64_t value)
{
    char buf[256];
    int n = snprintf(buf, sizeof(buf),
                     "C|%s|%s|%" PRIu64 "\n", g_tgid, key, value);
    write_marker(buf, n);
}

static void ftrace_data_float(const char* key, float value)
{
    char buf[256];
    int n = snprintf(buf, sizeof(buf),
                     "C|%s|%s|%g\n", g_tgid, key, (double)value);
    write_marker(buf, n);
}

static void ftrace_data_string(const char* key, const char* value)
{
    /* Emit as a zero-duration span — no native string event in atrace format. */
    char buf[512];
    int n = snprintf(buf, sizeof(buf), "B|%s|%s=%s\n", g_tgid, key, value);
    write_marker(buf, n);
    char end[32];
    int m = snprintf(end, sizeof(end), "E|%s\n", g_tgid);
    write_marker(end, m);
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

const trix_vtable_t* trix_backend_ftrace_init(void)
{
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

    g_tgid_len = snprintf(g_tgid, sizeof(g_tgid), "%d", (int)getpid());
    (void)g_tgid_len;

    return &s_ftrace_vtable;
}

