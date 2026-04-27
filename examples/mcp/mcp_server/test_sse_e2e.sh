set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  bash test_sse_e2e.sh [HOST] [ENDPOINT] [--verbose] [--keep-log] [--strict] [--proto YYYY-MM-DD] [--auth-token TOKEN] [--auth-low-scope-token TOKEN] [--jwt-matrix]

Examples:
  bash test_sse_e2e.sh 192.168.1.100 mcp_server
  bash test_sse_e2e.sh 192.168.1.100 mcp_server --verbose --keep-log
  bash test_sse_e2e.sh 192.168.1.100 mcp_server --strict
  bash test_sse_e2e.sh --proto 2025-11-25
EOF
}

HOST="192.168.1.100"
ENDPOINT="mcp_server"
PROTO="${MCP_PROTO_VERSION:-2025-11-25}"
DEFAULT_SERVER_FALLBACK_PROTO="${MCP_DEFAULT_SERVER_PROTO_FALLBACK:-2025-03-26}"
VERBOSE=0
KEEP_LOG=0
STRICT=0
HEARTBEAT_WAIT_SECS=16
SESSION_EXPIRE_WAIT_SECS=0
AUTH_TOKEN="${MCP_AUTH_TOKEN:-}"
AUTH_LOW_SCOPE_TOKEN="${MCP_AUTH_LOW_SCOPE_TOKEN:-}"
JWT_MATRIX_ENABLE="${MCP_JWT_MATRIX_ENABLE:-0}"
JWT_GOOD_TOKEN_INPUT="${MCP_JWT_GOOD_TOKEN:-}"
JWT_BAD_SIG_TOKEN_INPUT="${MCP_JWT_BAD_SIG_TOKEN:-}"
JWT_BAD_KID_TOKEN_INPUT="${MCP_JWT_BAD_KID_TOKEN:-}"
AUTH_TOKEN_ARG_SET=0

