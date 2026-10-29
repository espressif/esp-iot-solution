set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BASE_TEST_SCRIPT="${SCRIPT_DIR}/test_sse_e2e.sh"
MATRIX_DIR="${SCRIPT_DIR}/ci/sdkconfig.matrix"

usage() {
  cat <<'EOF'
Usage:
  bash test_kconfig_matrix.sh list-focused
  bash test_kconfig_matrix.sh list-all
  bash test_kconfig_matrix.sh emit-focused-fragments <OUT_DIR>
  bash test_kconfig_matrix.sh emit-all-fragments <OUT_DIR>
  bash test_kconfig_matrix.sh run-focused <HOST> <ENDPOINT>
  bash test_kconfig_matrix.sh run-case <CASE_ID> <HOST> <ENDPOINT>

Environment variables for run modes:
  MATRIX_AUTH_TOKEN                good bearer token for auth-enabled cases
  MATRIX_LOW_SCOPE_TOKEN           low-scope token for scope matrix case
  MATRIX_JWT_BAD_SIG_TOKEN         optional bad-signature token for jwt signature matrix
  MATRIX_JWT_BAD_KID_TOKEN         optional bad-kid token for jwt signature matrix

Notes:
  - This script provides 12 focused CI-ready combinations and a full Kconfig dependency-aware expansion.
  - Focused combinations use ready-to-include sdkconfig fragment files under:
      examples/mcp/mcp_server/ci/sdkconfig.matrix/
EOF
}

die() {
  echo "FAIL: $*" >&2
  exit 1
}

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "missing command: $1"
}

assert_contains() {
  local haystack="$1"
  local needle="$2"
  local msg="$3"
  if ! printf '%s' "${haystack}" | grep -Fq -- "${needle}"; then
    echo "FAIL: ${msg}" >&2
    echo "Expected: ${needle}" >&2
    echo "Actual:" >&2
    echo "${haystack}" >&2
    exit 1
  fi
}

assert_not_contains() {
  local haystack="$1"
  local needle="$2"
  local msg="$3"
  if printf '%s' "${haystack}" | grep -Fq -- "${needle}"; then
    echo "FAIL: ${msg}" >&2
    echo "Unexpected: ${needle}" >&2
    echo "Actual:" >&2
    echo "${haystack}" >&2
    exit 1
  fi
}

header() {
  echo "==== $* ===="
}

focused_case_ids=(
  "baseline_hs1_sse1_auth0_meta1_hc1"
  "hs1_sse0_auth0_meta1_hc1_get405"
  "hs1_sse1_auth1_jwt0_scope0_meta1_hc1"
  "hs1_sse1_auth1_jwt0_scope1_meta1_hc1"
  "hs1_sse1_auth1_jwt1_sig0_kid1_meta1_hc1"
  "hs1_sse1_auth1_jwt1_sig0_kid0_meta1_hc1"
  "hs1_sse1_auth1_jwt1_sig1_jwks_valid_meta1_hc1"
  "hs1_sse1_auth1_jwt1_sig1_jwks_invalid_meta1_hc1"
  "hs1_sse1_auth1_meta0_hc1"
  "hs1_sse1_auth1_ttl_short_meta1_hc1"
  "hs0_hc1_endpoint_unavailable"
  "hs1_sse1_auth0_meta1_hc0_server_only"
)

