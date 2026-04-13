#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
TEST_APPS_REL="components/mcp-c-sdk/test_apps"
PYTEST_FILE="${TEST_APPS_REL}/pytest_mcp.py"

TARGET="esp32c3"
PORT=""
SKIP_BUILD=0
SKIP_AUTOFLASH=0
SKIP_PORT_PROBE=0
UNITY_FILTER=""
PYTEST_K=""
LOG_DIR="${REPO_ROOT}/.cache/mcp-test-logs"
IDF_PATH_ARG=""

declare -a PYTEST_EXTRA_ARGS=()

usage() {
  cat <<'EOF'
Run MCP C SDK test_apps with optional build and serial-interactive pytest.

Usage:
  run_test_apps.sh --port <SERIAL_PORT> [options] [-- <extra pytest args>]

Options:
  --port <SERIAL_PORT>     Serial device for DUT, e.g. /dev/ttyUSB0
  --target <TARGET>        Chip target for build/pytest (default: esp32c3)
  --idf-path <PATH>        ESP-IDF root path. If omitted, use existing IDF_PATH env
  --skip-build             Skip build_apps.py step
  --skip-autoflash         Pass --skip-autoflash y to pytest
  --skip-port-probe        Skip preflight serial write probe
  --unity-filter <FILTER>  UNITY_TEST_FILTER value (group or case filters)
  --pytest-k <EXPR>        Pytest -k expression to narrow selected params/cases
  --log-dir <DIR>          Directory for test logs/junit (default: .cache/mcp-test-logs)
  -h, --help               Show this help

Examples:
  ./run_test_apps.sh --port /dev/ttyUSB0
  ./run_test_apps.sh --target esp32s3 --port /dev/ttyACM0 --skip-build
  ./run_test_apps.sh --port /dev/ttyUSB0 --unity-filter "[protocol]"
  ./run_test_apps.sh --port /dev/ttyUSB0 --pytest-k "default or api_test"
EOF
}

die() {
  echo "ERROR: $*" >&2
  exit 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --port)
      [[ $# -ge 2 ]] || die "--port requires a value"
      PORT="$2"
      shift 2
      ;;
    --target)
      [[ $# -ge 2 ]] || die "--target requires a value"
      TARGET="$2"
      shift 2
      ;;
    --idf-path)
      [[ $# -ge 2 ]] || die "--idf-path requires a value"
      IDF_PATH_ARG="$2"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    --skip-autoflash)
      SKIP_AUTOFLASH=1
      shift
      ;;
    --skip-port-probe)
      SKIP_PORT_PROBE=1
      shift
      ;;
    --unity-filter)
      [[ $# -ge 2 ]] || die "--unity-filter requires a value"
      UNITY_FILTER="$2"
      shift 2
      ;;
    --pytest-k)
      [[ $# -ge 2 ]] || die "--pytest-k requires a value"
      PYTEST_K="$2"
      shift 2
      ;;
    --log-dir)
      [[ $# -ge 2 ]] || die "--log-dir requires a value"
      LOG_DIR="$2"
      shift 2
      ;;
    --)
      shift
      PYTEST_EXTRA_ARGS=("$@")
      break
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      die "Unknown argument: $1"
      ;;
  esac
done

[[ -n "${PORT}" ]] || die "--port is required"
[[ -e "${PORT}" ]] || die "Serial port does not exist: ${PORT}"

if [[ -n "${IDF_PATH_ARG}" ]]; then
  export IDF_PATH="${IDF_PATH_ARG}"
fi
[[ -n "${IDF_PATH:-}" ]] || die "IDF_PATH is not set. Use --idf-path or source export.sh first."
[[ -f "${IDF_PATH}/export.sh" ]] || die "Invalid IDF_PATH, missing export.sh: ${IDF_PATH}"

# shellcheck source=/dev/null
. "${IDF_PATH}/export.sh" >/dev/null 2>&1

PYTHON_BIN="${PYTHON:-python3}"
command -v "${PYTHON_BIN}" >/dev/null 2>&1 || die "Python not found: ${PYTHON_BIN}"

if [[ "${SKIP_PORT_PROBE}" -eq 0 ]]; then
  echo "== Serial preflight probe =="
  # Unity menu tests require an interactive serial channel (read + write).
  if ! timeout 6s "${PYTHON_BIN}" -c "import serial; s=serial.Serial(port='${PORT}', baudrate=115200, timeout=1, write_timeout=2); s.write(b'\n'); s.flush(); s.close()" >/dev/null 2>&1; then
    die "Serial port is not writable for interactive tests: ${PORT}. Use a bidirectional console port or rerun with --skip-port-probe to bypass this check."
  fi
else
  echo "== Skip serial preflight probe =="
fi

mkdir -p "${LOG_DIR}"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
LOG_FILE="${LOG_DIR}/pytest_mcp_${TARGET}_${TIMESTAMP}.log"
JUNIT_FILE="${LOG_DIR}/pytest_mcp_${TARGET}_${TIMESTAMP}.xml"

echo "== MCP test_apps automation =="
echo "repo      : ${REPO_ROOT}"
echo "target    : ${TARGET}"
echo "port      : ${PORT}"
echo "log       : ${LOG_FILE}"
echo "junit     : ${JUNIT_FILE}"

cd "${REPO_ROOT}"

if [[ "${SKIP_BUILD}" -eq 0 ]]; then
  echo "== Build test apps (${TARGET}) =="
  if "${PYTHON_BIN}" -c "import idf_build_apps" >/dev/null 2>&1; then
    "${PYTHON_BIN}" tools/build_apps.py "${TEST_APPS_REL}" -t "${TARGET}"
  else
    cat >&2 <<EOF
ERROR: Missing Python module 'idf_build_apps', cannot run tools/build_apps.py.

Install it in the active ESP-IDF Python environment, for example:
  ${PYTHON_BIN} -m pip install idf-build-apps

Or rerun this script with --skip-build after you have prebuilt binaries.
EOF
    exit 1
  fi
else
  echo "== Skip build step =="
fi

if [[ -n "${UNITY_FILTER}" ]]; then
  export UNITY_TEST_FILTER="${UNITY_FILTER}"
  echo "UNITY_TEST_FILTER=${UNITY_TEST_FILTER}"
fi

declare -a PYTEST_CMD
PYTEST_CMD=(
  "${PYTHON_BIN}" -m pytest
  "${PYTEST_FILE}"
  --target "${TARGET}"
  --port "${PORT}"
  -v
  --junitxml "${JUNIT_FILE}"
)

if [[ "${SKIP_AUTOFLASH}" -eq 1 ]]; then
  PYTEST_CMD+=(--skip-autoflash y)
fi

if [[ -n "${PYTEST_K}" ]]; then
  PYTEST_CMD+=(-k "${PYTEST_K}")
fi

if [[ "${#PYTEST_EXTRA_ARGS[@]}" -gt 0 ]]; then
  PYTEST_CMD+=("${PYTEST_EXTRA_ARGS[@]}")
fi

echo "== Run pytest =="
printf 'CMD: '
printf '%q ' "${PYTEST_CMD[@]}"
printf '\n'

"${PYTEST_CMD[@]}" | tee "${LOG_FILE}"

echo "== Done =="
echo "log: ${LOG_FILE}"
echo "junit: ${JUNIT_FILE}"
