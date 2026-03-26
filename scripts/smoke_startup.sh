#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BIN_PATH="${1:-${REPO_ROOT}/build/elysium}"
LOG_PATH="${2:-/tmp/elysium_smoke.log}"
RUN_SECONDS="${RUN_SECONDS:-6}"
MAX_GLTF_LINES="${MAX_GLTF_LINES:-5}"
MAX_WALKMESH_LINES="${MAX_WALKMESH_LINES:-3}"

if [[ ! -x "${BIN_PATH}" ]]; then
    echo "Smoke startup failed: executable not found or not executable: ${BIN_PATH}" >&2
    exit 2
fi

rm -f "${LOG_PATH}"

"${BIN_PATH}" >"${LOG_PATH}" 2>&1 &
APP_PID=$!

sleep "${RUN_SECONDS}"

set +e
kill "${APP_PID}" >/dev/null 2>&1
wait "${APP_PID}"
APP_EXIT=$?
set -e

# Expected when we terminate a healthy run after RUN_SECONDS.
if [[ "${APP_EXIT}" != "0" && "${APP_EXIT}" != "143" ]]; then
    echo "Smoke startup failed: process exited with unexpected code ${APP_EXIT}" >&2
    tail -n 80 "${LOG_PATH}" >&2 || true
    exit 3
fi

GLTF_LOAD_LINES="$(grep -c "Loaded glTF" "${LOG_PATH}" || true)"
WALKMESH_LINES="$(grep -c "Walkmesh rebuilt" "${LOG_PATH}" || true)"
TINYGLTF_ERROR_LINES="$(grep -c "tinygltf error" "${LOG_PATH}" || true)"

if (( GLTF_LOAD_LINES > MAX_GLTF_LINES )); then
    echo "Smoke startup failed: excessive startup model loads (${GLTF_LOAD_LINES} > ${MAX_GLTF_LINES})" >&2
    tail -n 80 "${LOG_PATH}" >&2 || true
    exit 4
fi

if (( WALKMESH_LINES > MAX_WALKMESH_LINES )); then
    echo "Smoke startup failed: excessive walkmesh rebuilds (${WALKMESH_LINES} > ${MAX_WALKMESH_LINES})" >&2
    tail -n 80 "${LOG_PATH}" >&2 || true
    exit 5
fi

if (( TINYGLTF_ERROR_LINES > 0 )); then
    echo "Smoke startup failed: tinygltf errors observed during startup" >&2
    tail -n 80 "${LOG_PATH}" >&2 || true
    exit 6
fi

echo "Smoke startup passed"
echo "  log: ${LOG_PATH}"
echo "  Loaded glTF lines: ${GLTF_LOAD_LINES}"
echo "  Walkmesh rebuilt lines: ${WALKMESH_LINES}"
echo "  tinygltf error lines: ${TINYGLTF_ERROR_LINES}"
