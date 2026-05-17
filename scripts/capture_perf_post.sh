#!/usr/bin/env sh
# capture_perf_post.sh — export perf data to text and print converter hint.
#
# Run this after capture_perf_pre.sh.
#
# Usage:
#   sh ./scripts/capture_perf_post.sh
#
# Environment variables:
#   TRIX_PERF_OUT   Override the output text file path.
#                   Default: value saved by capture_perf_pre.sh

set -eu

STATE_FILE=/tmp/trix_perf_state

# ── Read state written by pre ─────────────────────────────────────────────────

if [ ! -f "${STATE_FILE}" ]; then
    echo "ERROR: state file ${STATE_FILE} not found." >&2
    echo "  Did you run capture_perf_pre.sh first?" >&2
    exit 1
fi

# Load PERF_DATA and OUTPUT from state file
. "${STATE_FILE}"

# Allow env var override of output path
if [ -n "${TRIX_PERF_OUT:-}" ]; then
    OUTPUT="${TRIX_PERF_OUT}"
fi

if [ ! -f "${PERF_DATA}" ]; then
    echo "ERROR: perf data file '${PERF_DATA}' not found." >&2
    exit 1
fi

# ── Export to text ────────────────────────────────────────────────────────────

echo "Exporting: ${PERF_DATA} → ${OUTPUT}"

perf script -i "${PERF_DATA}" \
    -F comm,tid,cpu,time,event,trace \
    --show-mmap-events \
    > "${OUTPUT}"

# Fix ownership if running under sudo
if [ -n "${SUDO_USER:-}" ]; then
    chown "${SUDO_USER}:" "${PERF_DATA}" "${OUTPUT}" 2>/dev/null || true
fi

rm -f "${STATE_FILE}"

# ── Report ────────────────────────────────────────────────────────────────────

LINES=$(wc -l < "${OUTPUT}")
SIZE=$(du -h "${OUTPUT}" | cut -f1)
PFTRACE="${OUTPUT%.txt}.pftrace"

echo ""
echo "Trace saved: ${OUTPUT}  (${SIZE}, ${LINES} lines)"
echo ""
echo "Convert to Perfetto:"
echo "  python3 scripts/perf_to_perfetto.py ${OUTPUT} -o ${PFTRACE}"
echo "  Open https://ui.perfetto.dev → drag and drop ${PFTRACE}"
