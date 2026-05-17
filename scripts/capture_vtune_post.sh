#!/usr/bin/env sh
# capture_vtune_post.sh — open VTune results collected by capture_vtune_pre.sh.
#
# Prints the GUI open command and optionally generates a summary text report.
#
# Usage:
#   sh ./scripts/capture_vtune_post.sh

set -eu

STATE_FILE=/tmp/trix_vtune_state

# ── Read state written by pre ─────────────────────────────────────────────────

if [ ! -f "${STATE_FILE}" ]; then
    echo "ERROR: state file ${STATE_FILE} not found." >&2
    echo "  Did you run capture_vtune_pre.sh first?" >&2
    exit 1
fi

. "${STATE_FILE}"

if [ ! -d "${RESULT}" ]; then
    echo "ERROR: result directory '${RESULT}' not found." >&2
    exit 1
fi

rm -f "${STATE_FILE}"

# ── Report ────────────────────────────────────────────────────────────────────

VTUNE_FILE="${RESULT}/$(basename "${RESULT}").vtune"

echo "Result saved: ${RESULT}/"
echo ""
echo "Open in VTune GUI:"
echo "  vtune-gui ${VTUNE_FILE}"
echo ""
echo "Generate a text summary report:"
echo "  ${VTUNE} -report summary -r ${RESULT}"
echo "  ${VTUNE} -report top-down -r ${RESULT}"
