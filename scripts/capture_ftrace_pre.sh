#!/usr/bin/env sh
# capture_ftrace_pre.sh — configure tracefs and start tracing.
#
# Run as root (or with sudo), then start your application.
# When done, call capture_ftrace_post.sh to stop and save.
#
# Usage:
#   sudo sh ./scripts/capture_ftrace_pre.sh
#
# Environment variables:
#   TRIX_BUFFER_KB   Per-CPU ring-buffer size in KB. Default: 8192.

set -eu

# Locate tracefs
TRACE_DIR=""
for dir in /sys/kernel/tracing /sys/kernel/debug/tracing; do
    if [ -f "${dir}/trace_marker" ]; then
        TRACE_DIR="${dir}"
        break
    fi
done

if [ -z "${TRACE_DIR}" ]; then
    if mount -t tracefs nodev /sys/kernel/tracing 2>/dev/null; then
        TRACE_DIR=/sys/kernel/tracing
    else
        echo "ERROR: tracefs not available. Check kernel config (CONFIG_FTRACE=y)." >&2
        exit 1
    fi
fi

echo "${TRACE_DIR}" > /tmp/trix_ftrace_state

# Configure buffer
echo 0   > "${TRACE_DIR}/tracing_on"
echo     > "${TRACE_DIR}/trace"
echo nop > "${TRACE_DIR}/current_tracer"
echo "${TRIX_BUFFER_KB:-8192}" > "${TRACE_DIR}/buffer_size_kb" 2>/dev/null || true

# Enable scheduling events
echo 1 > "${TRACE_DIR}/events/sched/sched_switch/enable"
echo 1 > "${TRACE_DIR}/events/sched/sched_wakeup/enable"
echo 1 > "${TRACE_DIR}/events/sched/sched_migrate_task/enable"

# Start tracing
echo 1 > "${TRACE_DIR}/tracing_on"

echo "Tracing started (${TRACE_DIR})"
echo ""
echo "Run your application, e.g.:"
echo "  ./your_app"
echo "  TRIX_BACKEND=ftrace LD_LIBRARY_PATH=\$PWD/build ./your_app"
echo ""
echo "When done: sudo sh ./scripts/capture_ftrace_post.sh"
