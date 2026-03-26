#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BIN_PATH="${1:-${REPO_ROOT}/build/elysium}"
BT_LOG_PATH="${2:-/tmp/elysium_crash_bt.log}"
RUN_SECONDS="${RUN_SECONDS:-20}"

if [[ ! -x "${BIN_PATH}" ]]; then
    echo "Smoke gdb failed: executable not found or not executable: ${BIN_PATH}" >&2
    exit 2
fi

if ! command -v gdb >/dev/null 2>&1; then
    echo "Smoke gdb failed: gdb is not installed" >&2
    exit 3
fi

rm -f "${BT_LOG_PATH}"

set +e
timeout "${RUN_SECONDS}"s gdb -q -batch \
    -ex "set pagination off" \
    -ex run \
    -ex "thread apply all bt full" \
    --args "${BIN_PATH}" >"${BT_LOG_PATH}" 2>&1
GDB_EXIT=$?
set -e

if grep -Eq "Program received signal (SIGABRT|SIGSEGV|SIGILL|SIGBUS|SIGFPE)" "${BT_LOG_PATH}"; then
    echo "Smoke gdb failed: crash signal detected during startup run" >&2
    echo "  backtrace log: ${BT_LOG_PATH}" >&2
    tail -n 120 "${BT_LOG_PATH}" >&2 || true
    exit 4
fi

# timeout(1) returns 124 when we intentionally stop a healthy long-running app.
if [[ "${GDB_EXIT}" != "0" && "${GDB_EXIT}" != "124" ]]; then
    echo "Smoke gdb failed: unexpected gdb/timeout exit code ${GDB_EXIT}" >&2
    echo "  backtrace log: ${BT_LOG_PATH}" >&2
    tail -n 120 "${BT_LOG_PATH}" >&2 || true
    exit 5
fi

echo "Smoke gdb passed"
echo "  backtrace log: ${BT_LOG_PATH}"
echo "  timeout/gdb exit code: ${GDB_EXIT}"
