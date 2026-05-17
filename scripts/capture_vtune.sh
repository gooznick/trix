#!/usr/bin/env sh
# capture_vtune.sh — collect a trix run with VTune threading analysis (all-in-one).
#
# Wraps capture_vtune_pre.sh (collects ITT tasks + context switches) and
# capture_vtune_post.sh (prints how to open results).
#
# For manual workflows, call the two scripts separately:
#
#   sh ./scripts/capture_vtune_pre.sh  <command> [args...]
#   sh ./scripts/capture_vtune_post.sh
#
# Usage:
#   sh ./scripts/capture_vtune.sh <command> [args...]
#
# Environment variables:
#   TRIX_VTUNE_RESULT   Result directory path.
#                       Default: trix_vtune_YYYYMMDD_HHMMSS
#   VTUNE_BIN           Path to vtune binary (optional, auto-detected).
#   LD_LIBRARY_PATH     Forwarded to the command if already set.

set -eu

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

if [ $# -eq 0 ]; then
    echo "Usage: $0 <command> [args...]" >&2
    echo "  e.g. $0 ./build/demo/trix_demo" >&2
    exit 1
fi

sh "${SCRIPT_DIR}/capture_vtune_pre.sh" "$@"
sh "${SCRIPT_DIR}/capture_vtune_post.sh"
