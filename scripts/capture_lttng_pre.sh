#!/usr/bin/env sh
# capture_lttng_pre.sh — create an LTTng session and record a trix run.
#
# The LTTng session stays ACTIVE after the command exits (or Ctrl+C).
# You can run more commands before calling capture_lttng_post.sh.
#
# Usage:
#   ./scripts/capture_lttng_pre.sh <command> [args...]
#   # With context switches (requires root + lttng-modules-dkms):
#   sudo ./scripts/capture_lttng_pre.sh <command> [args...]
#
# Environment variables:
#   TRIX_BACKEND      Forwarded to the command.  Defaults to "lttng".
#   LD_LIBRARY_PATH   Forwarded to the command if already set.
#
# Requirements:
#   - lttng-tools     (apt install lttng-tools)
#   - liblttng-ust-dev  (apt install liblttng-ust-dev)
#   - For context switches: lttng-modules-dkms + root

set -eu

STATE_FILE=/tmp/trix_lttng_state

# ── Arguments ─────────────────────────────────────────────────────────────────

if [ $# -eq 0 ]; then
    echo "Usage: $0 <command> [args...]" >&2
    echo "  e.g. $0 ./build/demo/trix_demo" >&2
    exit 1
fi

CMD="$1"; shift
CMD_ARGS="$*"

if [ ! -x "${CMD}" ]; then
    echo "ERROR: '${CMD}' is not executable or does not exist." >&2
    exit 1
fi

if ! command -v lttng >/dev/null 2>&1; then
    echo "ERROR: lttng not found." >&2
    echo "  apt install lttng-tools" >&2
    exit 1
fi

# ── Session setup ─────────────────────────────────────────────────────────────

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
SESSION="trix_${TIMESTAMP}"
CTF_DIR="/tmp/${SESSION}"

lttng-sessiond --daemonize 2>/dev/null || true
sleep 0.5

lttng create "${SESSION}" --output="${CTF_DIR}" >/dev/null
lttng enable-event -u 'trix:*' >/dev/null
lttng add-context -u -t vpid -t vtid -t procname >/dev/null

# ── Kernel sched_switch (requires lttng-modules-dkms + root) ─────────────────

HAVE_SCHED=0
if [ "$(id -u)" -eq 0 ]; then
    if lttng enable-event -k sched_switch >/dev/null 2>&1; then
        HAVE_SCHED=1
        echo "  kernel sched_switch: enabled"
    else
        echo "  kernel sched_switch: unavailable (install lttng-modules-dkms for context switches)"
    fi
else
    echo "  kernel sched_switch: skipped (run as root to include context switches)"
fi

# ── Save state for capture_lttng_post.sh ─────────────────────────────────────

printf 'SESSION=%s\nCTF_DIR=%s\n' "${SESSION}" "${CTF_DIR}" > "${STATE_FILE}"
if [ -n "${SUDO_USER:-}" ]; then
    printf 'ORIG_USER=%s\n' "${SUDO_USER}" >> "${STATE_FILE}"
fi

# ── Record ────────────────────────────────────────────────────────────────────

lttng start >/dev/null

echo "Recording '${CMD}${CMD_ARGS:+ ${CMD_ARGS}}' with LTTng..."
echo "  session : ${SESSION}"
echo "  CTF dir : ${CTF_DIR}"
echo "(Ctrl+C stops the command but the session stays active.)"
echo ""

trap '' INT

TRIX_BACKEND="${TRIX_BACKEND:-lttng}" \
    LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}" \
    "${CMD}" ${CMD_ARGS} || true

trap - INT

echo ""
echo "Command finished.  Session is still active."
echo "Run more commands, then save the trace:"
echo "  sh ./scripts/capture_lttng_post.sh"
