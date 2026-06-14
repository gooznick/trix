#!/usr/bin/env sh
# capture_lttng_pre.sh — create an LTTng session and start tracing.
#
# Run this first, then start your application.
# When done, call capture_lttng_post.sh to stop and save.
#
# Usage:
#   sh ./scripts/capture_lttng_pre.sh          # UST only
#   sudo sh ./scripts/capture_lttng_pre.sh     # UST + kernel sched events
#
# Environment variables:
#   TRIX_SESSION_NAME      LTTng session name. Default: trix_YYYYMMDD_HHMMSS.
#   TRIX_LTTNG_NO_KERNEL   Set to 1 to skip kernel sched events even when root.

set -eu

STATE_FILE=/tmp/trix_lttng_state

# ── Prerequisites ─────────────────────────────────────────────────────────────

if ! command -v lttng >/dev/null 2>&1; then
    echo "ERROR: lttng not found.  apt install lttng-tools" >&2
    exit 1
fi

# ── Session setup ─────────────────────────────────────────────────────────────

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
SESSION="${TRIX_SESSION_NAME:-trix_${TIMESTAMP}}"
CTF_DIR="/tmp/${SESSION}"

lttng-sessiond --daemonize 2>/dev/null || true
sleep 0.3

lttng create "${SESSION}" --output="${CTF_DIR}" >/dev/null
lttng enable-event -u 'trix:*' >/dev/null
lttng add-context -u -t vpid -t vtid -t procname >/dev/null

# ── Kernel sched events (requires root + lttng-modules-dkms) ─────────────────

HAVE_KERNEL=0
if [ "${TRIX_LTTNG_NO_KERNEL:-0}" = "1" ]; then
    echo "  kernel events: skipped (TRIX_LTTNG_NO_KERNEL=1)"
elif [ "$(id -u)" -ne 0 ]; then
    echo "  kernel events: skipped (re-run with sudo to include sched_switch/sched_wakeup)"
else
    if lttng enable-event -k sched_switch >/dev/null 2>&1 && \
       lttng enable-event -k sched_wakeup  >/dev/null 2>&1; then
        HAVE_KERNEL=1
        echo "  kernel events: enabled (sched_switch, sched_wakeup)"
    else
        echo "  kernel events: unavailable — install lttng-modules-dkms and reboot:"
        echo "    sudo apt install lttng-modules-dkms"
    fi
fi

# ── Save state for capture_lttng_post.sh ─────────────────────────────────────

printf 'SESSION=%s\nCTF_DIR=%s\n' "${SESSION}" "${CTF_DIR}" > "${STATE_FILE}"

# ── Start ─────────────────────────────────────────────────────────────────────

lttng start >/dev/null

echo "LTTng session '${SESSION}' started."
echo "  CTF dir : ${CTF_DIR}"
echo ""
echo "Run your application, e.g.:"
echo "  TRIX_BACKEND=lttng LD_LIBRARY_PATH=\$PWD/build ./your_app"
echo ""
echo "When done: sh ./scripts/capture_lttng_post.sh"