case_title() {
  case "$1" in
    baseline_hs1_sse1_auth0_meta1_hc1) echo "HTTP_SERVER=y,SSE=y,AUTH=n,OAUTH_META=y,HTTP_CLIENT=y (baseline)" ;;
    hs1_sse0_auth0_meta1_hc1_get405) echo "HTTP_SERVER=y,SSE=n,AUTH=n,OAUTH_META=y,HTTP_CLIENT=y (GET=405)" ;;
    hs1_sse1_auth1_jwt0_scope0_meta1_hc1) echo "HTTP_SERVER=y,SSE=y,AUTH=y,JWT=n,SCOPE=n,OAUTH_META=y,HTTP_CLIENT=y" ;;
    hs1_sse1_auth1_jwt0_scope1_meta1_hc1) echo "HTTP_SERVER=y,SSE=y,AUTH=y,JWT=n,SCOPE=y,OAUTH_META=y,HTTP_CLIENT=y" ;;
    hs1_sse1_auth1_jwt1_sig0_kid1_meta1_hc1) echo "HTTP_SERVER=y,SSE=y,AUTH=y,JWT=y,VERIFY_SIG=n,REQUIRE_KID=y,HTTP_CLIENT=y" ;;
    hs1_sse1_auth1_jwt1_sig0_kid0_meta1_hc1) echo "HTTP_SERVER=y,SSE=y,AUTH=y,JWT=y,VERIFY_SIG=n,REQUIRE_KID=n,HTTP_CLIENT=y" ;;
    hs1_sse1_auth1_jwt1_sig1_jwks_valid_meta1_hc1) echo "HTTP_SERVER=y,SSE=y,AUTH=y,JWT=y,VERIFY_SIG=y,JWKS_OVERRIDE=valid,HTTP_CLIENT=y" ;;
    hs1_sse1_auth1_jwt1_sig1_jwks_invalid_meta1_hc1) echo "HTTP_SERVER=y,SSE=y,AUTH=y,JWT=y,VERIFY_SIG=y,JWKS_OVERRIDE=empty/invalid,HTTP_CLIENT=y" ;;
    hs1_sse1_auth1_meta0_hc1) echo "HTTP_SERVER=y,SSE=y,AUTH=y,OAUTH_META=n,HTTP_CLIENT=y" ;;
    hs1_sse1_auth1_ttl_short_meta1_hc1) echo "HTTP_SERVER=y,SSE=y,AUTH=y,SESSION_TTL=short,OAUTH_META=y,HTTP_CLIENT=y" ;;
    hs0_hc1_endpoint_unavailable) echo "HTTP_SERVER=n,HTTP_CLIENT=y (server endpoint unavailable)" ;;
    hs1_sse1_auth0_meta1_hc0_server_only) echo "HTTP_SERVER=y,SSE=y,AUTH=n,OAUTH_META=y,HTTP_CLIENT=n (server-only)" ;;
    *) echo "$1" ;;
  esac
}

case_fragment_path() {
  echo "${MATRIX_DIR}/$1.defaults"
}

case_script_params() {
  local c="$1" host="$2" endpoint="$3"
  case "${c}" in
    baseline_hs1_sse1_auth0_meta1_hc1)
      echo "bash \"${BASE_TEST_SCRIPT}\" \"${host}\" \"${endpoint}\" --strict"
      ;;
    hs1_sse0_auth0_meta1_hc1_get405)
      echo "curl GET Accept:text/event-stream expect 405; POST ping expect 200"
      ;;
    hs1_sse1_auth1_jwt0_scope0_meta1_hc1)
      echo "bash \"${BASE_TEST_SCRIPT}\" \"${host}\" \"${endpoint}\" --strict --auth-token \"\${MATRIX_AUTH_TOKEN}\""
      ;;
    hs1_sse1_auth1_jwt0_scope1_meta1_hc1)
      echo "bash \"${BASE_TEST_SCRIPT}\" \"${host}\" \"${endpoint}\" --strict --auth-token \"\${MATRIX_AUTH_TOKEN}\" --auth-low-scope-token \"\${MATRIX_LOW_SCOPE_TOKEN}\""
      ;;
    hs1_sse1_auth1_jwt1_sig0_kid1_meta1_hc1|hs1_sse1_auth1_jwt1_sig0_kid0_meta1_hc1|hs1_sse1_auth1_jwt1_sig1_jwks_valid_meta1_hc1|hs1_sse1_auth1_jwt1_sig1_jwks_invalid_meta1_hc1)
      echo "MCP_JWT_BAD_SIG_TOKEN=\"\${MATRIX_JWT_BAD_SIG_TOKEN:-}\" MCP_JWT_BAD_KID_TOKEN=\"\${MATRIX_JWT_BAD_KID_TOKEN:-}\" bash \"${BASE_TEST_SCRIPT}\" \"${host}\" \"${endpoint}\" --strict --jwt-matrix --auth-token \"\${MATRIX_AUTH_TOKEN}\""
      ;;
    hs1_sse1_auth1_meta0_hc1)
      echo "metadata 404/405 + bash \"${BASE_TEST_SCRIPT}\" \"${host}\" \"${endpoint}\" --auth-token \"\${MATRIX_AUTH_TOKEN}\""
      ;;
    hs1_sse1_auth1_ttl_short_meta1_hc1)
      echo "bash \"${BASE_TEST_SCRIPT}\" \"${host}\" \"${endpoint}\" --strict --auth-token \"\${MATRIX_AUTH_TOKEN}\" --session-expire-wait 4"
      ;;
    hs0_hc1_endpoint_unavailable)
      echo "curl endpoint/metadata expect non-200 or connection refused"
      ;;
    hs1_sse1_auth0_meta1_hc0_server_only)
      echo "bash \"${BASE_TEST_SCRIPT}\" \"${host}\" \"${endpoint}\" --strict"
      ;;
    *)
      echo "N/A"
      ;;
  esac
}

