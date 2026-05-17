#!/usr/bin/env sh
# capture_perf.sh — record a trix-instrumented binary with perf (all-in-one).
#
# This is the all-in-one wrapper: runs capture_perf_pre.sh (which registers
# probes and records the run), then immediately calls capture_perf_post.sh
# (which exports the text trace and prints the converter hint).
#
# For manual workflows, call the two scripts separately:
#
#   sh ./scripts/capture_perf_pre.sh  <command> [args...]
#   sh ./scripts/capture_perf_post.sh
#
# Usage:
#   sudo sh ./scripts/capture_perf.sh <command> [args...]
#   # or without sudo if perf_event_paranoid <= 1
#
# Environment variables:
#   TRIX_BACKEND      Forwarded to the command.  Defaults to "perf".
#   LD_LIBRARY_PATH   Forwarded to the command if already set.
#   TRIX_PERF_OUT     Output text file path.
#                     Default: trix_perf_YYYYMMDD_HHMMSS.txt

set -eu

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

if [ $# -eq 0 ]; then
    echo "Usage: $0 <command> [args...]" >&2
    echo "  e.g. $0 ./build/demo/trix_demo" >&2
    exit 1
fi

sh "${SCRIPT_DIR}/capture_perf_pre.sh" "$@"
sh "${SCRIPT_DIR}/capture_perf_post.sh"
