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
 * Does not require root.  Works on any Linux with glibc >= 2.17.
 */

#define _GNU_SOURCE
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include "../../trix_internal.h"

static FILE*           g_file   = NULL;
static int             g_pid    = 0;
static char            g_comm[17] = "trix";   /* /proc/self/comm, max 15 chars + NUL */
static pthread_mutex_t g_lock   = PTHREAD_MUTEX_INITIALIZER;

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int gettid_impl(void)
{
    return (int)syscall(SYS_gettid);
}

/* ── Core writer ──────────────────────────────────────────────────────────── */

static void write_event(const char* payload)
{
    double ts  = now_sec();
    int    cpu = sched_getcpu();
    int    tid = gettid_impl();

    pthread_mutex_lock(&g_lock);
    fprintf(g_file, "       %s-%d  [%03d] .....%13.6f: tracing_mark_write: %s\n",
            g_comm, tid, cpu < 0 ? 0 : cpu, ts, payload);
    pthread_mutex_unlock(&g_lock);
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

/* ── Destructor — flush and close file ───────────────────────────────────── */

static void atrace_fini(void)
{
    if (g_file) {
        fflush(g_file);
        fclose(g_file);
        g_file = NULL;
    }
}

/* ── Init ─────────────────────────────────────────────────────────────────── */

const trix_vtable_t* trix_backend_atrace_init(void)
{
    g_pid = (int)getpid();

    /* Read process comm name */
    FILE* comm_f = fopen("/proc/self/comm", "r");
    if (comm_f) {
        if (fgets(g_comm, sizeof(g_comm), comm_f)) {
            /* Strip trailing newline */
            g_comm[strcspn(g_comm, "\n")] = '\0';
        }
        fclose(comm_f);
    }

    /* Determine output path */
    const char* out_path = getenv("TRIX_FILE_OUT");
    char default_path[64];
    if (!out_path || out_path[0] == '\0') {
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        snprintf(default_path, sizeof(default_path),
                 "trix_%04d%02d%02d_%02d%02d%02d.atrace",
                 tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                 tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
        out_path = default_path;
    }

    g_file = fopen(out_path, "w");
    if (!g_file) {
        perror("trix: atrace: cannot open output file");
        abort();
    }

    /* Write ftrace-compatible header so Perfetto recognises the format */
    fprintf(g_file,
            "# tracer: nop\n"
            "#\n"
            "# entries-in-buffer/entries-written: 0/0   #P:%d\n"
            "#\n"
            "#           TASK-PID     CPU#  |||||  TIMESTAMP  FUNCTION\n"
            "#              | |         |   |||||     |         |\n",
            (int)sysconf(_SC_NPROCESSORS_ONLN));

    fprintf(stderr, "trix: atrace: writing to %s\n", out_path);

    atexit(atrace_fini);

    return &s_atrace_vtable;
}