case_expected_assertions() {
  case "$1" in
    baseline_hs1_sse1_auth0_meta1_hc1) echo "SSE handshake + strict full profile pass" ;;
    hs1_sse0_auth0_meta1_hc1_get405) echo "GET SSE returns 405; POST JSON-RPC ping returns 200/result" ;;
    hs1_sse1_auth1_jwt0_scope0_meta1_hc1) echo "missing token=401, wrong token=403 invalid_token, good token passes" ;;
    hs1_sse1_auth1_jwt0_scope1_meta1_hc1) echo "low-scope token=403 scope_insufficient" ;;
    hs1_sse1_auth1_jwt1_sig0_kid1_meta1_hc1) echo "jwt matrix active; missing kid rejected; claim checks active" ;;
    hs1_sse1_auth1_jwt1_sig0_kid0_meta1_hc1) echo "jwt matrix active; missing kid path no longer mandatory" ;;
    hs1_sse1_auth1_jwt1_sig1_jwks_valid_meta1_hc1) echo "jwt signature verify active; valid token passes signature checks" ;;
    hs1_sse1_auth1_jwt1_sig1_jwks_invalid_meta1_hc1) echo "jwt signature verify active; good token fails with jwt_signature_invalid" ;;
    hs1_sse1_auth1_meta0_hc1) echo "metadata endpoint disabled (non-200), auth main flow still works with token" ;;
    hs1_sse1_auth1_ttl_short_meta1_hc1) echo "session-expire probe gets 404 after wait; delete-after-expire=404" ;;
    hs0_hc1_endpoint_unavailable) echo "server endpoint unavailable (non-200 or connection failure)" ;;
    hs1_sse1_auth0_meta1_hc0_server_only) echo "server-only baseline path passes strict profile" ;;
    *) echo "N/A" ;;
  esac
}

list_focused() {
  local host="${1:-<HOST>}" endpoint="${2:-<ENDPOINT>}"
  printf '%s\t%s\t%s\t%s\t%s\n' "case_id" "title" "sdkconfig_fragment" "script_params" "expected_assertions"
  local c
  for c in "${focused_case_ids[@]}"; do
    printf '%s\t%s\t%s\t%s\t%s\n' \
      "${c}" \
      "$(case_title "${c}")" \
      "$(case_fragment_path "${c}")" \
      "$(case_script_params "${c}" "${host}" "${endpoint}")" \
      "$(case_expected_assertions "${c}")"
  done
}

emit_focused_fragments() {
  local out_dir="$1"
  mkdir -p "${out_dir}"
  local c
  for c in "${focused_case_ids[@]}"; do
    cp -f "$(case_fragment_path "${c}")" "${out_dir}/${c}.defaults"
  done
  echo "Wrote ${#focused_case_ids[@]} focused fragments to ${out_dir}"
}

