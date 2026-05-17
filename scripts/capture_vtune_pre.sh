#!/usr/bin/env sh
# capture_vtune_pre.sh — collect a trix run with Intel VTune (threading analysis).
#
# Uses the "threading" analysis type which captures:
#   - ITT tasks (algo_begin/end, frame_begin/end) with full string names
#   - Thread context switches and wait states (CPU timeline)
#   - CPU sampling (call stacks, hotspots)
#   - ITT metadata (data_int, data_float, data_string)
#
# This script blocks until the target command exits.
# Call capture_vtune_post.sh afterward to open results.
#
# Usage:
#   sh ./scripts/capture_vtune_pre.sh <command> [args...]
#
# Environment variables:
#   TRIX_VTUNE_RESULT   Result directory path.
#                       Default: trix_vtune_YYYYMMDD_HHMMSS
#   VTUNE_BIN           Path to vtune binary.
#                       Default: auto-detected from PATH or common install locations.
#   LD_LIBRARY_PATH     Forwarded to the command if already set.
#
# Requirements:
#   - Intel VTune Profiler (free tier at intel.com/vtune)
#   - Does NOT require root (user-mode sampling is the default)

set -eu

STATE_FILE=/tmp/trix_vtune_state

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

# ── Locate vtune binary ───────────────────────────────────────────────────────

find_vtune() {
    # 1. Explicit override
    if [ -n "${VTUNE_BIN:-}" ]; then
        echo "${VTUNE_BIN}"
        return
    fi
    # 2. PATH
    if command -v vtune >/dev/null 2>&1; then
        command -v vtune
        return
    fi
    # 3. Common oneapi install locations
    for dir in \
        "${HOME}/intel/oneapi/vtune/latest/bin64" \
        /opt/intel/oneapi/vtune/latest/bin64 \
        /opt/intel/vtune_profiler/bin64; do
        if [ -x "${dir}/vtune" ]; then
            echo "${dir}/vtune"
            return
        fi
    done
    # 4. Glob under ~/intel/oneapi/vtune/
    for f in "${HOME}"/intel/oneapi/vtune/*/bin64/vtune; do
        if [ -x "${f}" ]; then
            echo "${f}"
            return
        fi
    done
}

VTUNE=$(find_vtune)

if [ -z "${VTUNE}" ]; then
    echo "ERROR: vtune not found." >&2
    echo "  Install Intel VTune Profiler: https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler.html" >&2
    echo "  Or set: VTUNE_BIN=/path/to/vtune" >&2
    exit 1
fi

echo "Using vtune: ${VTUNE}"

# ── Output path ───────────────────────────────────────────────────────────────

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULT="${TRIX_VTUNE_RESULT:-trix_vtune_${TIMESTAMP}}"

printf 'RESULT=%s\nVTUNE=%s\n' "${RESULT}" "${VTUNE}" > "${STATE_FILE}"

# ── Locate libtrix.so ─────────────────────────────────────────────────────────
#
# VTune does NOT require root — do not run this script with sudo.
# If libtrix.so is in a non-standard location, set LD_LIBRARY_PATH before
# calling this script:
#   LD_LIBRARY_PATH=$PWD/build ./scripts/capture_vtune_pre.sh ./build/demo/trix_demo

find_libtrix() {
    if [ -n "${TRIX_LIB:-}" ]; then echo "${TRIX_LIB}"; return; fi
    LIBTRIX=$(ldd "${CMD}" 2>/dev/null | awk '/libtrix/{print $3}' | head -1)
    if [ -n "${LIBTRIX}" ] && [ -f "${LIBTRIX}" ]; then echo "${LIBTRIX}"; return; fi
    OLD_IFS="${IFS}"; IFS=:
    for dir in ${LD_LIBRARY_PATH:-}; do
        if [ -f "${dir}/libtrix.so" ]; then
            IFS="${OLD_IFS}"; echo "${dir}/libtrix.so"; return
        fi
    done
    IFS="${OLD_IFS}"
}

LIBTRIX=$(find_libtrix)
if [ -z "${LIBTRIX}" ]; then
    echo "ERROR: libtrix.so not found.  Set LD_LIBRARY_PATH before calling this script:" >&2
    echo "  LD_LIBRARY_PATH=\$PWD/build $0 ${CMD}" >&2
    echo "  (do NOT use sudo — VTune does not require root)" >&2
    rm -f "${STATE_FILE}"
    exit 1
fi
echo "  libtrix.so : ${LIBTRIX}"

# ── Check ptrace scope ────────────────────────────────────────────────────────

PTRACE=$(cat /proc/sys/kernel/yama/ptrace_scope 2>/dev/null || echo 0)
if [ "${PTRACE}" -gt 0 ]; then
    echo "ERROR: ptrace_scope=${PTRACE} — VTune cannot attach to the child process." >&2
    echo "  Fix: echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope" >&2
    rm -f "${STATE_FILE}"
    exit 1
fi
#
# threading analysis captures:
#   - Context switches (thread transitions, wait reasons, sleep/spin time)
#   - ITT tasks from the running process
#   - CPU samples with call stacks
#
# -knob sampling-and-waits=sw  — user-mode, no root required
# -knob enable-stack-collection=true — call stacks for CPU timeline

echo "Collecting '${CMD}${CMD_ARGS:+ ${CMD_ARGS}}' with VTune threading analysis..."
echo "  result dir : ${RESULT}"
echo ""

TRIX_BACKEND=itt \
    LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}" \
    "${VTUNE}" \
        -collect threading \
        -knob sampling-and-waits=sw \
        -knob enable-stack-collection=true \
        -result-dir "${RESULT}" \
        -- "${CMD}" ${CMD_ARGS} || true

echo ""
echo "Collection done.  Open the results:"
echo "  sh ./scripts/capture_vtune_post.sh"
