#!/usr/bin/env sh
# capture_ftrace.sh — record any trix-instrumented binary with ftrace.
#
# This is the all-in-one wrapper: runs capture_ftrace_pre.sh (which starts
# tracing and the command), then immediately calls capture_ftrace_post.sh
# (which stops tracing and saves the output).
#
# For manual multi-run or delayed-save workflows, call the two scripts
# separately:
#
#   sudo sh ./scripts/capture_ftrace_pre.sh  <command> [args...]
#   # ... run more commands, or just wait ...
#   sudo sh ./scripts/capture_ftrace_post.sh
#
# Usage:
#   sudo sh ./scripts/capture_ftrace.sh <command> [args...]
#
# Environment variables:
#   TRIX_BACKEND      Forwarded to the command.  Defaults to "ftrace".
#   LD_LIBRARY_PATH   Forwarded to the command if already set.
#   TRIX_FTRACE_OUT   Output file path (passed to post script).
#                     Default: trix_ftrace_YYYYMMDD_HHMMSS.txt
#   TRIX_BUFFER_KB    Per-CPU ring-buffer size in KB.  Default: 8192.

set -eu

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

if [ $# -eq 0 ]; then
    echo "Usage: $0 <command> [args...]" >&2
    echo "  e.g. sudo sh $0 ./build/demo/trix_demo" >&2
    exit 1
fi

sh "${SCRIPT_DIR}/capture_ftrace_pre.sh" "$@"
sh "${SCRIPT_DIR}/capture_ftrace_post.sh"
