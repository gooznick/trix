#!/usr/bin/env sh
# capture_perf_pre.sh — register SDT probes and record a trix-instrumented run.
#
# perf record blocks until the command exits (or Ctrl+C stops it).
# After this script finishes, call capture_perf_post.sh to export the trace.
#
# Usage:
#   sudo sh ./scripts/capture_perf_pre.sh <command> [args...]
#   # or without sudo if perf_event_paranoid <= 1
#
# Environment variables:
#   TRIX_BACKEND      Forwarded to the command.  Defaults to "perf".
#   LD_LIBRARY_PATH   Forwarded to the command if already set.
#   TRIX_PERF_OUT     Output text file path (sets default; can override in post).
#                     Default: trix_perf_YYYYMMDD_HHMMSS.txt
#
# Requirements:
#   - perf  (apt install linux-perf  or  linux-tools-$(uname -r))
#   - Root or: echo 1 | sudo tee /proc/sys/kernel/perf_event_paranoid
#
# WARNING — string arguments in perf output:
#   The perf backend passes string pointers to SDT probes.  perf script shows
#   the raw pointer address (e.g. arg1=0x401a20), NOT the string content.
#   To resolve pointer → name offline:
#     strings <binary> | grep -E 'correlate|generate|estimate|...'

set -eu

STATE_FILE=/tmp/trix_perf_state

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

# ── Checks ────────────────────────────────────────────────────────────────────

if ! command -v perf >/dev/null 2>&1; then
    echo "ERROR: perf not found." >&2
    echo "  apt install linux-perf  or  linux-tools-$(uname -r)" >&2
    exit 1
fi

PARANOID=$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo 3)
if [ "${PARANOID}" -gt 1 ] && [ "$(id -u)" -ne 0 ]; then
    echo "WARNING: perf_event_paranoid=${PARANOID} — recording may fail." >&2
    echo "  Fix: echo 1 | sudo tee /proc/sys/kernel/perf_event_paranoid" >&2
fi

# ── Output paths ──────────────────────────────────────────────────────────────

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT="${TRIX_PERF_OUT:-trix_perf_${TIMESTAMP}.txt}"
PERF_DATA="${OUTPUT%.txt}.data"

# ── Locate libtrix.so and register SDT probes ─────────────────────────────────

find_libtrix() {
    if [ -n "${TRIX_LIB:-}" ]; then
        echo "${TRIX_LIB}"
        return
    fi
    LIBTRIX=$(ldd "${CMD}" 2>/dev/null \
        | awk '/libtrix/{print $3}' \
        | head -1)
    if [ -n "${LIBTRIX}" ] && [ -f "${LIBTRIX}" ]; then
        echo "${LIBTRIX}"
        return
    fi
    OLD_IFS="${IFS}"; IFS=:
    for dir in ${LD_LIBRARY_PATH:-}; do
        if [ -f "${dir}/libtrix.so" ]; then
            IFS="${OLD_IFS}"
            echo "${dir}/libtrix.so"
            return
        fi
    done
    IFS="${OLD_IFS}"
}

LIBTRIX=$(find_libtrix)

if [ -z "${LIBTRIX}" ]; then
    echo "ERROR: libtrix.so not found." >&2
    echo "  Set LD_LIBRARY_PATH to the directory containing libtrix.so, or:" >&2
    echo "  TRIX_LIB=/path/to/libtrix.so $0 ${CMD}" >&2
    exit 1
fi

echo "Registering SDT probes from: ${LIBTRIX}"
perf buildid-cache --add "${LIBTRIX}"

perf probe --del 'sdt_trix:*' >/dev/null 2>&1 || true

for probe in algo_begin algo_end frame_begin frame_end data_int data_float data_string; do
    perf probe --add "sdt_trix:${probe}" >/dev/null
done
echo "  Probes registered."
echo ""

# ── Save state for capture_perf_post.sh ───────────────────────────────────────

printf 'PERF_DATA=%s\nOUTPUT=%s\n' "${PERF_DATA}" "${OUTPUT}" > "${STATE_FILE}"

# ── Record ────────────────────────────────────────────────────────────────────

echo "Recording '${CMD}${CMD_ARGS:+ ${CMD_ARGS}}' with perf..."
echo "  perf data : ${PERF_DATA}"
echo ""

TRIX_BACKEND="${TRIX_BACKEND:-perf}" \
    perf record \
        -e "sdt_trix:algo_begin" \
        -e "sdt_trix:algo_end" \
        -e "sdt_trix:frame_begin" \
        -e "sdt_trix:frame_end" \
        -e "sdt_trix:data_int" \
        -e "sdt_trix:data_float" \
        -e "sched:sched_switch" \
        -o "${PERF_DATA}" \
        -- "${CMD}" ${CMD_ARGS} || true

echo ""
echo "Recording done.  Export the trace:"
echo "  sh ./scripts/capture_perf_post.sh"
