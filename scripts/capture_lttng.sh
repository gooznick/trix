#!/usr/bin/env sh
# capture_lttng.sh — record a trix-instrumented binary with LTTng (all-in-one).
#
# This is the all-in-one wrapper: runs capture_lttng_pre.sh (which sets up
# the session and runs the command), then immediately calls
# capture_lttng_post.sh (which stops the session and exports the trace).
#
# For manual multi-run or delayed-save workflows, call the two scripts
# separately:
#
#   ./scripts/capture_lttng_pre.sh  <command> [args...]
#   # ... run more commands against the same session ...
#   sh ./scripts/capture_lttng_post.sh
#
# Usage:
#   ./scripts/capture_lttng.sh <command> [args...]
#   # With context switches (requires root + lttng-modules-dkms):
#   sudo ./scripts/capture_lttng.sh <command> [args...]
#
# Environment variables:
#   TRIX_BACKEND      Forwarded to the command.  Defaults to "lttng".
#   LD_LIBRARY_PATH   Forwarded to the command if already set.
#   TRIX_LTTNG_OUT    Output text file path.
#                     Default: trix_lttng_YYYYMMDD_HHMMSS.txt

set -eu

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

if [ $# -eq 0 ]; then
    echo "Usage: $0 <command> [args...]" >&2
    echo "  e.g. $0 ./build/demo/trix_demo" >&2
    exit 1
fi

sh "${SCRIPT_DIR}/capture_lttng_pre.sh" "$@"
sh "${SCRIPT_DIR}/capture_lttng_post.sh"