POSITIONALS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --verbose|-v)
      VERBOSE=1
      shift
      ;;
    --keep-log|-k)
      KEEP_LOG=1
      shift
      ;;
    --strict|-s)
      STRICT=1
      shift
      ;;
    --proto)
      [[ $# -ge 2 ]] || { echo "Missing value for --proto"; exit 2; }
      PROTO="$2"
      shift 2
      ;;
    --auth-token)
      [[ $# -ge 2 ]] || { echo "Missing value for --auth-token"; exit 2; }
      AUTH_TOKEN="$2"
      AUTH_TOKEN_ARG_SET=1
      shift 2
      ;;
    --auth-low-scope-token)
      [[ $# -ge 2 ]] || { echo "Missing value for --auth-low-scope-token"; exit 2; }
      AUTH_LOW_SCOPE_TOKEN="$2"
      shift 2
      ;;
    --session-expire-wait)
      [[ $# -ge 2 ]] || { echo "Missing value for --session-expire-wait"; exit 2; }
      SESSION_EXPIRE_WAIT_SECS="$2"
      shift 2
      ;;
    --jwt-matrix)
      JWT_MATRIX_ENABLE=1
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      POSITIONALS+=("$1")
      shift
      ;;
  esac
done

if [[ ${#POSITIONALS[@]} -ge 1 ]]; then
  HOST="${POSITIONALS[0]}"
fi
if [[ ${#POSITIONALS[@]} -ge 2 ]]; then
  ENDPOINT="${POSITIONALS[1]}"
fi
if [[ ${#POSITIONALS[@]} -gt 2 ]]; then
  echo "Too many positional arguments"
  usage
  exit 2
fi

if [[ "${AUTH_TOKEN_ARG_SET}" -eq 1 && -z "${AUTH_TOKEN}" ]]; then
  echo "FAIL: --auth-token was provided but is empty." >&2
  echo "HINT: check token file content, e.g. wc -c /tmp/mcp-es256/token_good.txt" >&2
  exit 2
fi

BASE_URL="http://${HOST}/${ENDPOINT}"
PROTECTED_RESOURCE_METADATA_URL="http://${HOST}/.well-known/oauth-protected-resource"
AUTH_SERVER_METADATA_URL="http://${HOST}/.well-known/oauth-authorization-server"
SSE_LOG1="${TMPDIR:-/tmp}/mcp_sse_e2e_$$.stream1.log"
SSE_LOG2="${TMPDIR:-/tmp}/mcp_sse_e2e_$$.stream2.log"
SSE_HDR1="${TMPDIR:-/tmp}/mcp_sse_e2e_$$.stream1.hdr"
SSE_HDR2="${TMPDIR:-/tmp}/mcp_sse_e2e_$$.stream2.hdr"
SSE_ERR1="${TMPDIR:-/tmp}/mcp_sse_e2e_$$.stream1.err"
SSE_ERR2="${TMPDIR:-/tmp}/mcp_sse_e2e_$$.stream2.err"
REQ_ID=1

log() { echo "$*" >&2; }
dbg() {
  if [[ "${VERBOSE}" -eq 1 ]]; then
    echo "[verbose] $*" >&2
  fi
  return 0
}

text_contains() {
  local text="$1"
  local needle="$2"
  if [[ "${needle}" == *$'\n'* ]]; then
    printf '%s' "${text}" | grep -Fq -- "${needle}"
  elif command -v rg >/dev/null 2>&1; then
    printf '%s' "${text}" | rg -Fq -- "${needle}"
  else
    printf '%s' "${text}" | grep -Fq -- "${needle}"
  fi
}

assert_contains() {
  local text="$1"
  local needle="$2"
  local msg="$3"
  if ! text_contains "${text}" "${needle}"; then
    echo "FAIL: ${msg}" >&2
    echo "Expected to find: ${needle}" >&2
    echo "Response was: ${text}" >&2
    exit 1
  fi
}

assert_not_contains() {
  local text="$1"
  local needle="$2"
  local msg="$3"
  if text_contains "${text}" "${needle}"; then
    echo "FAIL: ${msg}" >&2
    echo "Did not expect to find: ${needle}" >&2
    echo "Response was: ${text}" >&2
    exit 1
  fi
}

b64url_json() {
  local json="$1"
  printf '%s' "${json}" | base64 | tr -d '\n=' | tr '+/' '-_'
}

make_fake_jwt() {
  local payload_json="$1"
  local include_kid="${2:-1}"
  local header='{"alg":"HS256","typ":"JWT","kid":"k1"}'
  if [[ "${include_kid}" -eq 0 ]]; then
    header='{"alg":"HS256","typ":"JWT"}'
  fi
  printf '%s.%s.%s' "$(b64url_json "${header}")" "$(b64url_json "${payload_json}")" "sig"
}

http_auth_ping_with_token() {
  local token="$1"
  local id="${2:-jwt-probe}"
  curl -sS -i -X POST "${BASE_URL}" \
    -H "Content-Type: application/json" \
    -H "Accept: application/json, text/event-stream" \
    -H "MCP-Protocol-Version: ${PROTO}" \
    -H "Authorization: Bearer ${token}" \
    -d "{\"jsonrpc\":\"2.0\",\"id\":\"${id}\",\"method\":\"ping\",\"params\":{}}" || true
}

rpc_raw_as() {
  local label="$1"
  local payload="$2"
  local auth_token="${3:-${AUTH_TOKEN}}"
  log "${label}"
  local resp
  local session_header=()
  local auth_header=()
  if [[ -n "${ACTIVE_SESSION_ID:-}" ]]; then
    session_header=(-H "MCP-Session-Id: ${ACTIVE_SESSION_ID}")
  fi
  if [[ -n "${auth_token}" ]]; then
    auth_header=(-H "Authorization: Bearer ${auth_token}")
  fi
  resp="$(curl -sS -X POST "${BASE_URL}" \
    -H "Content-Type: application/json" \
    -H "Accept: application/json, text/event-stream" \
    -H "MCP-Protocol-Version: ${PROTO}" \
    "${auth_header[@]}" \
    "${session_header[@]}" \
    -d "${payload}")"
  dbg "response: ${resp}"
  echo "${resp}"
}

rpc_raw() {
  local label="$1"
  local payload="$2"
  rpc_raw_as "${label}" "${payload}" "${AUTH_TOKEN}"
}

rpc_ok() {
  local label="$1"
  local payload="$2"
  local resp
  resp="$(rpc_raw "${label}" "${payload}")"
  if [[ "${resp}" == *"\"error\""* ]]; then
    echo "FAIL: request returned JSON-RPC error: ${label}" >&2
    echo "${resp}" >&2
    exit 1
  fi
  echo "${resp}"
}

delete_session() {
  local session_id="$1"
  local auth_token="${2:-${AUTH_TOKEN}}"
  local auth_header=()
  if [[ -n "${auth_token}" ]]; then
    auth_header=(-H "Authorization: Bearer ${auth_token}")
  fi
  curl -sS -i -X DELETE "${BASE_URL}" \
    -H "MCP-Protocol-Version: ${PROTO}" \
    -H "MCP-Session-Id: ${session_id}" \
    "${auth_header[@]}" || true
}

next_id() {
  NEXT_ID="${REQ_ID}"
  REQ_ID=$((REQ_ID + 1))
}

cleanup() {
  if [[ -n "${SSE1_PID:-}" ]]; then
    kill "${SSE1_PID}" >/dev/null 2>&1 || true
  fi
  if [[ -n "${SSE2_PID:-}" ]]; then
    kill "${SSE2_PID}" >/dev/null 2>&1 || true
  fi
  if [[ "${KEEP_LOG}" -eq 0 ]]; then
    rm -f "${SSE_LOG1}" "${SSE_LOG2}" "${SSE_HDR1}" "${SSE_HDR2}" "${SSE_ERR1}" "${SSE_ERR2}" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

wait_sse_handshake() {
  local hdr_file="$1"
  local log_file="$2"
  local pid="$3"
  local wait_loops=25
  local i
  for ((i=0; i<wait_loops; i++)); do
    if [[ -s "${hdr_file}" || -s "${log_file}" ]]; then
      break
    fi
    if ! kill -0 "${pid}" >/dev/null 2>&1; then
      break
    fi
    sleep 0.2
  done
}

AUTH_CURL_HEADER=()
if [[ -n "${AUTH_TOKEN}" ]]; then
  AUTH_CURL_HEADER=(-H "Authorization: Bearer ${AUTH_TOKEN}")
fi

log "[1/36] Open SSE stream #1: ${BASE_URL}"
curl -sS -N -D "${SSE_HDR1}" \
  -H "Accept: text/event-stream" \
  -H "MCP-Protocol-Version: ${PROTO}" \
  "${AUTH_CURL_HEADER[@]}" \
  "${BASE_URL}" > "${SSE_LOG1}" 2>"${SSE_ERR1}" &
SSE1_PID=$!
wait_sse_handshake "${SSE_HDR1}" "${SSE_LOG1}" "${SSE1_PID}"
ACTIVE_SESSION_ID="$(sed -nE 's/^MCP-Session-Id:[[:space:]]*([^[:space:]\r]+).*/\1/ip' "${SSE_HDR1}" | sed -n '1p' | tr -d '\r')"
if [[ -z "${ACTIVE_SESSION_ID}" ]]; then
  sse_hdr_dump="$(cat "${SSE_HDR1}" 2>/dev/null || true)"
  if text_contains "${sse_hdr_dump}" "401 Unauthorized" || text_contains "${sse_hdr_dump}" "403 Forbidden"; then
    if [[ -z "${AUTH_TOKEN}" ]]; then
      echo "FAIL: SSE #1 auth required but no token provided; use --auth-token <TOKEN> or MCP_AUTH_TOKEN" >&2
      echo "SSE #1 response headers:" >&2
      echo "${sse_hdr_dump}" >&2
      exit 1
    fi
    log "SSE #1 retry with Authorization token"
    kill "${SSE1_PID}" >/dev/null 2>&1 || true
    unset SSE1_PID
    curl -sS -N -D "${SSE_HDR1}" \
      -H "Accept: text/event-stream" \
      -H "MCP-Protocol-Version: ${PROTO}" \
      -H "Authorization: Bearer ${AUTH_TOKEN}" \
      "${BASE_URL}" > "${SSE_LOG1}" 2>"${SSE_ERR1}" &
    SSE1_PID=$!
    wait_sse_handshake "${SSE_HDR1}" "${SSE_LOG1}" "${SSE1_PID}"
    ACTIVE_SESSION_ID="$(sed -nE 's/^MCP-Session-Id:[[:space:]]*([^[:space:]\r]+).*/\1/ip' "${SSE_HDR1}" | sed -n '1p' | tr -d '\r')"
  fi
  if [[ -z "${ACTIVE_SESSION_ID}" ]]; then
    echo "FAIL: failed to parse session id from SSE #1 headers" >&2
    echo "SSE #1 response headers:" >&2
    cat "${SSE_HDR1}" >&2 || true
    echo "SSE #1 response body preview:" >&2
    sed -n '1,20p' "${SSE_LOG1}" >&2 || true
    if [[ -s "${SSE_ERR1}" ]]; then
      echo "SSE #1 curl stderr:" >&2
      sed -n '1,20p' "${SSE_ERR1}" >&2 || true
    fi
    if ! kill -0 "${SSE1_PID}" >/dev/null 2>&1; then
      echo "HINT: SSE #1 curl exited early (connection issue or immediate server close)." >&2
    fi
    sse_hdr_dump="$(cat "${SSE_HDR1}" 2>/dev/null || true)"
    sse_body_preview="$(sed -n '1,20p' "${SSE_LOG1}" 2>/dev/null || true)"
    if { text_contains "${sse_hdr_dump}" "401 Unauthorized" || text_contains "${sse_hdr_dump}" "403 Forbidden"; } && \
       { text_contains "${sse_hdr_dump}" "invalid_token" || text_contains "${sse_body_preview}" "invalid_token" || text_contains "${sse_body_preview}" "jwt_"; }; then
      echo "HINT: current --auth-token looks like a negative-case token (e.g. bad_sig/bad_kid)." >&2
      echo "HINT: use a GOOD token for main flow, and pass bad tokens via --jwt-matrix." >&2
      echo "HINT: example:" >&2
      echo "  MCP_JWT_BAD_SIG_TOKEN=\"\$(cat /tmp/mcp-es256/token_bad_sig.txt)\" \\" >&2
      echo "  MCP_JWT_BAD_KID_TOKEN=\"\$(cat /tmp/mcp-es256/token_bad_kid.txt)\" \\" >&2
      echo "  bash test_sse_e2e.sh ${HOST} ${ENDPOINT} --strict --verbose --keep-log --jwt-matrix --auth-token \"\$(cat /tmp/mcp-es256/token_good.txt)\"" >&2
    fi
    exit 1
  fi
fi

oauth_resource_metadata="$(curl -sS -H "Accept: application/json" "${PROTECTED_RESOURCE_METADATA_URL}" || true)"
oauth_auth_server_metadata="$(curl -sS -H "Accept: application/json" "${AUTH_SERVER_METADATA_URL}" || true)"
META_ISSUER="$(printf '%s\n' "${oauth_auth_server_metadata}" | sed -nE 's/.*"issuer":"([^"]+)".*/\1/p' | sed -n '1p')"
META_RESOURCE="$(printf '%s\n' "${oauth_resource_metadata}" | sed -nE 's/.*"resource":"([^"]+)".*/\1/p' | sed -n '1p')"
if [[ -z "${META_ISSUER}" ]]; then META_ISSUER="https://example.local"; fi
if [[ -z "${META_RESOURCE}" ]]; then META_RESOURCE="mcp://esp32"; fi

auth_probe_no_token="$(curl -sS -i -X POST "${BASE_URL}" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  -H "MCP-Protocol-Version: ${PROTO}" \
  -d '{"jsonrpc":"2.0","id":"auth-none","method":"ping","params":{}}' || true)"
if text_contains "${auth_probe_no_token}" "401 Unauthorized"; then
  AUTH_REQUIRED=1
else
  AUTH_REQUIRED=0
fi

if [[ "${AUTH_REQUIRED}" -eq 1 ]]; then
  auth_probe_bad_token="$(curl -sS -i -X POST "${BASE_URL}" \
    -H "Content-Type: application/json" \
    -H "Accept: application/json, text/event-stream" \
    -H "MCP-Protocol-Version: ${PROTO}" \
    -H "Authorization: Bearer definitely-wrong-token" \
    -d '{"jsonrpc":"2.0","id":"auth-bad","method":"ping","params":{}}' || true)"
  if [[ -n "${AUTH_TOKEN}" ]]; then
    auth_probe_good_token="$(curl -sS -i -X POST "${BASE_URL}" \
      -H "Content-Type: application/json" \
      -H "Accept: application/json, text/event-stream" \
      -H "MCP-Protocol-Version: ${PROTO}" \
      -H "Authorization: Bearer ${AUTH_TOKEN}" \
      -d "{\"jsonrpc\":\"2.0\",\"id\":\"auth-good\",\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"${PROTO}\",\"clientInfo\":{\"name\":\"auth-probe\",\"version\":\"1.0.0\"},\"capabilities\":{}}}" || true)"
  fi
  if [[ -n "${AUTH_LOW_SCOPE_TOKEN}" ]]; then
    auth_probe_low_scope="$(curl -sS -i -X POST "${BASE_URL}" \
      -H "Content-Type: application/json" \
      -H "Accept: application/json, text/event-stream" \
      -H "MCP-Protocol-Version: ${PROTO}" \
      -H "Authorization: Bearer ${AUTH_LOW_SCOPE_TOKEN}" \
      -d '{"jsonrpc":"2.0","id":"auth-scope","method":"ping","params":{}}' || true)"
    if text_contains "${auth_probe_low_scope}" "invalid_token" && ! text_contains "${auth_probe_low_scope}" "scope_insufficient"; then
      echo "HINT: low-scope token mismatch: server returned invalid_token instead of scope_insufficient." >&2
      echo "HINT: 低权限 token 与固件配置不一致，请检查 CONFIG_MCP_HTTP_AUTH_LOW_SCOPE_TOKEN 与 --auth-low-scope-token 是否完全一致。" >&2
    fi
  fi
fi
JWT_VERIFY_ACTIVE=0
jwt_probe_invalid_format=""
jwt_probe_expired=""
jwt_probe_bad_iss=""
jwt_probe_bad_aud=""
jwt_probe_missing_kid=""
jwt_probe_scope_insufficient=""
jwt_probe_good=""
jwt_probe_bad_sig=""
jwt_probe_bad_kid=""
if [[ "${AUTH_REQUIRED}" -eq 1 && "${JWT_MATRIX_ENABLE}" -eq 1 ]]; then
  now_ts="$(date +%s)"
  jwt_good_payload="{\"iss\":\"${META_ISSUER}\",\"aud\":\"${META_RESOURCE}\",\"sub\":\"jwt-e2e\",\"scope\":\"mcp:full\",\"exp\":$((now_ts + 3600)),\"nbf\":$((now_ts - 10)),\"iat\":$((now_ts - 10))}"
  jwt_expired_payload="{\"iss\":\"${META_ISSUER}\",\"aud\":\"${META_RESOURCE}\",\"sub\":\"jwt-expired\",\"scope\":\"mcp:full\",\"exp\":$((now_ts - 3600)),\"nbf\":$((now_ts - 7200)),\"iat\":$((now_ts - 7200))}"
  jwt_bad_iss_payload="{\"iss\":\"${META_ISSUER}/bad\",\"aud\":\"${META_RESOURCE}\",\"sub\":\"jwt-bad-iss\",\"scope\":\"mcp:full\",\"exp\":$((now_ts + 3600)),\"nbf\":$((now_ts - 10)),\"iat\":$((now_ts - 10))}"
  jwt_bad_aud_payload="{\"iss\":\"${META_ISSUER}\",\"aud\":\"mcp://wrong\",\"sub\":\"jwt-bad-aud\",\"scope\":\"mcp:full\",\"exp\":$((now_ts + 3600)),\"nbf\":$((now_ts - 10)),\"iat\":$((now_ts - 10))}"
  jwt_scope_payload="{\"iss\":\"${META_ISSUER}\",\"aud\":\"${META_RESOURCE}\",\"sub\":\"jwt-scope\",\"scope\":\"mcp:read\",\"exp\":$((now_ts + 3600)),\"nbf\":$((now_ts - 10)),\"iat\":$((now_ts - 10))}"
  JWT_GOOD_TOKEN="$(make_fake_jwt "${jwt_good_payload}" 1)"
  JWT_EXPIRED_TOKEN="$(make_fake_jwt "${jwt_expired_payload}" 1)"
  JWT_BAD_ISS_TOKEN="$(make_fake_jwt "${jwt_bad_iss_payload}" 1)"
  JWT_BAD_AUD_TOKEN="$(make_fake_jwt "${jwt_bad_aud_payload}" 1)"
  JWT_MISSING_KID_TOKEN="$(make_fake_jwt "${jwt_good_payload}" 0)"
  JWT_SCOPE_TOKEN="$(make_fake_jwt "${jwt_scope_payload}" 1)"
  if [[ -z "${AUTH_TOKEN}" ]]; then
    AUTH_TOKEN="${JWT_GOOD_TOKEN}"
    AUTH_CURL_HEADER=(-H "Authorization: Bearer ${AUTH_TOKEN}")
  fi

  jwt_probe_invalid_format="$(http_auth_ping_with_token "bad-token" "jwt-bad-format")"
  if text_contains "${jwt_probe_invalid_format}" "jwt_"; then
    JWT_VERIFY_ACTIVE=1
  fi
  if [[ "${JWT_VERIFY_ACTIVE}" -eq 1 ]]; then
    jwt_probe_expired="$(http_auth_ping_with_token "${JWT_EXPIRED_TOKEN}" "jwt-expired")"
    jwt_probe_bad_iss="$(http_auth_ping_with_token "${JWT_BAD_ISS_TOKEN}" "jwt-bad-iss")"
    jwt_probe_bad_aud="$(http_auth_ping_with_token "${JWT_BAD_AUD_TOKEN}" "jwt-bad-aud")"
    jwt_probe_missing_kid="$(http_auth_ping_with_token "${JWT_MISSING_KID_TOKEN}" "jwt-missing-kid")"
    jwt_probe_scope_insufficient="$(http_auth_ping_with_token "${JWT_SCOPE_TOKEN}" "jwt-scope")"
    jwt_probe_good="$(http_auth_ping_with_token "${JWT_GOOD_TOKEN}" "jwt-good")"
    if [[ -n "${JWT_GOOD_TOKEN_INPUT}" ]]; then
      jwt_probe_good="$(http_auth_ping_with_token "${JWT_GOOD_TOKEN_INPUT}" "jwt-good-provided")"
    fi
    if [[ -n "${JWT_BAD_SIG_TOKEN_INPUT}" ]]; then
      jwt_probe_bad_sig="$(http_auth_ping_with_token "${JWT_BAD_SIG_TOKEN_INPUT}" "jwt-bad-sig-provided")"
    fi
    if [[ -n "${JWT_BAD_KID_TOKEN_INPUT}" ]]; then
      jwt_probe_bad_kid="$(http_auth_ping_with_token "${JWT_BAD_KID_TOKEN_INPUT}" "jwt-bad-kid-provided")"
    fi
  fi
fi
if [[ "${AUTH_REQUIRED}" -eq 1 && -z "${AUTH_TOKEN}" ]]; then
  echo "FAIL: server requires auth; provide --auth-token <TOKEN> (or MCP_AUTH_TOKEN)" >&2
  exit 1
fi

next_id
id="${NEXT_ID}"
rpc_ok "[2/36] initialize (numeric id)" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"${PROTO}\",\"clientInfo\":{\"name\":\"sse-e2e\",\"version\":\"1.0.0\"},\"capabilities\":{\"roots\":{},\"sampling\":{},\"elicitation\":{}}}}" >/dev/null

rpc_ok "[3/36] notifications/initialized" \
  "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\",\"params\":{}}" >/dev/null

resp_ping="$(rpc_ok "[4/36] ping (string id)" \
  "{\"jsonrpc\":\"2.0\",\"id\":\"ping-1\",\"method\":\"ping\",\"params\":{}}")"
resp_dup_id_first="$(rpc_ok "       duplicate-id probe #1" \
  "{\"jsonrpc\":\"2.0\",\"id\":\"dup-req\",\"method\":\"ping\",\"params\":{}}")"
resp_dup_id_second="$(rpc_raw "       duplicate-id probe #2 (same session/id)" \
  "{\"jsonrpc\":\"2.0\",\"id\":\"dup-req\",\"method\":\"ping\",\"params\":{}}")"

resp_tools_list="$(rpc_ok "[5/36] tools/list (string id)" \
  "{\"jsonrpc\":\"2.0\",\"id\":\"tools-list-1\",\"method\":\"tools/list\",\"params\":{}}")"

next_id
id="${NEXT_ID}"
resp_tool_call_status="$(rpc_ok "[6/36] tools/call self.get_device_status" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tools/call\",\"params\":{\"name\":\"self.get_device_status\",\"arguments\":{}}}")"

next_id
id="${NEXT_ID}"
resp_set_volume="$(rpc_ok "[7/36] tools/call self.audio_speaker.set_volume(66)" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tools/call\",\"params\":{\"name\":\"self.audio_speaker.set_volume\",\"arguments\":{\"volume\":66}}}")"

next_id
id="${NEXT_ID}"
resp_set_brightness="$(rpc_ok "[8/36] tools/call self.screen.set_brightness(42)" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tools/call\",\"params\":{\"name\":\"self.screen.set_brightness\",\"arguments\":{\"brightness\":42}}}")"

next_id
id="${NEXT_ID}"
resp_set_theme="$(rpc_ok "[9/36] tools/call self.screen.set_theme(dark)" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tools/call\",\"params\":{\"name\":\"self.screen.set_theme\",\"arguments\":{\"theme\":\"dark\"}}}")"

next_id
id="${NEXT_ID}"
resp_set_rgb="$(rpc_ok "[10/36] tools/call self.screen.set_rgb({11,22,33})" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tools/call\",\"params\":{\"name\":\"self.screen.set_rgb\",\"arguments\":{\"RGB\":{\"red\":11,\"green\":22,\"blue\":33}}}}")"

next_id
id="${NEXT_ID}"
resp_set_hsv="$(rpc_ok "[11/36] tools/call self.screen.set_hsv([180,70,55])" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tools/call\",\"params\":{\"name\":\"self.screen.set_hsv\",\"arguments\":{\"HSV\":[180,70,55]}}}")"

next_id
id="${NEXT_ID}"
resp_resources_list="$(rpc_ok "[12/36] resources/list" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"resources/list\",\"params\":{}}")"
next_id
id="${NEXT_ID}"
resp_resources_list_page1="$(rpc_ok "       resources/list limit=1" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"resources/list\",\"params\":{\"limit\":1}}")"
resources_next_cursor="$(printf '%s\n' "${resp_resources_list_page1}" | sed -nE 's/.*"nextCursor":"([^"]+)".*/\1/p' | sed -n '1p')"
resp_resources_list_page2=""
if [[ -n "${resources_next_cursor}" ]]; then
  next_id
  id="${NEXT_ID}"
  resp_resources_list_page2="$(rpc_ok "       resources/list cursor=${resources_next_cursor}" \
    "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"resources/list\",\"params\":{\"cursor\":\"${resources_next_cursor}\",\"limit\":1}}")"
fi

next_id
id="${NEXT_ID}"
resp_resource_read="$(rpc_ok "[13/36] resources/read device://status" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"resources/read\",\"params\":{\"uri\":\"device://status\"}}")"

next_id
id="${NEXT_ID}"
resp_templates_list="$(rpc_ok "[14/36] resources/templates/list" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"resources/templates/list\",\"params\":{}}")"

next_id
id="${NEXT_ID}"
resp_prompts_list="$(rpc_ok "[15/36] prompts/list" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"prompts/list\",\"params\":{}}")"

next_id
id="${NEXT_ID}"
resp_prompt_get="$(rpc_ok "[16/36] prompts/get status.summary" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"prompts/get\",\"params\":{\"name\":\"status.summary\",\"arguments\":{\"device\":\"screen\"}}}")"

next_id
id="${NEXT_ID}"
resp_completion="$(rpc_ok "[17/36] completion/complete" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"completion/complete\",\"params\":{\"ref\":{\"type\":\"ref/prompt\",\"name\":\"status.summary\"},\"argument\":{\"name\":\"device\",\"value\":\"s\"}}}")"

resp_completion_bad="$(rpc_raw "       completion/complete invalid params (error.data.expected)" \
  "{\"jsonrpc\":\"2.0\",\"id\":\"completion-bad\",\"method\":\"completion/complete\",\"params\":{\"ref\":{\"type\":\"ref/prompt\",\"name\":\"status.summary\"}}}")"

next_id
id="${NEXT_ID}"
resp_subscribe="$(rpc_ok "[18/36] resources/subscribe device://status" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"resources/subscribe\",\"params\":{\"uri\":\"device://status\"}}")"

next_id
id="${NEXT_ID}"
task_resp="$(rpc_ok "[19/36] tools/call task={} (self.get_device_status)" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tools/call\",\"params\":{\"name\":\"self.get_device_status\",\"arguments\":{},\"task\":{}}}")"
task_id="$(printf '%s\n' "${task_resp}" | sed -nE 's/.*"taskId":"([^"]+)".*/\1/p' | sed -n '1p')"
if [[ -z "${task_id}" ]]; then
  echo "FAIL: task-mode tools/call did not return taskId" >&2
  exit 1
fi

next_id
id="${NEXT_ID}"
resp_tasks_get="$(rpc_ok "[20/36] tasks/get ${task_id}" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tasks/get\",\"params\":{\"taskId\":\"${task_id}\"}}")"

next_id
id="${NEXT_ID}"
resp_tasks_result="$(rpc_ok "[21/36] tasks/result ${task_id}" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tasks/result\",\"params\":{\"taskId\":\"${task_id}\"}}")"

next_id
id="${NEXT_ID}"
task_done_seed="$(rpc_ok "[22/36] tools/call task={} (seed complete-before-cancel case)" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tools/call\",\"params\":{\"name\":\"self.get_device_status\",\"arguments\":{},\"task\":{}}}")"
task_done_id="$(printf '%s\n' "${task_done_seed}" | sed -nE 's/.*"taskId":"([^"]+)".*/\1/p' | sed -n '1p')"
if [[ -z "${task_done_id}" ]]; then
  echo "FAIL: complete-before-cancel case tools/call did not return taskId" >&2
  exit 1
fi

next_id
id="${NEXT_ID}"
resp_tasks_result_done="$(rpc_ok "       tasks/result ${task_done_id} (complete before cancel)" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tasks/result\",\"params\":{\"taskId\":\"${task_done_id}\"}}")"

next_id
id="${NEXT_ID}"
resp_tasks_cancel_after_done="$(rpc_raw "       tasks/cancel ${task_done_id} (after completed)" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tasks/cancel\",\"params\":{\"taskId\":\"${task_done_id}\"}}")"

next_id
id="${NEXT_ID}"
task_cancel_seed="$(rpc_ok "[23/36] tools/call task={} (seed cancel race case)" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tools/call\",\"params\":{\"name\":\"self.get_device_status\",\"arguments\":{\"__simulateWorkMs\":2500},\"task\":{}}}")"
task_cancel_id="$(printf '%s\n' "${task_cancel_seed}" | sed -nE 's/.*"taskId":"([^"]+)".*/\1/p' | sed -n '1p')"
if [[ -z "${task_cancel_id}" ]]; then
  echo "FAIL: cancel case tools/call did not return taskId" >&2
  exit 1
fi

next_id
id="${NEXT_ID}"
resp_tasks_cancel="$(rpc_ok "[24/36] tasks/cancel ${task_cancel_id}" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tasks/cancel\",\"params\":{\"taskId\":\"${task_cancel_id}\"}}")"

next_id
id="${NEXT_ID}"
resp_tasks_cancel_repeat="$(rpc_raw "       tasks/cancel ${task_cancel_id} (repeat)" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tasks/cancel\",\"params\":{\"taskId\":\"${task_cancel_id}\"}}")"

next_id
id="${NEXT_ID}"
resp_tasks_get_after_cancel="$(rpc_ok "[25/36] tasks/get ${task_cancel_id} (after cancel)" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tasks/get\",\"params\":{\"taskId\":\"${task_cancel_id}\"}}")"

next_id
id="${NEXT_ID}"
resp_tasks_result_after_cancel="$(rpc_raw "       tasks/result ${task_cancel_id} (after cancel)" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tasks/result\",\"params\":{\"taskId\":\"${task_cancel_id}\"}}")"