run_case() {
  local c="$1" host="$2" endpoint="$3"
  header "RUN ${c}"
  case "${c}" in
    baseline_hs1_sse1_auth0_meta1_hc1|hs1_sse1_auth0_meta1_hc0_server_only)
      bash "${BASE_TEST_SCRIPT}" "${host}" "${endpoint}" --strict
      ;;
    hs1_sse0_auth0_meta1_hc1_get405)
      local get_resp post_resp
      get_resp="$(curl -sS -i -H "Accept: text/event-stream" -H "MCP-Protocol-Version: 2025-11-25" "http://${host}/${endpoint}" || true)"
      assert_contains "${get_resp}" "405 Method Not Allowed" "SSE-disabled case must return 405 on GET"
      post_resp="$(curl -sS -i -X POST "http://${host}/${endpoint}" \
        -H "Content-Type: application/json" \
        -H "Accept: application/json, text/event-stream" \
        -H "MCP-Protocol-Version: 2025-11-25" \
        -d '{"jsonrpc":"2.0","id":"sse-off-ping","method":"ping","params":{}}' || true)"
      assert_contains "${post_resp}" "200 OK" "SSE-disabled case POST should still work"
      assert_contains "${post_resp}" "\"id\":\"sse-off-ping\"" "SSE-disabled case response id mismatch"
      ;;
    hs1_sse1_auth1_jwt0_scope0_meta1_hc1)
      [[ -n "${MATRIX_AUTH_TOKEN:-}" ]] || die "MATRIX_AUTH_TOKEN is required for ${c}"
      bash "${BASE_TEST_SCRIPT}" "${host}" "${endpoint}" --strict --auth-token "${MATRIX_AUTH_TOKEN}"
      ;;
    hs1_sse1_auth1_jwt0_scope1_meta1_hc1)
      [[ -n "${MATRIX_AUTH_TOKEN:-}" ]] || die "MATRIX_AUTH_TOKEN is required for ${c}"
      [[ -n "${MATRIX_LOW_SCOPE_TOKEN:-}" ]] || die "MATRIX_LOW_SCOPE_TOKEN is required for ${c}"
      bash "${BASE_TEST_SCRIPT}" "${host}" "${endpoint}" --strict \
        --auth-token "${MATRIX_AUTH_TOKEN}" \
        --auth-low-scope-token "${MATRIX_LOW_SCOPE_TOKEN}"
      ;;
    hs1_sse1_auth1_jwt1_sig0_kid1_meta1_hc1|hs1_sse1_auth1_jwt1_sig0_kid0_meta1_hc1|hs1_sse1_auth1_jwt1_sig1_jwks_valid_meta1_hc1|hs1_sse1_auth1_jwt1_sig1_jwks_invalid_meta1_hc1)
      [[ -n "${MATRIX_AUTH_TOKEN:-}" ]] || die "MATRIX_AUTH_TOKEN is required for ${c}"
      MCP_JWT_BAD_SIG_TOKEN="${MATRIX_JWT_BAD_SIG_TOKEN:-}" \
      MCP_JWT_BAD_KID_TOKEN="${MATRIX_JWT_BAD_KID_TOKEN:-}" \
      bash "${BASE_TEST_SCRIPT}" "${host}" "${endpoint}" --strict --jwt-matrix --auth-token "${MATRIX_AUTH_TOKEN}"
      ;;
    hs1_sse1_auth1_meta0_hc1)
      [[ -n "${MATRIX_AUTH_TOKEN:-}" ]] || die "MATRIX_AUTH_TOKEN is required for ${c}"
      local protected_meta_resp auth_server_meta_resp
      protected_meta_resp="$(curl -sS -i -H "Accept: application/json" "http://${host}/.well-known/oauth-protected-resource" || true)"
      auth_server_meta_resp="$(curl -sS -i -H "Accept: application/json" "http://${host}/.well-known/oauth-authorization-server" || true)"
      assert_not_contains "${protected_meta_resp}" "200 OK" "protected-resource metadata-disabled case should not return 200"
      assert_not_contains "${auth_server_meta_resp}" "200 OK" "authorization-server metadata-disabled case should not return 200"
      bash "${BASE_TEST_SCRIPT}" "${host}" "${endpoint}" --auth-token "${MATRIX_AUTH_TOKEN}"
      ;;
    hs1_sse1_auth1_ttl_short_meta1_hc1)
      [[ -n "${MATRIX_AUTH_TOKEN:-}" ]] || die "MATRIX_AUTH_TOKEN is required for ${c}"
      bash "${BASE_TEST_SCRIPT}" "${host}" "${endpoint}" --strict --auth-token "${MATRIX_AUTH_TOKEN}" --session-expire-wait 4
      ;;
    hs0_hc1_endpoint_unavailable)
      local resp
      resp="$(curl -sS -i -H "Accept: application/json" "http://${host}/${endpoint}" || true)"
      if printf '%s' "${resp}" | grep -Fq "200 OK"; then
        die "HTTP server disabled case unexpectedly returned 200 at endpoint"
      fi
      ;;
    *)
      die "unknown case: ${c}"
      ;;
  esac
}

run_focused() {
  local host="$1" endpoint="$2"
  local c
  for c in "${focused_case_ids[@]}"; do
    run_case "${c}" "${host}" "${endpoint}"
  done
}

