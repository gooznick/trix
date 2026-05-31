#!/usr/bin/env sh
# capture_vtune.sh — collect a trix run with Intel VTune (threading analysis).
#
# Launches VTune with the target command and returns immediately.
# VTune injects its ITT collector into the launched process, so ITT task
# spans (trix_algo_begin/end, trix_frame_begin/end) appear in the result.
# Open the result with: vtune-gui <result-dir>/<result-dir>.vtune
#
# Usage:
#   sh ./scripts/capture_vtune.sh <command> [args...]
#
# Environment variables:
#   TRIX_VTUNE_RESULT   Result directory path (absolute or relative to cwd).
#                       Default: trix_vtune_YYYYMMDD_HHMMSS
#   VTUNE_BIN           Path to vtune binary (optional, auto-detected).
#   LD_LIBRARY_PATH     Forwarded to the launched command if already set.
#
# Requirements:
#   - Intel VTune Profiler (free tier at intel.com/vtune)
#   - Does NOT require root

set -eu

# ── Arguments ─────────────────────────────────────────────────────────────────

if [ $# -eq 0 ]; then
    echo "Usage: $0 <command> [args...]" >&2
    echo "  e.g. $0 ./build/demo/trix_demo" >&2
    exit 1
fi

# ── Locate vtune binary ───────────────────────────────────────────────────────

find_vtune() {
    if [ -n "${VTUNE_BIN:-}" ]; then
        echo "${VTUNE_BIN}"
        return
    fi
    if command -v vtune >/dev/null 2>&1; then
        command -v vtune
        return
    fi
    for dir in \
        "${HOME}/intel/oneapi/vtune/latest/bin64" \
        /opt/intel/oneapi/vtune/latest/bin64 \
        /opt/intel/vtune_profiler/bin64; do
        if [ -x "${dir}/vtune" ]; then
            echo "${dir}/vtune"
            return
        fi
    done
    for f in "${HOME}"/intel/oneapi/vtune/*/bin64/vtune; do
        if [ -x "${f}" ]; then
            echo "${f}"
            return
        fi
    done
}

VTUNE=$(find_vtune || true)

if [ -z "${VTUNE}" ]; then
    echo "ERROR: vtune not found." >&2
    echo "  Install Intel VTune Profiler: https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler.html" >&2
    echo "  Or set: VTUNE_BIN=/path/to/vtune" >&2
    exit 1
fi

echo "Using vtune: ${VTUNE}"

# ── Check ptrace scope ────────────────────────────────────────────────────────

PTRACE=$(cat /proc/sys/kernel/yama/ptrace_scope 2>/dev/null || echo 0)
if [ "${PTRACE}" -gt 0 ]; then
    echo "ERROR: ptrace_scope=${PTRACE} — VTune cannot attach to target processes." >&2
    echo "  Fix: echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope" >&2
    exit 1
fi

# ── Output path (absolute so post.sh works from any cwd) ─────────────────────

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULT_REL="${TRIX_VTUNE_RESULT:-trix_vtune_${TIMESTAMP}}"
RESULT="$(pwd)/${RESULT_REL}"

# ── Launch VTune + app in background ─────────────────────────────────────────
#
# VTune injects its ITT collector into the launched process so that ITT
# task spans are captured.  The process is started with TRIX_BACKEND=itt.
#
# -collect threading: captures ITT tasks, context switches, CPU samples
# -knob sampling-and-waits=sw: user-mode only, no root required
# -knob enable-stack-collection=true: call stacks for CPU timeline

TRIX_BACKEND=itt \
LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}" \
"${VTUNE}" \
    -collect threading \
    -knob sampling-and-waits=sw \
    -knob enable-stack-collection=true \
    -result-dir "${RESULT}" \
    -- "$@" &

VTUNE_PID=$!

# Give vtune a moment to start before returning
sleep 1

if ! ps -p "${VTUNE_PID}" > /dev/null 2>&1; then
    echo "ERROR: vtune exited immediately — check the command and VTune installation." >&2
    exit 1
fi

echo "VTune collection started (PID ${VTUNE_PID})"
echo "  result dir : ${RESULT}"
echo ""
echo "Open when done: vtune-gui ${RESULT}/$(basename ${RESULT}).vtune"
