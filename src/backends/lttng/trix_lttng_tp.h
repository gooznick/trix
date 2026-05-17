/*
 * trix_lttng_tp.h — LTTng-UST tracepoint provider definition for trix.
 *
 * This header is included multiple times by the LTTng-UST machinery (once
 * per translation unit that needs probe definitions), so it intentionally
 * lacks a conventional one-shot include guard at the top level.
 *
 * Two API generations are supported:
 *   - lttng-ust < 2.14  (Ubuntu 18.04 – 22.04): TRACEPOINT_EVENT / tracepoint()
 *   - lttng-ust >= 2.14 (Ubuntu 24.04+):        LTTNG_UST_TRACEPOINT_EVENT /
 *                                                lttng_ust_tracepoint()
 *
 * CMake sets TRIX_LTTNG_UST_NEW_API when lttng-ust >= 2.14 is detected via
 * pkg-config.
 */

/* ── Provider / include registration (version-aware) ─────────────────────── */
#ifdef TRIX_LTTNG_UST_NEW_API
#  undef  LTTNG_UST_TRACEPOINT_PROVIDER
#  define LTTNG_UST_TRACEPOINT_PROVIDER trix
#  undef  LTTNG_UST_TRACEPOINT_INCLUDE
#  define LTTNG_UST_TRACEPOINT_INCLUDE  "trix_lttng_tp.h"
#else
#  undef  TRACEPOINT_PROVIDER
#  define TRACEPOINT_PROVIDER           trix
#  undef  TRACEPOINT_INCLUDE
#  define TRACEPOINT_INCLUDE            "trix_lttng_tp.h"
#endif

/* ── Multi-include guard (accept both old and new guard macro names) ──────── */
#if !defined(TRIX_LTTNG_TP_H) \
    || defined(TRACEPOINT_HEADER_MULTI_READ) \
    || defined(LTTNG_UST_TRACEPOINT_HEADER_MULTI_READ)
#define TRIX_LTTNG_TP_H

#include <lttng/tracepoint.h>
#include <stdint.h>

#ifdef TRIX_LTTNG_UST_NEW_API
/* ══════════════════════════ lttng-ust >= 2.14 API ══════════════════════════ */

LTTNG_UST_TRACEPOINT_EVENT(trix, frame_begin,
    LTTNG_UST_TP_ARGS(uint64_t, frame_num),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(uint64_t, frame_num, frame_num)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(trix, frame_end,
    LTTNG_UST_TP_ARGS(uint64_t, frame_num),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(uint64_t, frame_num, frame_num)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(trix, algo_begin,
    LTTNG_UST_TP_ARGS(const char *, name),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(name, name)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(trix, algo_end,
    LTTNG_UST_TP_ARGS(const char *, name),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(name, name)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(trix, data_int,
    LTTNG_UST_TP_ARGS(const char *, key, uint64_t, value),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(key, key)
        lttng_ust_field_integer(uint64_t, value, value)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(trix, data_float,
    LTTNG_UST_TP_ARGS(const char *, key, float, value),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(key, key)
        lttng_ust_field_float(float, value, value)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(trix, data_string,
    LTTNG_UST_TP_ARGS(const char *, key, const char *, value),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(key, key)
        lttng_ust_field_string(value, value)
    )
)

#else /* ══════════════════════ lttng-ust < 2.14 API ══════════════════════ */

TRACEPOINT_EVENT(trix, frame_begin,
    TP_ARGS(uint64_t, frame_num),
    TP_FIELDS(
        ctf_integer(uint64_t, frame_num, frame_num)
    )
)

TRACEPOINT_EVENT(trix, frame_end,
    TP_ARGS(uint64_t, frame_num),
    TP_FIELDS(
        ctf_integer(uint64_t, frame_num, frame_num)
    )
)

TRACEPOINT_EVENT(trix, algo_begin,
    TP_ARGS(const char *, name),
    TP_FIELDS(
        ctf_string(name, name)
    )
)

TRACEPOINT_EVENT(trix, algo_end,
    TP_ARGS(const char *, name),
    TP_FIELDS(
        ctf_string(name, name)
    )
)

TRACEPOINT_EVENT(trix, data_int,
    TP_ARGS(const char *, key, uint64_t, value),
    TP_FIELDS(
        ctf_string(key, key)
        ctf_integer(uint64_t, value, value)
    )
)

TRACEPOINT_EVENT(trix, data_float,
    TP_ARGS(const char *, key, float, value),
    TP_FIELDS(
        ctf_string(key, key)
        ctf_float(float, value, value)
    )
)

TRACEPOINT_EVENT(trix, data_string,
    TP_ARGS(const char *, key, const char *, value),
    TP_FIELDS(
        ctf_string(key, key)
        ctf_string(value, value)
    )
)

#endif /* TRIX_LTTNG_UST_NEW_API */

#endif /* TRIX_LTTNG_TP_H */

#include <lttng/tracepoint-event.h>
