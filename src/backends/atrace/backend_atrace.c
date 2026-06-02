/*
 * backend_atrace.c — file backend using the atrace/systrace wire format.
 *
 * Writes the same tracing_mark_write lines that the ftrace backend sends to
 * the kernel's trace_marker, but directly to a regular file.  The output is
 * a valid ftrace text trace that Perfetto can open without any conversion:
 *
 *   Open https://ui.perfetto.dev → drag and drop the .atrace file
 *
 * Output file:
 *   TRIX_FILE_OUT env var  (if set)
 *   trix_YYYYMMDD_HHMMSS.atrace  (default, created in current directory)
 *
 * Thread safety: write_event formats a complete line into a stack buffer and
 * calls atrace_write() which uses a raw O_APPEND file descriptor.  On Linux,
 * POSIX guarantees that write() on an O_APPEND fd is atomic — no mutex needed.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atrace_platform.h"
#include "../../trix_internal.h"

static int   g_fd              = -1;
static int   g_pid             = 0;
static char  g_comm[17]        = "trix";  /* process short name, max 15 chars + NUL */

/* ── Core writer ──────────────────────────────────────────────────────────── */

static void write_event(const char* payload)
{
    double ts  = atrace_now_sec();
    int    cpu = atrace_getcpu();
    int    tid = atrace_gettid();

    char line[640];
    int len = snprintf(line, sizeof(line),
                       "       %s-%d  [%03d] .....%13.6f: tracing_mark_write: %s\n",
                       g_comm, tid, cpu < 0 ? 0 : cpu, ts, payload);
    if (len > 0 && len < (int)sizeof(line))
        atrace_write(g_fd, line, len);
}

/* ── Backend functions ────────────────────────────────────────────────────── */

static void atrace_frame_begin(uint64_t frame_num)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "B|%d|frame_%" PRIu64, g_pid, frame_num);
    write_event(buf);
}

static void atrace_frame_end(uint64_t frame_num)
{
    (void)frame_num;
    char buf[32];
    snprintf(buf, sizeof(buf), "E|%d", g_pid);
    write_event(buf);
}

static void atrace_algo_begin(const char* name)
{
    char buf[272];
    snprintf(buf, sizeof(buf), "B|%d|%s", g_pid, name);
    write_event(buf);
}

static void atrace_algo_end(const char* name)
{
    (void)name;
    char buf[32];
    snprintf(buf, sizeof(buf), "E|%d", g_pid);
    write_event(buf);
}

static void atrace_data_int(const char* key, uint64_t value)
{
    char buf[272];
    snprintf(buf, sizeof(buf), "C|%d|%s|%" PRIu64, g_pid, key, value);
    write_event(buf);
}

static void atrace_data_float(const char* key, float value)
{
    char buf[272];
    snprintf(buf, sizeof(buf), "C|%d|%s|%g", g_pid, key, (double)value);
    write_event(buf);
}

static void atrace_data_string(const char* key, const char* value)
{
    /* Emit as a zero-duration span — no native string event in atrace format. */
    char buf[528];
    snprintf(buf, sizeof(buf), "B|%d|%s=%s", g_pid, key, value);
    write_event(buf);
    char end[32];
    snprintf(end, sizeof(end), "E|%d", g_pid);
    write_event(end);
}

static const trix_vtable_t s_atrace_vtable = {
    atrace_frame_begin,
    atrace_frame_end,
    atrace_algo_begin,
    atrace_algo_end,
    atrace_data_int,
    atrace_data_float,
    atrace_data_string,
};

/* ── Destructor — close file ──────────────────────────────────────────────── */

static void atrace_fini(void)
{
    if (g_fd >= 0) {
        atrace_close(g_fd);
        g_fd = -1;
    }
}

/* ── Init ─────────────────────────────────────────────────────────────────── */

const trix_vtable_t* trix_backend_atrace_init(void)
{
    g_pid = atrace_getpid();
    atrace_getcomm(g_comm, sizeof(g_comm));

    /* Determine output path */
    const char* out_path = getenv("TRIX_FILE_OUT");
    char default_path[64];
    if (!out_path || out_path[0] == '\0') {
        time_t now = time(NULL);
        struct tm tm_now;
        atrace_localtime(now, &tm_now);
        snprintf(default_path, sizeof(default_path),
                 "trix_%04d%02d%02d_%02d%02d%02d.atrace",
                 tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                 tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
        out_path = default_path;
    }

    g_fd = atrace_open(out_path);
    if (g_fd < 0) {
        perror("trix: atrace: cannot open output file");
        abort();
    }

    /* Write ftrace-compatible header so Perfetto recognises the format */
    char header[256];
    int  hlen = snprintf(header, sizeof(header),
            "# tracer: nop\n"
            "#\n"
            "# entries-in-buffer/entries-written: 0/0   #P:%d\n"
            "#\n"
            "#           TASK-PID     CPU#  |||||  TIMESTAMP  FUNCTION\n"
            "#              | |         |   |||||     |         |\n",
            atrace_getcpucount());
    atrace_write(g_fd, header, hlen);

    fprintf(stderr, "trix: atrace: writing to %s\n", out_path);

    atexit(atrace_fini);

    return &s_atrace_vtable;
}