resp_batch="$(rpc_raw "[26/36] batch: ping + resources/read + notification + invalid method" \
  "[{\"jsonrpc\":\"2.0\",\"id\":\"b1\",\"method\":\"ping\",\"params\":{}},{\"jsonrpc\":\"2.0\",\"id\":\"b2\",\"method\":\"resources/read\",\"params\":{\"uri\":\"device://status\"}},{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\",\"params\":{}},{\"jsonrpc\":\"2.0\",\"id\":\"b3\",\"method\":\"no/suchMethod\",\"params\":{}}]")"

resp_err_method="$(rpc_raw "[27/36] invalid method (error.data.method)" \
  "{\"jsonrpc\":\"2.0\",\"id\":\"err-method\",\"method\":\"no/suchMethod\",\"params\":{}}")"

resp_err_resource_not_found="$(rpc_raw "[28/36] resources/read unknown uri (error.data.uri)" \
  "{\"jsonrpc\":\"2.0\",\"id\":\"err-uri\",\"method\":\"resources/read\",\"params\":{\"uri\":\"device://not-found\"}}")"

resp_err_invalid_params="$(rpc_raw "[29/36] resources/read missing uri (error.data.expected)" \
  "{\"jsonrpc\":\"2.0\",\"id\":\"err-params\",\"method\":\"resources/read\",\"params\":{}}")"

resp_err_invalid_id="$(rpc_raw "[30/36] invalid request id=null (error.data.method)" \
  "{\"jsonrpc\":\"2.0\",\"id\":null,\"method\":\"tools/list\",\"params\":{}}")"

next_id
id="${NEXT_ID}"
rpc_ok "[31/36] Trigger resources notification #1 (touch_status)" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tools/call\",\"params\":{\"name\":\"self.resource.touch_status\",\"arguments\":{}}}" >/dev/null

log "[32/36] Wait ${HEARTBEAT_WAIT_SECS}s for SSE heartbeat comment"
sleep "${HEARTBEAT_WAIT_SECS}"

kill "${SSE1_PID}" >/dev/null 2>&1 || true
unset SSE1_PID

SESSION_ID_1="$(sed -nE 's/^MCP-Session-Id:[[:space:]]*([^[:space:]\r]+).*/\1/ip' "${SSE_HDR1}" | sed -n '1p' | tr -d '\r')"
mapfile -t SSE_EVENT_IDS < <(sed -nE 's/^id:[[:space:]]*([^[:space:]\r]+).*/\1/p' "${SSE_LOG1}" | tr -d '\r')
if (( ${#SSE_EVENT_IDS[@]} > 0 )); then
  LAST_EVENT_ID="${SSE_EVENT_IDS[$((${#SSE_EVENT_IDS[@]} - 1))]}"
else
  LAST_EVENT_ID="0"
fi
if (( ${#SSE_EVENT_IDS[@]} > 1 )); then
  REPLAY_FROM_EVENT_ID="${SSE_EVENT_IDS[$((${#SSE_EVENT_IDS[@]} - 2))]}"
else
  REPLAY_FROM_EVENT_ID="${LAST_EVENT_ID}"
fi

log "[33/36] Re-open SSE stream #2 with MCP-Session-Id + Last-Event-ID replay window"
curl -sS -N -D "${SSE_HDR2}" \
  -H "Accept: text/event-stream" \
  -H "MCP-Protocol-Version: ${PROTO}" \
  -H "MCP-Session-Id: ${SESSION_ID_1}" \
  -H "Last-Event-ID: ${REPLAY_FROM_EVENT_ID}" \
  "${AUTH_CURL_HEADER[@]}" \
  "${BASE_URL}" > "${SSE_LOG2}" &
SSE2_PID=$!
sleep 1

next_id
id="${NEXT_ID}"
rpc_ok "[34/36] Trigger resources notification #2 after reconnect" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tools/call\",\"params\":{\"name\":\"self.resource.touch_status\",\"arguments\":{}}}" >/dev/null

next_id
id="${NEXT_ID}"
resp_unsubscribe="$(rpc_ok "[35/36] resources/unsubscribe device://status" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"resources/unsubscribe\",\"params\":{\"uri\":\"device://status\"}}")"

next_id
id="${NEXT_ID}"
resp_cancel_missing="$(rpc_raw "[36/36] tasks/cancel unknown task (error case)" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tasks/cancel\",\"params\":{\"taskId\":\"ffffffffffffffffffffffffffffffff\"}}")"

next_id
id="${NEXT_ID}"
resp_roots_req="$(rpc_ok "       tools/call self.server.roots_list" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tools/call\",\"params\":{\"name\":\"self.server.roots_list\",\"arguments\":{}}}")"
roots_req_id="$(printf '%s\n' "${resp_roots_req}" | sed -nE 's/.*"text":"([[:alnum:]_-]+)".*/\1/p' | sed -n '1p')"
if [[ -n "${roots_req_id}" ]]; then
  rpc_raw "       post response for roots/list ${roots_req_id}" \
    "{\"jsonrpc\":\"2.0\",\"id\":\"${roots_req_id}\",\"result\":{\"roots\":[]}}" >/dev/null
fi

next_id
id="${NEXT_ID}"
resp_sampling_req="$(rpc_ok "       tools/call self.server.sampling_create" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tools/call\",\"params\":{\"name\":\"self.server.sampling_create\",\"arguments\":{}}}")"
sampling_req_id="$(printf '%s\n' "${resp_sampling_req}" | sed -nE 's/.*"text":"([[:alnum:]_-]+)".*/\1/p' | sed -n '1p')"
if [[ -n "${sampling_req_id}" ]]; then
  rpc_raw "       post response for sampling/createMessage ${sampling_req_id}" \
    "{\"jsonrpc\":\"2.0\",\"id\":\"${sampling_req_id}\",\"result\":{\"model\":\"mock\",\"content\":[{\"type\":\"text\",\"text\":\"ok\"}]}}" >/dev/null
fi

next_id
id="${NEXT_ID}"
resp_elicitation_req="$(rpc_ok "       tools/call self.server.elicitation_request" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tools/call\",\"params\":{\"name\":\"self.server.elicitation_request\",\"arguments\":{}}}")"
elicitation_req_id="$(printf '%s\n' "${resp_elicitation_req}" | sed -nE 's/.*"text":"([[:alnum:]_-]+)".*/\1/p' | sed -n '1p')"
if [[ -n "${elicitation_req_id}" ]]; then
  rpc_raw "       post response for elicitation/create ${elicitation_req_id}" \
    "{\"jsonrpc\":\"2.0\",\"id\":\"${elicitation_req_id}\",\"result\":{\"values\":{\"confirm\":true}}}" >/dev/null
fi

next_id
id="${NEXT_ID}"
resp_reinit_rejected="$(rpc_raw "       re-initialize should be invalid_request" \
  "{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"${PROTO}\",\"clientInfo\":{\"name\":\"sse-e2e-no-cap\",\"version\":\"1.0.0\"},\"capabilities\":{}}}")"

resp_batch_mixed_invalid="$(rpc_raw "       batch mixed with invalid element" \
  "[{\"jsonrpc\":\"2.0\",\"id\":\"bm1\",\"method\":\"ping\",\"params\":{}},123,{\"jsonrpc\":\"2.0\",\"id\":\"bm2\",\"method\":\"ping\",\"params\":{}}]")"
resp_batch_all_notifications="$(rpc_raw "       batch all notifications (no response expected)" \
  "[{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\",\"params\":{}},{\"jsonrpc\":\"2.0\",\"method\":\"notifications/cancelled\",\"params\":{}}]")"
resp_batch_all_notifications_http="$(curl -sS -i -X POST "${BASE_URL}" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  -H "MCP-Protocol-Version: ${PROTO}" \
  "${AUTH_CURL_HEADER[@]}" \
  -H "MCP-Session-Id: ${ACTIVE_SESSION_ID}" \
  -d '[{"jsonrpc":"2.0","method":"notifications/initialized","params":{}},{"jsonrpc":"2.0","method":"notifications/cancelled","params":{}}]' || true)"
resp_batch_all_notifications_body="$(printf '%s' "${resp_batch_all_notifications_http}" | awk 'BEGIN{body=0} body{print} /^\r?$/{body=1}')"
resp_batch_all_notifications_body_compact="$(printf '%s' "${resp_batch_all_notifications_body}" | tr -d '\r\n[:space:]')"
resp_batch_empty="$(rpc_raw "       empty batch should be invalid_request" "[]")"
resp_bad_content_type="$(curl -sS -i -X POST "${BASE_URL}" \
  -H "Content-Type: text/plain" \
  -H "Accept: application/json, text/event-stream" \
  -H "MCP-Protocol-Version: ${PROTO}" \
  "${AUTH_CURL_HEADER[@]}" \
  -d '{"jsonrpc":"2.0","id":"bad-ct","method":"ping","params":{}}' || true)"
resp_bad_accept_post="$(curl -sS -i -X POST "${BASE_URL}" \
  -H "Content-Type: application/json" \
  -H "Accept: text/html" \
  -H "MCP-Protocol-Version: ${PROTO}" \
  "${AUTH_CURL_HEADER[@]}" \
  -d '{"jsonrpc":"2.0","id":"bad-accept","method":"ping","params":{}}' || true)"
resp_accept_json_only_ping="$(curl -sS -i -X POST "${BASE_URL}" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json" \
  -H "MCP-Protocol-Version: ${PROTO}" \
  "${AUTH_CURL_HEADER[@]}" \
  -H "MCP-Session-Id: ${ACTIVE_SESSION_ID}" \
  -d '{"jsonrpc":"2.0","id":"accept-json-only","method":"ping","params":{}}' || true)"
resp_bad_accept_sse_only_ping="$(curl -sS -i -X POST "${BASE_URL}" \
  -H "Content-Type: application/json" \
  -H "Accept: text/event-stream" \
  -H "MCP-Protocol-Version: ${PROTO}" \
  "${AUTH_CURL_HEADER[@]}" \
  -H "MCP-Session-Id: ${ACTIVE_SESSION_ID}" \
  -d '{"jsonrpc":"2.0","id":"bad-accept-sse-only","method":"ping","params":{}}' || true)"
resp_missing_proto_post="$(curl -sS -i -X POST "${BASE_URL}" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  "${AUTH_CURL_HEADER[@]}" \
  -d '{"jsonrpc":"2.0","id":"proto-missing","method":"ping","params":{}}' || true)"
resp_missing_proto_init_legacy="$(curl -sS -i -X POST "${BASE_URL}" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  "${AUTH_CURL_HEADER[@]}" \
  -d '{"jsonrpc":"2.0","id":"proto-init-missing","method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"proto-check","version":"1.0.0"}}}' || true)"
proto_init_missing_session_id="$(printf '%s\n' "${resp_missing_proto_init_legacy}" | sed -nE 's/^MCP-Session-Id:[[:space:]]*([^[:space:]\r]+).*/\1/ip' | sed -n '1p' | tr -d '\r')"
resp_missing_proto_session_ping="$(curl -sS -i -X POST "${BASE_URL}" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  "${AUTH_CURL_HEADER[@]}" \
  -H "MCP-Session-Id: ${proto_init_missing_session_id}" \
  -d '{"jsonrpc":"2.0","id":"proto-session-missing","method":"ping","params":{}}' || true)"
resp_unsupported_proto_post="$(curl -sS -i -X POST "${BASE_URL}" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  -H "MCP-Protocol-Version: 2099-01-01" \
  "${AUTH_CURL_HEADER[@]}" \
  -H "MCP-Session-Id: ${ACTIVE_SESSION_ID}" \
  -d '{"jsonrpc":"2.0","id":"proto-unsupported","method":"ping","params":{}}' || true)"
resp_parse_error="$(curl -sS -X POST "${BASE_URL}" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  -H "MCP-Protocol-Version: ${PROTO}" \
  "${AUTH_CURL_HEADER[@]}" \
  -H "MCP-Session-Id: ${ACTIVE_SESSION_ID}" \
  -d '{"jsonrpc":"2.0","id":"parse-bad","method":"ping","params":' || true)"

sleep 2

session_expire_probe=""
SESSION_EXPIRED=0
if [[ "${SESSION_EXPIRE_WAIT_SECS}" -gt 0 ]]; then
  log "session-expire probe: wait ${SESSION_EXPIRE_WAIT_SECS}s"
  sleep "${SESSION_EXPIRE_WAIT_SECS}"
  session_expire_probe="$(curl -sS -i -X POST "${BASE_URL}" \
    -H "Content-Type: application/json" \
    -H "Accept: application/json, text/event-stream" \
    -H "MCP-Protocol-Version: ${PROTO}" \
    -H "MCP-Session-Id: ${SESSION_ID_1}" \
    "${AUTH_CURL_HEADER[@]}" \
    -d '{"jsonrpc":"2.0","id":"session-expire-probe","method":"ping","params":{}}' || true)"
  if text_contains "${session_expire_probe}" "404 Not Found"; then
    SESSION_EXPIRED=1
  fi
fi

delete_resp="$(delete_session "${SESSION_ID_1}")"
delete_resp_body="$(printf '%s' "${delete_resp}" | awk 'BEGIN{body=0} body{print} /^\r?$/{body=1}')"
delete_resp_body_compact="$(printf '%s' "${delete_resp_body}" | tr -d '\r\n[:space:]')"
delete_again_resp="$(delete_session "${SESSION_ID_1}")"
post_after_delete="$(curl -sS -i -X POST "${BASE_URL}" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  -H "MCP-Protocol-Version: ${PROTO}" \
  -H "MCP-Session-Id: ${SESSION_ID_1}" \
  "${AUTH_CURL_HEADER[@]}" \
  -d '{"jsonrpc":"2.0","id":"post-after-delete","method":"ping","params":{}}' || true)"
sleep 1
if [[ -n "${SSE2_PID:-}" ]]; then
  kill "${SSE2_PID}" >/dev/null 2>&1 || true
  unset SSE2_PID
fi

if [[ "${STRICT}" -eq 1 ]]; then
  assert_contains "${oauth_resource_metadata}" "\"resource\"" "protected-resource metadata missing resource"
  assert_contains "${oauth_resource_metadata}" "\"authorization_servers\"" "protected-resource metadata missing authorization_servers"
  assert_contains "${oauth_resource_metadata}" "\"bearer_methods_supported\"" "protected-resource metadata missing bearer_methods_supported"
  assert_contains "${oauth_auth_server_metadata}" "\"issuer\"" "authorization-server metadata missing issuer"
  assert_contains "${oauth_auth_server_metadata}" "\"authorization_endpoint\"" "authorization-server metadata missing authorization_endpoint"
  assert_contains "${oauth_auth_server_metadata}" "\"token_endpoint\"" "authorization-server metadata missing token_endpoint"
  assert_contains "${oauth_auth_server_metadata}" "\"registration_endpoint\"" "authorization-server metadata missing registration_endpoint"
  assert_contains "${oauth_auth_server_metadata}" "\"jwks_uri\"" "authorization-server metadata missing jwks_uri"
  assert_contains "${oauth_auth_server_metadata}" "\"response_types_supported\"" "authorization-server metadata missing response_types_supported"
  assert_contains "${oauth_auth_server_metadata}" "\"grant_types_supported\"" "authorization-server metadata missing grant_types_supported"
  assert_contains "${oauth_auth_server_metadata}" "\"code_challenge_methods_supported\"" "authorization-server metadata missing code_challenge_methods_supported"
  if [[ "${AUTH_REQUIRED}" -eq 1 ]]; then
    assert_contains "${auth_probe_no_token}" "401 Unauthorized" "auth matrix: missing-token should be 401"
    assert_contains "${auth_probe_no_token}" "WWW-Authenticate: Bearer realm=\"mcp\"" "auth matrix: missing-token missing realm"
    assert_contains "${auth_probe_no_token}" "resource_metadata=" "auth matrix: missing-token missing resource_metadata"
    assert_contains "${auth_probe_no_token}" "scope=\"" "auth matrix: missing-token missing scope"
    assert_contains "${auth_probe_no_token}" "resource=\"" "auth matrix: missing-token missing resource"
    assert_contains "${auth_probe_bad_token}" "403 Forbidden" "auth matrix: wrong-token should be 403"
    assert_contains "${auth_probe_bad_token}" "invalid_token" "auth matrix: wrong-token missing invalid_token"
    if [[ -n "${AUTH_TOKEN}" ]]; then
      assert_contains "${auth_probe_good_token}" "\"protocolVersion\":\"${PROTO}\"" "auth matrix: correct-token should pass initialize"
    fi
    if [[ -n "${AUTH_LOW_SCOPE_TOKEN}" ]]; then
      assert_contains "${auth_probe_low_scope}" "403 Forbidden" "auth matrix: low-scope token should be 403"
      assert_contains "${auth_probe_low_scope}" "scope_insufficient" "auth matrix: low-scope token missing reason"
    fi
    if [[ "${JWT_VERIFY_ACTIVE}" -eq 1 ]]; then
      assert_contains "${jwt_probe_invalid_format}" "403 Forbidden" "jwt matrix: invalid format should be 403"
      assert_contains "${jwt_probe_invalid_format}" "jwt_format_invalid" "jwt matrix: invalid format reason mismatch"
      assert_contains "${jwt_probe_expired}" "jwt_expired" "jwt matrix: expired reason mismatch"
      assert_contains "${jwt_probe_bad_iss}" "jwt_iss_mismatch" "jwt matrix: issuer reason mismatch"
      assert_contains "${jwt_probe_bad_aud}" "jwt_aud_mismatch" "jwt matrix: audience reason mismatch"
      assert_contains "${jwt_probe_missing_kid}" "jwt_kid_missing" "jwt matrix: missing kid reason mismatch"
      assert_contains "${jwt_probe_scope_insufficient}" "scope_insufficient" "jwt matrix: scope should be insufficient_scope"
      if text_contains "${jwt_probe_good}" "\"result\":{}"; then
        :
      else
        assert_contains "${jwt_probe_good}" "jwt_signature_invalid" "jwt matrix: good token should pass or fail with signature invalid"
      fi
      if [[ -n "${jwt_probe_bad_sig}" ]]; then
        assert_contains "${jwt_probe_bad_sig}" "jwt_signature_invalid" "jwt matrix: provided bad signature token should fail"
      fi
      if [[ -n "${jwt_probe_bad_kid}" ]]; then
        assert_contains "${jwt_probe_bad_kid}" "jwt_signature_invalid" "jwt matrix: provided bad kid token should fail"
      fi
    fi
  fi

  assert_contains "${resp_ping}" "\"id\":\"ping-1\"" "ping string-id response missing id"
  assert_contains "${resp_ping}" "\"result\":{}" "ping result is not empty object"
  assert_contains "${resp_dup_id_first}" "\"id\":\"dup-req\"" "duplicate-id first response missing id"
  assert_contains "${resp_dup_id_second}" "\"error\":{\"code\":-32600" "duplicate-id second should be invalid_request"
  assert_contains "${resp_dup_id_second}" "duplicate request id in session" "duplicate-id reason missing"
  assert_contains "${resp_tools_list}" "\"id\":\"tools-list-1\"" "tools/list string-id response missing id"
  assert_contains "${resp_tools_list}" "self.get_device_status" "tools/list missing self.get_device_status"
  assert_contains "${resp_tools_list}" "self.resource.touch_status" "tools/list missing self.resource.touch_status"
  assert_contains "${resp_tools_list}" "self.audio_speaker.set_volume" "tools/list missing self.audio_speaker.set_volume"
  assert_contains "${resp_tools_list}" "self.screen.set_brightness" "tools/list missing self.screen.set_brightness"
  assert_contains "${resp_tools_list}" "self.screen.set_theme" "tools/list missing self.screen.set_theme"
  assert_contains "${resp_tools_list}" "self.screen.set_rgb" "tools/list missing self.screen.set_rgb"
  assert_contains "${resp_tools_list}" "self.screen.set_hsv" "tools/list missing self.screen.set_hsv"
  assert_contains "${resp_tools_list}" "\"annotations\"" "tools/list missing tool annotations"

  assert_contains "${resp_tool_call_status}" "\"content\"" "tools/call self.get_device_status missing content"
  assert_contains "${resp_set_volume}" "\"text\":\"true\"" "set_volume did not return true"
  assert_contains "${resp_set_brightness}" "\"text\":\"true\"" "set_brightness did not return true"
  assert_contains "${resp_set_theme}" "\"text\":\"true\"" "set_theme did not return true"
  assert_contains "${resp_set_rgb}" "\"text\":\"true\"" "set_rgb did not return true"
  assert_contains "${resp_set_hsv}" "\"text\":\"true\"" "set_hsv did not return true"

  assert_contains "${resp_resources_list}" "device://status" "resources/list missing device://status"
  assert_contains "${resp_resources_list}" "\"annotations\"" "resources/list missing annotations"
  assert_contains "${resp_resources_list}" "\"audience\"" "resources/list missing annotations.audience"
  assert_contains "${resp_resources_list}" "\"priority\"" "resources/list missing annotations.priority"
  assert_contains "${resp_resources_list}" "\"lastModified\"" "resources/list missing annotations.lastModified"
  assert_contains "${resp_resources_list_page1}" "\"nextCursor\"" "resources/list limit=1 missing nextCursor"
  if [[ -n "${resp_resources_list_page2}" ]]; then
    assert_contains "${resp_resources_list_page2}" "\"resources\"" "resources/list page2 missing resources"
  fi
  assert_contains "${resp_resource_read}" "device://status" "resources/read missing requested URI"
  assert_contains "${resp_resource_read}" "\\\"volume\\\":66" "resources/read missing updated volume"
  assert_contains "${resp_resource_read}" "\\\"brightness\\\":42" "resources/read missing updated brightness"
  assert_contains "${resp_resource_read}" "\\\"theme\\\":\\\"dark\\\"" "resources/read missing updated theme"
  assert_contains "${resp_resource_read}" "\\\"red\\\":11" "resources/read missing updated RGB.red"
  assert_contains "${resp_resource_read}" "\\\"green\\\":22" "resources/read missing updated RGB.green"
  assert_contains "${resp_resource_read}" "\\\"blue\\\":33" "resources/read missing updated RGB.blue"
  assert_contains "${resp_templates_list}" "device://sensors/{id}" "resources/templates/list missing device://sensors/{id}"

  assert_contains "${resp_prompts_list}" "status.summary" "prompts/list missing status.summary"
  assert_contains "${resp_prompts_list}" "\"annotations\"" "prompts/list missing annotations"
  assert_contains "${resp_prompts_list}" "\"audience\"" "prompts/list missing annotations.audience"
  assert_contains "${resp_prompts_list}" "\"priority\"" "prompts/list missing annotations.priority"
  assert_contains "${resp_prompts_list}" "\"lastModified\"" "prompts/list missing annotations.lastModified"
  assert_contains "${resp_prompt_get}" "\"messages\"" "prompts/get missing messages"
  assert_contains "${resp_prompt_get}" "screen" "prompts/get missing requested argument rendering"
  assert_contains "${resp_completion}" "\"completion\"" "completion/complete missing completion object"
  assert_contains "${resp_completion}" "\"values\"" "completion/complete missing values"
  assert_contains "${resp_completion_bad}" "\"error\":{\"code\":-32602" "completion invalid params error code mismatch"
  assert_contains "${resp_completion_bad}" "\"expected\":\"params.ref+params.argument\"" "completion invalid params missing error.data.expected"

  assert_contains "${resp_subscribe}" "\"result\"" "resources/subscribe missing result"
  assert_contains "${resp_unsubscribe}" "\"result\"" "resources/unsubscribe missing result"

  assert_contains "${task_resp}" "\"taskId\"" "task-mode tools/call missing taskId"
  assert_contains "${task_resp}" "\"status\"" "task-mode tools/call missing task status"
  assert_contains "${resp_tasks_get}" "\"taskId\":\"${task_id}\"" "tasks/get missing task id"
  assert_contains "${resp_tasks_result}" "\"content\"" "tasks/result missing content"
  assert_contains "${resp_tasks_result}" "\"isError\"" "tasks/result missing isError flag"
  assert_contains "${resp_tasks_result_done}" "\"content\"" "tasks/result (done-before-cancel) missing content"
  assert_contains "${resp_tasks_cancel_after_done}" "\"error\":{\"code\":-32602" "tasks/cancel after done error code mismatch"
  assert_contains "${resp_tasks_cancel_after_done}" "\"expected\":\"non-terminal params.taskId\"" "tasks/cancel after done missing error.data.expected"
  assert_contains "${resp_tasks_cancel}" "\"taskId\":\"${task_cancel_id}\"" "tasks/cancel missing task id"
  assert_contains "${resp_tasks_cancel}" "\"status\":\"cancelled\"" "tasks/cancel missing cancelled status"
  assert_contains "${resp_tasks_cancel_repeat}" "\"error\":{\"code\":-32602" "tasks/cancel repeat error code mismatch"
  assert_contains "${resp_tasks_cancel_repeat}" "\"expected\":\"non-terminal params.taskId\"" "tasks/cancel repeat missing error.data.expected"
  assert_contains "${resp_tasks_get_after_cancel}" "\"taskId\":\"${task_cancel_id}\"" "tasks/get after cancel missing task id"
  if text_contains "${resp_tasks_result_after_cancel}" "\"error\""; then
    assert_contains "${resp_tasks_result_after_cancel}" "\"error\":{\"code\":-32000" "tasks/result after cancel error code mismatch"
    assert_contains "${resp_tasks_result_after_cancel}" "\"reason\":\"task cancelled\"" "tasks/result after cancel missing cancellation reason"
  else
    assert_contains "${resp_tasks_result_after_cancel}" "\"taskId\":\"${task_cancel_id}\"" "tasks/result after cancel missing task id"
  fi

  assert_contains "${resp_batch}" "[{" "batch response is not a JSON array"
  assert_contains "${resp_batch}" "\"id\":\"b1\"" "batch response missing b1"
  assert_contains "${resp_batch}" "\"id\":\"b2\"" "batch response missing b2"
  assert_contains "${resp_batch}" "\"id\":\"b3\"" "batch response missing b3"
  assert_contains "${resp_batch}" "\"error\":{\"code\":-32601" "batch missing method_not_found error item"

  assert_contains "${resp_err_method}" "\"error\":{\"code\":-32601" "invalid method error code mismatch"
  assert_contains "${resp_err_method}" "\"data\":{\"method\":\"no/suchMethod\"}" "invalid method missing error.data.method"
  assert_contains "${resp_err_resource_not_found}" "\"error\":{\"code\":-32002" "resource-not-found error code mismatch"
  assert_contains "${resp_err_resource_not_found}" "\"uri\":\"device://not-found\"" "resource-not-found missing error.data.uri"
  assert_contains "${resp_err_invalid_params}" "\"error\":{\"code\":-32602" "invalid params error code mismatch"
  assert_contains "${resp_err_invalid_params}" "\"expected\":\"params.uri\"" "invalid params missing error.data.expected"
  assert_contains "${resp_err_invalid_id}" "\"error\":{\"code\":-32600" "invalid id error code mismatch"
  assert_contains "${resp_err_invalid_id}" "\"data\":{\"method\":\"tools/list\"}" "invalid id missing error.data.method"
  assert_contains "${resp_cancel_missing}" "\"error\"" "tasks/cancel missing expected error for unknown task"
  assert_contains "${resp_reinit_rejected}" "\"error\":{\"code\":-32600" "re-initialize should be invalid_request"
  assert_contains "${resp_reinit_rejected}" "\"reason\":\"initialize may only be sent once\"" "re-initialize missing invalid_request reason"
  assert_contains "${resp_batch_mixed_invalid}" "\"id\":\"bm1\"" "mixed-invalid batch missing bm1 response"
  assert_contains "${resp_batch_mixed_invalid}" "\"id\":\"bm2\"" "mixed-invalid batch missing bm2 response"
  assert_contains "${resp_batch_mixed_invalid}" "\"error\":{\"code\":-32600" "mixed-invalid batch missing invalid_request item"
  assert_contains "${resp_batch_empty}" "\"error\":{\"code\":-32600" "empty-batch should be invalid_request"
  assert_contains "${resp_batch_empty}" "\"reason\":\"empty batch\"" "empty-batch missing error.data.reason"
  assert_contains "${resp_parse_error}" "\"error\":{\"code\":-32700" "parse error code mismatch"
  assert_contains "${resp_parse_error}" "\"reason\":\"invalid json\"" "parse error missing error.data.reason"
  assert_contains "${resp_batch_all_notifications_http}" "202 Accepted" "batch notifications-only should return 202"
  assert_contains "${resp_bad_content_type}" "400 Bad Request" "invalid Content-Type should return 400"
  assert_contains "${resp_bad_content_type}" "Content-Type must include application/json" "invalid Content-Type reason mismatch"
  assert_contains "${resp_bad_accept_post}" "406 Not Acceptable" "POST with unacceptable Accept should return 406"
  assert_contains "${resp_accept_json_only_ping}" "200 OK" "POST with Accept application/json only should succeed (Streamable HTTP)"
  assert_contains "${resp_accept_json_only_ping}" "\"id\":\"accept-json-only\"" "Accept-json-only ping response missing request id"
  assert_contains "${resp_accept_json_only_ping}" "\"result\":{}" "Accept-json-only ping should return empty result"
  assert_contains "${resp_bad_accept_sse_only_ping}" "406 Not Acceptable" "POST non-initialize with SSE-only Accept should return 406"
  assert_contains "${resp_missing_proto_post}" "200 OK" "POST without MCP-Protocol-Version should be accepted"
  assert_contains "${resp_missing_proto_post}" "MCP-Protocol-Version: ${DEFAULT_SERVER_FALLBACK_PROTO}" "missing-version request should use fallback MCP protocol version"
  assert_contains "${resp_missing_proto_post}" "\"id\":\"proto-missing\"" "missing-version POST response missing request id"
  assert_contains "${resp_missing_proto_post}" "\"result\":{}" "missing-version POST ping should return empty result"
  assert_contains "${resp_missing_proto_init_legacy}" "200 OK" "initialize without MCP-Protocol-Version should be accepted"
  assert_contains "${resp_missing_proto_init_legacy}" "MCP-Protocol-Version: 2024-11-05" "initialize without protocol header should echo negotiated version"
  assert_contains "${resp_missing_proto_init_legacy}" "\"protocolVersion\":\"2024-11-05\"" "initialize without protocol header should negotiate requested version"
  assert_contains "${resp_missing_proto_init_legacy}" "\"id\":\"proto-init-missing\"" "initialize without protocol header response missing request id"
  if [[ -z "${proto_init_missing_session_id}" ]]; then
    echo "FAIL: initialize without protocol header did not return MCP-Session-Id" >&2
    echo "HTTP response was: ${resp_missing_proto_init_legacy}" >&2
    exit 1
  fi
  assert_contains "${resp_missing_proto_session_ping}" "200 OK" "session follow-up without MCP-Protocol-Version should be accepted"
  assert_contains "${resp_missing_proto_session_ping}" "MCP-Protocol-Version: 2024-11-05" "session follow-up without protocol header should reuse negotiated session version"
  assert_contains "${resp_missing_proto_session_ping}" "\"id\":\"proto-session-missing\"" "session follow-up without protocol header response missing request id"
  assert_contains "${resp_missing_proto_session_ping}" "\"result\":{}" "session follow-up without protocol header ping should return empty result"
  assert_contains "${resp_unsupported_proto_post}" "400 Bad Request" "unsupported MCP-Protocol-Version should return 400"
  assert_contains "${resp_unsupported_proto_post}" "Unsupported MCP-Protocol-Version" "unsupported MCP-Protocol-Version reason mismatch"
  if text_contains "${resp_bad_accept_post}" "application/json and/or text/event-stream"; then
    :
  else
    dbg "unacceptable Accept reason text missing (allowed when peer closes before body flush)"
  fi
  if text_contains "${resp_bad_accept_sse_only_ping}" "non-initialize"; then
    :
  else
    dbg "SSE-only Accept non-init reason text missing (allowed when peer closes before body flush)"
  fi
  if [[ -n "${resp_batch_all_notifications_body_compact}" ]]; then
    echo "FAIL: notifications-only batch (202) must not include response body" >&2
    echo "HTTP response was: ${resp_batch_all_notifications_http}" >&2
    exit 1
  fi
  if [[ -n "${resp_batch_all_notifications}" ]]; then
    echo "FAIL: batch with notifications only should not return JSON-RPC body" >&2
    echo "Response was: ${resp_batch_all_notifications}" >&2
    exit 1
  fi

  sse_log1_head="$(sed -n '1,8p' "${SSE_LOG1}")"
  assert_contains "$(cat "${SSE_LOG1}")" ": ping" "SSE stream #1 missing heartbeat comment"
  assert_contains "${sse_log1_head}" "id: " "SSE stream #1 missing priming event id"
  assert_contains "${sse_log1_head}" "data:" "SSE stream #1 missing priming empty data field"
  assert_not_contains "${sse_log1_head}" "event: endpoint" "SSE stream #1 must not send legacy endpoint event on MCP endpoint"
  assert_contains "$(cat "${SSE_LOG1}")" "notifications/resources/updated" "SSE stream #1 missing resources/updated"
  assert_contains "$(cat "${SSE_LOG1}")" "notifications/resources/list_changed" "SSE stream #1 missing resources/list_changed"
  assert_contains "$(cat "${SSE_HDR1}")" "MCP-Session-Id:" "SSE stream #1 missing MCP-Session-Id header"
  if [[ -z "${SESSION_ID_1}" ]]; then
    echo "FAIL: SSE stream #1 failed to parse session id" >&2
    exit 1
  fi

  assert_contains "$(cat "${SSE_LOG2}")" "notifications/resources/updated" "SSE stream #2 missing resources/updated after reconnect"
  assert_contains "$(cat "${SSE_HDR2}")" "MCP-Session-Id:" "SSE stream #2 missing MCP-Session-Id header"
  assert_contains "$(cat "${SSE_LOG2}")" "id: ${LAST_EVENT_ID}" "SSE stream #2 missing Last-Event-ID replay event"
  assert_contains "$(cat "${SSE_LOG2}")" "\"method\":\"roots/list\"" "SSE stream #2 missing server roots/list request"
  assert_contains "$(cat "${SSE_LOG2}")" "\"method\":\"sampling/createMessage\"" "SSE stream #2 missing server sampling/createMessage request"
  assert_contains "$(cat "${SSE_LOG2}")" "\"method\":\"elicitation/create\"" "SSE stream #2 missing server elicitation/create request"
  if text_contains "$(cat "${SSE_LOG2}")" "retry: "; then
    :
  else
    dbg "SSE stream #2 has no retry hint (allowed when peer closes without error-triggered flush)"
  fi
  if [[ "${SESSION_EXPIRED}" -eq 1 ]]; then
    assert_contains "${session_expire_probe}" "404 Not Found" "session expire probe should return 404"
    assert_contains "${delete_resp}" "404 Not Found" "DELETE should return 404 for expired session"
  else
    assert_contains "${delete_resp}" "200 OK" "DELETE session should return 200"
    if [[ -n "${delete_resp_body_compact}" ]]; then
      echo "FAIL: successful DELETE should not include response body" >&2
      echo "HTTP response was: ${delete_resp}" >&2
      exit 1
    fi
  fi
  assert_contains "${delete_again_resp}" "404 Not Found" "DELETE unknown session should return 404"
  assert_contains "${post_after_delete}" "404 Not Found" "POST on deleted session should return 404"
fi

echo "PASS: full-conformance profile passed (methods + RPC + SSE/reconnect/heartbeat)"
echo "SSE stream #1 log: ${SSE_LOG1}"
echo "SSE stream #2 log: ${SSE_LOG2}"
echo "SSE stream #1 headers: ${SSE_HDR1}"
echo "SSE stream #2 headers: ${SSE_HDR2}"
