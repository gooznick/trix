#!/usr/bin/env sh
# capture.sh — record trix demo for Perfetto on a Linux embedded target.
#
# Requirements on target:
#   - Linux kernel with CONFIG_FTRACE + CONFIG_SCHED_TRACER
#   - libtrix.so and trix_demo built (cmake -DTRIX_BUILD_DEMO=ON)
#   - Root / sudo access
#
# No extra packages required beyond a shell and a mounted (or mountable) tracefs.
#
# Usage:
#   sudo ./demo/capture.sh [output_file]
#
# Output:
#   trix_trace.txt  (transfer to host, open with Perfetto: https://ui.perfetto.dev)

set -eu

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
DEMO_BIN="${REPO_ROOT}/build/demo/trix_demo"
LIB_DIR="${REPO_ROOT}/build"
OUTPUT="${1:-${SCRIPT_DIR}/trix_trace.txt}"

# ── Locate tracefs ────────────────────────────────────────────────────────────

TRACE_DIR=""
for dir in /sys/kernel/tracing /sys/kernel/debug/tracing; do
    if [ -f "${dir}/trace_marker" ]; then
        TRACE_DIR="${dir}"
        break
    fi
done

if [ -z "${TRACE_DIR}" ]; then
    # Try mounting tracefs
    if mount -t tracefs nodev /sys/kernel/tracing 2>/dev/null; then
        TRACE_DIR=/sys/kernel/tracing
    elif mount -t debugfs nodev /sys/kernel/debug 2>/dev/null && \
         [ -f /sys/kernel/debug/tracing/trace_marker ]; then
        TRACE_DIR=/sys/kernel/debug/tracing
    else
        echo "ERROR: tracefs not available. Check kernel config (CONFIG_TRACEFS=y)."
        exit 1
    fi
fi

echo "Using tracefs: ${TRACE_DIR}"

# ── Check demo binary ─────────────────────────────────────────────────────────

if [ ! -x "${DEMO_BIN}" ]; then
    echo "ERROR: ${DEMO_BIN} not found."
    echo "       Build first: cmake -B build -DTRIX_BUILD_DEMO=ON && cmake --build build"
    exit 1
fi

# ── Configure trace buffer ────────────────────────────────────────────────────

echo 0 > "${TRACE_DIR}/tracing_on"
echo > "${TRACE_DIR}/trace"
echo nop > "${TRACE_DIR}/current_tracer"

# Per-CPU buffer size (KB) — increase if trace is truncated
echo 8192 > "${TRACE_DIR}/buffer_size_kb" 2>/dev/null || true

# Enable scheduling events (threads, cores, context switches, migrations)
echo 1 > "${TRACE_DIR}/events/sched/sched_switch/enable"
echo 1 > "${TRACE_DIR}/events/sched/sched_wakeup/enable"
echo 1 > "${TRACE_DIR}/events/sched/sched_migrate_task/enable"

# trix ftrace backend writes to trace_marker — already enabled by default.
# Ensure the tracing_mark_write event is visible (optional, ignore errors):
( echo 1 > "${TRACE_DIR}/events/ftrace/print/enable" ) 2>/dev/null || true

# ── Run ───────────────────────────────────────────────────────────────────────

echo "Recording — running demo at 50 Hz for 200 frames..."
echo 1 > "${TRACE_DIR}/tracing_on"

TRIX_BACKEND=ftrace LD_LIBRARY_PATH="${LIB_DIR}" "${DEMO_BIN}"

echo 0 > "${TRACE_DIR}/tracing_on"

# ── Save trace ────────────────────────────────────────────────────────────────

cat "${TRACE_DIR}/trace" > "${OUTPUT}"

# Disable events (good practice)
echo 0 > "${TRACE_DIR}/events/sched/sched_switch/enable"
echo 0 > "${TRACE_DIR}/events/sched/sched_wakeup/enable"
echo 0 > "${TRACE_DIR}/events/sched/sched_migrate_task/enable"
( echo 0 > "${TRACE_DIR}/events/ftrace/print/enable" ) 2>/dev/null || true

LINES=$(wc -l < "${OUTPUT}")
SIZE=$(du -h "${OUTPUT}" | cut -f1)
echo ""
echo "Trace saved: ${OUTPUT}  (${SIZE}, ${LINES} lines)"
echo ""
echo "Transfer to host and open with Perfetto:"
echo "  scp ${OUTPUT} user@host:~/"
echo "  Open https://ui.perfetto.dev → drag and drop trix_trace.txt"
echo ""
echo "In Perfetto: look for process 'trix_demo' with threads:"
echo "  trix_demo      — main loop (frame_N spans, generate/dispatch/wait/estimate)"
echo "  trix_worker_0..3 — parallel correlate spans"
