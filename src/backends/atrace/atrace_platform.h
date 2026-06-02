/*
 * atrace_platform.h — platform abstraction for the atrace backend.
 *
 * Two implementations are compiled behind #ifdef _WIN32 / #else.
 */

#ifndef ATRACE_PLATFORM_H
#define ATRACE_PLATFORM_H

#include <stdint.h>
#include <string.h>
#include <time.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Windows implementation
 * ═══════════════════════════════════════════════════════════════════════════ */
#ifdef _WIN32

#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#include <windows.h>

/* Process ID. */
static inline int atrace_getpid(void) { return (int)GetCurrentProcessId(); }
/* Kernel thread ID. */
static inline int atrace_gettid(void) { return (int)GetCurrentThreadId(); }
/* Current CPU number. */
static inline int atrace_getcpu(void) { return (int)GetCurrentProcessorNumber(); }

/* Monotonic timestamp in seconds.
 * QueryPerformanceFrequency returns the same value for the process
 * lifetime — the benign write race on first call is intentional. */
static inline double atrace_now_sec(void)
{
    static LARGE_INTEGER freq;
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
}

/* Process short name (basename without extension) into caller's buffer. */
static inline void atrace_getcomm(char* buf, int size)
{
    char path[MAX_PATH];
    if (!GetModuleFileNameA(NULL, path, MAX_PATH)) {
        strncpy(buf, "trix", size - 1);
        buf[size - 1] = '\0';
        return;
    }
    char* base = strrchr(path, '\\');
    base = base ? base + 1 : path;
    char* dot  = strrchr(base, '.');
    if (dot) *dot = '\0';
    strncpy(buf, base, size - 1);
    buf[size - 1] = '\0';
}

/* Logical CPU count. */
static inline int atrace_getcpucount(void)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
}

/* Thread-safe local time breakdown (Windows has reversed arg order vs POSIX). */
static inline void atrace_localtime(time_t t, struct tm* out)
{
    localtime_s(out, &t);  /* Windows: reversed arg order vs POSIX */
}

/* Open or create the trace file for writing; returns fd. */
static inline int atrace_open(const char* path)
{
    return _open(path,
                 _O_WRONLY | _O_CREAT | _O_TRUNC | _O_APPEND | _O_BINARY,
                 _S_IREAD | _S_IWRITE);
}

/* Write len bytes from buf to fd. */
static inline void atrace_write(int fd, const char* buf, int len)
{
    _write(fd, buf, len);
}

/* Close the trace file fd. */
static inline void atrace_close(int fd) { _close(fd); }


/* ═══════════════════════════════════════════════════════════════════════════
 * Linux / POSIX implementation
 * ═══════════════════════════════════════════════════════════════════════════ */
#else

#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

/* Process ID. */
static inline int atrace_getpid(void) { return (int)getpid(); }
/* Kernel thread ID. */
static inline int atrace_gettid(void) { return (int)syscall(SYS_gettid); }
/* Current CPU number. */
static inline int atrace_getcpu(void) { return sched_getcpu(); }

/* Monotonic timestamp in seconds.
 * CLOCK_MONOTONIC is portable and sufficient — Perfetto doesn't require
 * CLOCK_BOOTTIME since the atrace file carries its own time origin. */
static inline double atrace_now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* Process short name from /proc/self/comm into caller's buffer. */
static inline void atrace_getcomm(char* buf, int size)
{
    FILE* f = fopen("/proc/self/comm", "r");
    if (f) {
        if (fgets(buf, size, f))
            buf[strcspn(buf, "\n")] = '\0';
        fclose(f);
    }
}

/* Logical CPU count. */
static inline int atrace_getcpucount(void)
{
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
}

/* Thread-safe local time breakdown. */
static inline void atrace_localtime(time_t t, struct tm* out)
{
    localtime_r(&t, out);
}

/* Open or create the trace file for writing; returns fd.
 * O_APPEND: POSIX guarantees write() on such an fd is atomic —
 * seek-to-end and write happen as a single kernel operation, so no
 * mutex is needed in backend_atrace.c. */
static inline int atrace_open(const char* path)
{
    return open(path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0666);
}

/* Write len bytes from buf to fd. */
static inline void atrace_write(int fd, const char* buf, int len)
{
    (void)write(fd, buf, len);
}

/* Close the trace file fd. */
static inline void atrace_close(int fd) { close(fd); }

#endif  /* _WIN32 */

#endif  /* ATRACE_PLATFORM_H */
