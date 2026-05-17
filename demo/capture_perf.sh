#!/usr/bin/env sh
# capture_perf.sh — record trix demo with perf using SDT (USDT) probes.
#
# Requirements on target:
#   - perf  (linux-perf / perf-tools package)
#   - libtrix.so and trix_demo built with -DTRIX_BUILD_DEMO=ON
#   - Root or perf_event_paranoid <= 1:
#       echo 1 > /proc/sys/kernel/perf_event_paranoid
#
# WARNING — string arguments:
#   The perf backend passes string pointers to SDT probes. perf script
#   shows the raw pointer address (e.g. arg1=0x401a20), NOT the string
#   content. Span matching in perf_to_perfetto.py works correctly because
#   the same string literal always has the same pointer in the same binary.
#   To resolve pointer→name offline:
#     strings -t x build/demo/trix_demo | grep -E 'correlate|generate|estimate|...'
#   or:
#     readelf -p .rodata build/demo/trix_demo
#
# Usage:
#   sudo ./demo/capture_perf.sh [output_file]
#
# Output:
#   trix_perf.txt  — perf script text output (transfer to host)
#   Convert on host: python3 demo/perf_to_perfetto.py trix_perf.txt

set -eu

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
DEMO_BIN="${REPO_ROOT}/build/demo/trix_demo"
LIB_DIR="${REPO_ROOT}/build"
PERF_DATA="${SCRIPT_DIR}/trix_perf.data"
OUTPUT="${1:-${SCRIPT_DIR}/trix_perf.txt}"

# ── Checks ────────────────────────────────────────────────────────────────────

if [ ! -x "${DEMO_BIN}" ]; then
    echo "ERROR: ${DEMO_BIN} not found."
    echo "       Build first: cmake -B build -DTRIX_BUILD_DEMO=ON && cmake --build build"
    exit 1
fi

if ! command -v perf >/dev/null 2>&1; then
    echo "ERROR: perf not found. Install linux-perf or perf-tools."
    exit 1
fi

# ── Record ────────────────────────────────────────────────────────────────────

echo "Recording with perf..."
TRIX_BACKEND=perf LD_LIBRARY_PATH="${LIB_DIR}" \
    perf record \
        -e "sdt_trix:algo_begin" \
        -e "sdt_trix:algo_end" \
        -e "sdt_trix:frame_begin" \
        -e "sdt_trix:frame_end" \
        -e "sdt_trix:data_int" \
        -e "sdt_trix:data_float" \
        -e "sched:sched_switch" \
        -o "${PERF_DATA}" \
        -- "${DEMO_BIN}"

# ── Convert to text ───────────────────────────────────────────────────────────

echo "Converting to text..."
perf script -i "${PERF_DATA}" -F comm,tid,time,event,sym > "${OUTPUT}"

SIZE=$(du -h "${OUTPUT}" | cut -f1)
LINES=$(wc -l < "${OUTPUT}")
echo ""
echo "Trace saved: ${OUTPUT}  (${SIZE}, ${LINES} lines)"
echo ""
echo "Convert to Perfetto on the host:"
echo "  scp target:${OUTPUT} ."
echo "  python3 demo/perf_to_perfetto.py trix_perf.txt -o trix_perf.pftrace"
echo "  Open https://ui.perfetto.dev → drag and drop trix_perf.pftrace"