combo_bool() {
  if [[ "$1" -eq 1 ]]; then echo "y"; else echo "n"; fi
}

emit_combo_fragment() {
  local hs="$1" sse="$2" auth="$3" meta="$4" hc="$5" jwt="$6" scope="$7" kid="$8" sig="$9"
  cat <<EOF
CONFIG_MCP_TRANSPORT_HTTP_SERVER=$(combo_bool "${hs}")
CONFIG_MCP_TRANSPORT_HTTP_CLIENT=$(combo_bool "${hc}")
EOF
  if [[ "${hs}" -eq 1 ]]; then
    cat <<EOF
CONFIG_MCP_HTTP_SSE_ENABLE=$(combo_bool "${sse}")
CONFIG_MCP_HTTP_OAUTH_METADATA_ENABLE=$(combo_bool "${meta}")
CONFIG_MCP_HTTP_DEFAULT_PROTOCOL_VERSION="2025-03-26"
CONFIG_MCP_HTTP_SESSION_TTL_MS=600000
CONFIG_MCP_HTTP_AUTH_ENABLE=$(combo_bool "${auth}")
EOF
    if [[ "${auth}" -eq 1 ]]; then
      cat <<'EOF'
CONFIG_MCP_HTTP_AUTH_BEARER_TOKEN="mcp-dev-token"
CONFIG_MCP_HTTP_AUTH_REQUIRED_SCOPE="mcp:full"
EOF
      cat <<EOF
CONFIG_MCP_HTTP_AUTH_SCOPE_ENABLE=$(combo_bool "${scope}")
EOF
      if [[ "${scope}" -eq 1 ]]; then
        cat <<'EOF'
CONFIG_MCP_HTTP_AUTH_LOW_SCOPE_TOKEN="mcp-readonly-token"
EOF
      fi
      cat <<EOF
CONFIG_MCP_HTTP_AUTH_JWT_VERIFY_ENABLE=$(combo_bool "${jwt}")
EOF
      if [[ "${jwt}" -eq 1 ]]; then
        cat <<'EOF'
CONFIG_MCP_HTTP_AUTH_JWT_EXPECT_ISS="https://example.local"
CONFIG_MCP_HTTP_AUTH_JWT_EXPECT_AUD="mcp://esp32"
CONFIG_MCP_HTTP_AUTH_JWT_CLOCK_SKEW_SEC=60
EOF
        cat <<EOF
CONFIG_MCP_HTTP_AUTH_JWT_REQUIRE_KID=$(combo_bool "${kid}")
CONFIG_MCP_HTTP_AUTH_JWT_VERIFY_SIGNATURE=$(combo_bool "${sig}")
EOF
        if [[ "${sig}" -eq 1 ]]; then
          cat <<'EOF'
CONFIG_MCP_HTTP_AUTH_JWKS_FETCH_TIMEOUT_MS=3000
CONFIG_MCP_HTTP_AUTH_JWKS_CACHE_TTL_SEC=300
CONFIG_MCP_HTTP_AUTH_JWKS_JSON_OVERRIDE=""
EOF
        fi
      fi
    fi
  fi
}

list_all() {
  printf '%s\t%s\t%s\n' "combo_id" "sdkconfig_fragment_hint" "expected_assertions"
  local hs hc sse auth meta jwt scope kid sig combo_id expected
  for hs in 0 1; do
    for hc in 0 1; do
      if [[ "${hs}" -eq 0 ]]; then
        combo_id="auto_hs0_hc${hc}"
        expected="server endpoint unavailable (or not MCP-serving)"
        printf '%s\t%s\t%s\n' "${combo_id}" "emit-all-fragments/${combo_id}.defaults" "${expected}"
        continue
      fi
      for sse in 0 1; do
        for auth in 0 1; do
          for meta in 0 1; do
            if [[ "${auth}" -eq 0 ]]; then
              combo_id="auto_hs1_hc${hc}_sse${sse}_auth0_meta${meta}"
              if [[ "${sse}" -eq 0 ]]; then
                expected="GET SSE=405; POST ping=200"
              else
                expected="SSE path active; no-auth RPC path active"
              fi
              printf '%s\t%s\t%s\n' "${combo_id}" "emit-all-fragments/${combo_id}.defaults" "${expected}"
              continue
            fi
            for jwt in 0 1; do
              for scope in 0 1; do
                if [[ "${jwt}" -eq 0 ]]; then
                  combo_id="auto_hs1_hc${hc}_sse${sse}_auth1_meta${meta}_jwt0_scope${scope}"
                  expected="auth static bearer path; optional low-scope path"
                  printf '%s\t%s\t%s\n' "${combo_id}" "emit-all-fragments/${combo_id}.defaults" "${expected}"
                  continue
                fi
                for kid in 0 1; do
                  for sig in 0 1; do
                    combo_id="auto_hs1_hc${hc}_sse${sse}_auth1_meta${meta}_jwt1_scope${scope}_kid${kid}_sig${sig}"
                    expected="jwt claim matrix active; signature path depends on sig=${sig}"
                    printf '%s\t%s\t%s\n' "${combo_id}" "emit-all-fragments/${combo_id}.defaults" "${expected}"
                  done
                done
              done
            done
          done
        done
      done
    done
  done
}

