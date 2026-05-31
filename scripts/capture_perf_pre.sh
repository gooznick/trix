#!/usr/bin/env sh
# capture_perf_pre.sh — register SDT probes and start system-wide perf recording.
#
# Run as root (or with sudo), then start your application.
# When done, call capture_perf_post.sh to stop and export.
#
# Usage:
#   sudo sh ./scripts/capture_perf_pre.sh
#
# Environment variables:
#   TRIX_LIB        Path to libtrix.so. Default: auto-detected from LD_LIBRARY_PATH or ./build/
#   TRIX_PERF_OUT   Output text file path. Default: trix_perf_YYYYMMDD_HHMMSS.txt
#
# Requirements:
#   - perf  (apt install linux-perf  or  linux-tools-$(uname -r))
#   - sudo / root (SDT probe events require tracefs read access)
#
# WARNING — string arguments in perf output:
#   The perf backend passes string pointers to SDT probes.  perf script shows
#   the raw pointer address (e.g. arg1=0x401a20), NOT the string content.
#   perf_to_perfetto.py resolves these to algo_0, algo_1, ctr_0, etc.

set -eu

STATE_FILE=/tmp/trix_perf_state

# ── Checks ────────────────────────────────────────────────────────────────────

if ! command -v perf >/dev/null 2>&1; then
    echo "ERROR: perf not found." >&2
    echo "  apt install linux-perf  or  linux-tools-$(uname -r)" >&2
    exit 1
fi

# ── Output paths ──────────────────────────────────────────────────────────────

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT="${TRIX_PERF_OUT:-trix_perf_${TIMESTAMP}.txt}"
PERF_DATA="${OUTPUT%.txt}.data"

# ── Locate libtrix.so ─────────────────────────────────────────────────────────

find_libtrix() {
    if [ -n "${TRIX_LIB:-}" ]; then
        echo "${TRIX_LIB}"
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
    if [ -f "./build/libtrix.so" ]; then
        echo "./build/libtrix.so"
    fi
}

LIBTRIX=$(find_libtrix)

if [ -z "${LIBTRIX}" ]; then
    echo "ERROR: libtrix.so not found." >&2
    echo "  Set LD_LIBRARY_PATH to the directory containing libtrix.so, or:" >&2
    echo "  TRIX_LIB=/path/to/libtrix.so $0" >&2
    exit 1
fi

# ── Register SDT probes ───────────────────────────────────────────────────────

echo "Registering SDT probes from: ${LIBTRIX}"
perf buildid-cache --add "${LIBTRIX}"
perf probe --del 'sdt_trix:*' 2>/dev/null || true
for probe in algo_begin algo_end frame_begin frame_end data_int data_float data_string; do
    perf probe -x "${LIBTRIX}" "sdt_trix:${probe}"
done
echo "  Probes registered."
echo ""

# ── Start recording (system-wide, background) ─────────────────────────────────

perf record -a \
    -e "sdt_trix:algo_begin" \
    -e "sdt_trix:algo_end" \
    -e "sdt_trix:frame_begin" \
    -e "sdt_trix:frame_end" \
    -e "sdt_trix:data_int" \
    -e "sdt_trix:data_float" \
    -e "sdt_trix:data_string" \
    -e "sched:sched_switch" \
    -o "${PERF_DATA}" &
PERF_PID=$!

printf 'PERF_PID=%s\nPERF_DATA=%s\nOUTPUT=%s\n' "${PERF_PID}" "${PERF_DATA}" "${OUTPUT}" > "${STATE_FILE}"

echo "perf recording started (PID ${PERF_PID})"
echo "  perf data : ${PERF_DATA}"
echo ""
echo "Run your application:"
echo "  TRIX_BACKEND=perf LD_LIBRARY_PATH=\$PWD/build ./your_app"
echo ""
echo "When done: sudo sh ./scripts/capture_perf_post.sh"
