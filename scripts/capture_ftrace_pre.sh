#!/usr/bin/env sh
# capture_ftrace_pre.sh — configure tracefs, start tracing, run a command.
#
# Tracing stays ACTIVE after this script exits, even if the command was
# killed with Ctrl+C.  Call capture_ftrace_post.sh when ready to save.
#
# Usage:
#   sudo sh ./scripts/capture_ftrace_pre.sh <command> [args...]
#
# Environment variables:
#   TRIX_BACKEND      Forwarded to the command.  Defaults to "ftrace".
#   LD_LIBRARY_PATH   Forwarded to the command if already set.
#   TRIX_BUFFER_KB    Per-CPU ring-buffer size in KB.  Default: 8192.
#
# Requirements:
#   - Root (writes to /sys/kernel/tracing or /sys/kernel/debug/tracing)
#   - Kernel with CONFIG_FTRACE=y  (standard on all Ubuntu kernels)

set -eu

# ── Arguments ─────────────────────────────────────────────────────────────────

if [ $# -eq 0 ]; then
    echo "Usage: $0 <command> [args...]" >&2
    echo "  e.g. sudo sh $0 ./build/demo/trix_demo" >&2
    exit 1
fi

CMD="$1"; shift
CMD_ARGS="$*"

if [ ! -x "${CMD}" ]; then
    echo "ERROR: '${CMD}' is not executable or does not exist." >&2
    exit 1
fi

# ── Locate tracefs ────────────────────────────────────────────────────────────

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
    elif mount -t debugfs nodev /sys/kernel/debug 2>/dev/null && \
         [ -f /sys/kernel/debug/tracing/trace_marker ]; then
        TRACE_DIR=/sys/kernel/debug/tracing
    else
        echo "ERROR: tracefs not available. Check kernel config (CONFIG_TRACEFS=y)." >&2
        exit 1
    fi
fi

echo "Using tracefs: ${TRACE_DIR}"

# ── Save state for capture_ftrace_post.sh ─────────────────────────────────────

STATE_FILE=/tmp/trix_ftrace_state
printf '%s\n' "${TRACE_DIR}" > "${STATE_FILE}"

# ── Configure trace buffer ────────────────────────────────────────────────────

echo 0 > "${TRACE_DIR}/tracing_on"
echo  > "${TRACE_DIR}/trace"
echo nop > "${TRACE_DIR}/current_tracer"
echo "${TRIX_BUFFER_KB:-8192}" > "${TRACE_DIR}/buffer_size_kb" 2>/dev/null || true

echo 1 > "${TRACE_DIR}/events/sched/sched_switch/enable"
echo 1 > "${TRACE_DIR}/events/sched/sched_wakeup/enable"
echo 1 > "${TRACE_DIR}/events/sched/sched_migrate_task/enable"
( echo 1 > "${TRACE_DIR}/events/ftrace/print/enable" ) 2>/dev/null || true

# ── Start tracing and run the command ─────────────────────────────────────────

echo 1 > "${TRACE_DIR}/tracing_on"
echo "Tracing started."
echo "Running: ${CMD}${CMD_ARGS:+ ${CMD_ARGS}}"
echo "(Ctrl+C stops the command but tracing stays active.)"
echo ""

# Ignore SIGINT in this shell — child process still receives it and exits,
# but tracing is not stopped.
trap '' INT

TRIX_BACKEND="${TRIX_BACKEND:-ftrace}" \
    LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}" \
    "${CMD}" ${CMD_ARGS} || true

trap - INT

echo ""
echo "Command finished.  Tracing is still active."
echo "Add more runs if needed, then save the trace:"
echo "  sudo sh ./scripts/capture_ftrace_post.sh"