emit_all_fragments() {
  local out_dir="$1"
  mkdir -p "${out_dir}"
  local hs hc sse auth meta jwt scope kid sig combo_id
  for hs in 0 1; do
    for hc in 0 1; do
      if [[ "${hs}" -eq 0 ]]; then
        combo_id="auto_hs0_hc${hc}"
        emit_combo_fragment "${hs}" 0 0 0 "${hc}" 0 0 0 0 > "${out_dir}/${combo_id}.defaults"
        continue
      fi
      for sse in 0 1; do
        for auth in 0 1; do
          for meta in 0 1; do
            if [[ "${auth}" -eq 0 ]]; then
              combo_id="auto_hs1_hc${hc}_sse${sse}_auth0_meta${meta}"
              emit_combo_fragment "${hs}" "${sse}" "${auth}" "${meta}" "${hc}" 0 0 0 0 > "${out_dir}/${combo_id}.defaults"
              continue
            fi
            for jwt in 0 1; do
              for scope in 0 1; do
                if [[ "${jwt}" -eq 0 ]]; then
                  combo_id="auto_hs1_hc${hc}_sse${sse}_auth1_meta${meta}_jwt0_scope${scope}"
                  emit_combo_fragment "${hs}" "${sse}" "${auth}" "${meta}" "${hc}" "${jwt}" "${scope}" 0 0 > "${out_dir}/${combo_id}.defaults"
                  continue
                fi
                for kid in 0 1; do
                  for sig in 0 1; do
                    combo_id="auto_hs1_hc${hc}_sse${sse}_auth1_meta${meta}_jwt1_scope${scope}_kid${kid}_sig${sig}"
                    emit_combo_fragment "${hs}" "${sse}" "${auth}" "${meta}" "${hc}" "${jwt}" "${scope}" "${kid}" "${sig}" > "${out_dir}/${combo_id}.defaults"
                  done
                done
              done
            done
          done
        done
      done
    done
  done
  local count
  count="$(ls -1 "${out_dir}"/*.defaults 2>/dev/null | wc -l | tr -d ' ')"
  echo "Wrote ${count} dependency-valid Kconfig combo fragments to ${out_dir}"
}

main() {
  require_cmd curl
  require_cmd grep
  require_cmd bash
  [[ -f "${BASE_TEST_SCRIPT}" ]] || die "missing base test script: ${BASE_TEST_SCRIPT}"

  local cmd="${1:-}"
  case "${cmd}" in
    list-focused)
      list_focused "${2:-<HOST>}" "${3:-<ENDPOINT>}"
      ;;
    list-all)
      list_all
      ;;
    emit-focused-fragments)
      [[ $# -eq 2 ]] || die "usage: emit-focused-fragments <OUT_DIR>"
      emit_focused_fragments "$2"
      ;;
    emit-all-fragments)
      [[ $# -eq 2 ]] || die "usage: emit-all-fragments <OUT_DIR>"
      emit_all_fragments "$2"
      ;;
    run-focused)
      [[ $# -eq 3 ]] || die "usage: run-focused <HOST> <ENDPOINT>"
      run_focused "$2" "$3"
      ;;
    run-case)
      [[ $# -eq 4 ]] || die "usage: run-case <CASE_ID> <HOST> <ENDPOINT>"
      run_case "$2" "$3" "$4"
      ;;
    -h|--help|help|"")
      usage
      ;;
    *)
      die "unknown command: ${cmd}"
      ;;
  esac
}

main "$@"
