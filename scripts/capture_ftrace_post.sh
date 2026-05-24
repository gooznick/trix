#!/usr/bin/env sh
# capture_ftrace_post.sh — stop tracing and save the trace to a file.
#
# Run after capture_ftrace_pre.sh and your application.
#
# Usage:
#   sudo sh ./scripts/capture_ftrace_post.sh
#
# Environment variables:
#   TRIX_FTRACE_OUT   Output file path.
#                     Default: trix_ftrace_YYYYMMDD_HHMMSS.txt

set -eu

# Locate tracefs (from state file written by pre, or auto-detect)
STATE_FILE=/tmp/trix_ftrace_state
if [ -f "${STATE_FILE}" ]; then
    TRACE_DIR=$(cat "${STATE_FILE}")
else
    TRACE_DIR=""
    for dir in /sys/kernel/tracing /sys/kernel/debug/tracing; do
        if [ -f "${dir}/tracing_on" ]; then
            TRACE_DIR="${dir}"
            break
        fi
    done
fi

if [ -z "${TRACE_DIR}" ]; then
    echo "ERROR: tracefs not found. Did you run capture_ftrace_pre.sh first?" >&2
    exit 1
fi

# Stop tracing
echo 0 > "${TRACE_DIR}/tracing_on"

# Save output
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT="${TRIX_FTRACE_OUT:-trix_ftrace_${TIMESTAMP}.txt}"
cat "${TRACE_DIR}/trace" > "${OUTPUT}"

if [ -n "${SUDO_USER:-}" ]; then
    chown "${SUDO_USER}:" "${OUTPUT}" 2>/dev/null || true
fi

# Disable events
echo 0 > "${TRACE_DIR}/events/sched/sched_switch/enable"
echo 0 > "${TRACE_DIR}/events/sched/sched_wakeup/enable"
echo 0 > "${TRACE_DIR}/events/sched/sched_migrate_task/enable"

rm -f "${STATE_FILE}"

LINES=$(wc -l < "${OUTPUT}")
SIZE=$(du -h "${OUTPUT}" | cut -f1)
echo "Trace saved: ${OUTPUT}  (${SIZE}, ${LINES} lines)"
echo "Open with Perfetto: https://ui.perfetto.dev"
