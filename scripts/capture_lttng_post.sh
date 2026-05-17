#!/usr/bin/env sh
# capture_lttng_post.sh — stop the LTTng session and export the trace.
#
# Run this after capture_lttng_pre.sh (and any number of instrumented runs).
#
# Usage:
#   sh ./scripts/capture_lttng_post.sh
#   # (or sudo if the session was started as root)
#
# Environment variables:
#   TRIX_LTTNG_OUT   Output text file path.
#                    Default: trix_lttng_YYYYMMDD_HHMMSS.txt

set -eu

STATE_FILE=/tmp/trix_lttng_state

# ── Read state written by pre ─────────────────────────────────────────────────

if [ ! -f "${STATE_FILE}" ]; then
    echo "ERROR: state file ${STATE_FILE} not found." >&2
    echo "  Did you run capture_lttng_pre.sh first?" >&2
    exit 1
fi

. "${STATE_FILE}"

if ! command -v babeltrace2 >/dev/null 2>&1; then
    echo "ERROR: babeltrace2 not found." >&2
    echo "  apt install babeltrace2" >&2
    exit 1
fi

# ── Stop session ──────────────────────────────────────────────────────────────

lttng stop >/dev/null
echo "Session stopped: ${SESSION}"

# ── Export CTF → text ─────────────────────────────────────────────────────────

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT="${TRIX_LTTNG_OUT:-trix_lttng_${TIMESTAMP}.txt}"

echo "Exporting: ${CTF_DIR} → ${OUTPUT}"
babeltrace2 "${CTF_DIR}" > "${OUTPUT}"

# Fix ownership if started under sudo
ORIG="${ORIG_USER:-${SUDO_USER:-}}"
if [ -n "${ORIG}" ]; then
    chown "${ORIG}:" "${OUTPUT}" 2>/dev/null || true
fi

# ── Cleanup ───────────────────────────────────────────────────────────────────

lttng destroy "${SESSION}" >/dev/null
rm -rf "${CTF_DIR}"
rm -f "${STATE_FILE}"

# ── Report ────────────────────────────────────────────────────────────────────

LINES=$(wc -l < "${OUTPUT}")
SIZE=$(du -h "${OUTPUT}" | cut -f1)
PFTRACE="${OUTPUT%.txt}.pftrace"

echo ""
echo "Trace saved: ${OUTPUT}  (${SIZE}, ${LINES} lines)"
echo ""
echo "Convert to Perfetto:"
echo "  python3 scripts/lttng_to_perfetto.py ${OUTPUT} -o ${PFTRACE}"
echo "  Open https://ui.perfetto.dev → drag and drop ${PFTRACE}"
