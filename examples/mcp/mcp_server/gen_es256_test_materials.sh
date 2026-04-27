#!/usr/bin/env bash
set -euo pipefail

# One-shot generator for ES256 keypair + JWKS + JWT test tokens.
# Outputs:
#   - es256_private.pem
#   - es256_public.pem
#   - jwks.json
#   - token_good.txt
#   - token_bad_kid.txt
#   - token_bad_sig.txt

OUT_DIR="${OUT_DIR:-/tmp/mcp-es256}"
MCP_ISS="${MCP_ISS:-https://example.local}"
MCP_AUD="${MCP_AUD:-mcp://esp32}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDKCONFIG_PATH="${SDKCONFIG_PATH:-${SCRIPT_DIR}/sdkconfig}"
# JWT time profile:
# - normal: iat/nbf around current time (default)
# - clock_agnostic: iat/nbf=0, exp in far future (for devices without synced RTC)
JWT_TIME_PROFILE="${JWT_TIME_PROFILE:-normal}"

need_cmd() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "ERROR: missing command: $1" >&2
        exit 1
    }
}

need_cmd openssl
need_cmd python3
need_cmd sed

mkdir -p "${OUT_DIR}"
cd "${OUT_DIR}"

echo "[1/4] Generate ES256 keypair in ${OUT_DIR}"
openssl ecparam -name prime256v1 -genkey -noout -out es256_private.pem
openssl ec -in es256_private.pem -pubout -out es256_public.pem

echo "[2/4] Generate JWKS (kid=k1)"
python3 - <<'PY'
import re
import json
import base64
import subprocess

def b64u(b: bytes) -> str:
    return base64.urlsafe_b64encode(b).decode().rstrip('=')

txt = subprocess.check_output(
    ["openssl", "ec", "-pubin", "-in", "es256_public.pem", "-text", "-noout"],
    text=True
)
m = re.search(r'pub:\n((?:\s+[0-9a-f:]+\n)+)', txt, re.I)
if not m:
    raise SystemExit("failed to parse public key")

hex_bytes = ''.join(re.sub(r'[^0-9a-f]', '', line) for line in m.group(1).splitlines())
pub = bytes.fromhex(hex_bytes)
if len(pub) != 65 or pub[0] != 0x04:
    raise SystemExit(f"unexpected EC public key format len={len(pub)}")

x = pub[1:33]
y = pub[33:65]

jwks = {
    "keys": [{
        "kty": "EC",
        "crv": "P-256",
        "use": "sig",
        "alg": "ES256",
        "kid": "k1",
        "x": b64u(x),
        "y": b64u(y),
    }]
}

with open("jwks.json", "w", encoding="utf-8") as f:
    f.write(json.dumps(jwks, separators=(',', ':')))

print(json.dumps(jwks))
PY

echo "[3/4] Generate good / bad_sig / bad_kid tokens"
MCP_ISS="${MCP_ISS}" MCP_AUD="${MCP_AUD}" JWT_TIME_PROFILE="${JWT_TIME_PROFILE}" python3 - <<'PY'
import os
import json
import time
import base64
import subprocess

ISS = os.environ.get("MCP_ISS", "https://example.local")
AUD = os.environ.get("MCP_AUD", "mcp://esp32")
TIME_PROFILE = os.environ.get("JWT_TIME_PROFILE", "normal")

def b64u(b: bytes) -> str:
    return base64.urlsafe_b64encode(b).decode().rstrip('=')

def der_to_raw64(der: bytes) -> bytes:
    # Minimal ASN.1 DER parser for ECDSA signature:
    # SEQUENCE(INTEGER r, INTEGER s)
    if len(der) < 8 or der[0] != 0x30:
        raise ValueError("bad DER")
    i = 2
    if der[1] & 0x80:
        n = der[1] & 0x7F
        i = 2 + n
    if der[i] != 0x02:
        raise ValueError("bad DER r")
    rl = der[i + 1]
    r = der[i + 2:i + 2 + rl]
    i = i + 2 + rl
    if der[i] != 0x02:
        raise ValueError("bad DER s")
    sl = der[i + 1]
    s = der[i + 2:i + 2 + sl]
    r = r.lstrip(b'\x00').rjust(32, b'\x00')
    s = s.lstrip(b'\x00').rjust(32, b'\x00')
    if len(r) != 32 or len(s) != 32:
        raise ValueError("bad r/s size")
    return r + s

def sign_es256(signing_input: bytes) -> str:
    with open("to_sign.bin", "wb") as f:
        f.write(signing_input)
    subprocess.check_call([
        "openssl", "dgst", "-sha256", "-sign", "es256_private.pem",
        "-out", "sig.der", "to_sign.bin"
    ])
    with open("sig.der", "rb") as f:
        der = f.read()
    raw = der_to_raw64(der)
    return b64u(raw)

def make_token(kid: str, scope: str = "mcp:full", exp_delta: int = 3600) -> str:
    now = int(time.time())
    if TIME_PROFILE == "clock_agnostic":
        iat = 0
        nbf = 0
        exp = 4102444800  # 2100-01-01T00:00:00Z (test-only profile)
    else:
        iat = now - 10
        nbf = now - 10
        exp = now + exp_delta
    header = {"alg": "ES256", "typ": "JWT", "kid": kid}
    payload = {
        "iss": ISS,
        "aud": AUD,
        "sub": "mcp-e2e",
        "scope": scope,
        "iat": iat,
        "nbf": nbf,
        "exp": exp,
    }
    h = b64u(json.dumps(header, separators=(',', ':')).encode())
    p = b64u(json.dumps(payload, separators=(',', ':')).encode())
    signing_input = f"{h}.{p}".encode()
    s = sign_es256(signing_input)
    return f"{h}.{p}.{s}"

good = make_token("k1")
bad_kid = make_token("k404")
# Tamper one char in signature part to break signature.
bad_sig = good[:-1] + ("A" if good[-1] != "A" else "B")

with open("token_good.txt", "w", encoding="utf-8") as f:
    f.write(good)
with open("token_bad_kid.txt", "w", encoding="utf-8") as f:
    f.write(bad_kid)
with open("token_bad_sig.txt", "w", encoding="utf-8") as f:
    f.write(bad_sig)

print("GOOD =", good)
print("BAD_KID =", bad_kid)
print("BAD_SIG =", bad_sig)
print("TIME_PROFILE =", TIME_PROFILE)
PY

echo "[4/5] Write JWKS override into sdkconfig"
JWKS_PATH="${OUT_DIR}/jwks.json" SDKCONFIG_PATH="${SDKCONFIG_PATH}" python3 - <<'PY'
import json
import os
import pathlib

jwks_path = pathlib.Path(os.environ["JWKS_PATH"])
sdkconfig_path = pathlib.Path(os.environ["SDKCONFIG_PATH"])

jwks_compact = json.dumps(json.loads(jwks_path.read_text(encoding="utf-8")), separators=(",", ":"))
# Kconfig string literal encoding.
escaped = jwks_compact.replace("\\", "\\\\").replace('"', '\\"')
target_key = "CONFIG_MCP_HTTP_AUTH_JWKS_JSON_OVERRIDE="
new_line = f'{target_key}"{escaped}"\n'

lines = sdkconfig_path.read_text(encoding="utf-8").splitlines(keepends=True)
replaced = False
for i, line in enumerate(lines):
    if line.startswith(target_key):
        lines[i] = new_line
        replaced = True
        break
if not replaced:
    lines.append(new_line)
sdkconfig_path.write_text("".join(lines), encoding="utf-8")
print("sdkconfig_jwks_written=ok")
PY

echo "[5/5] Done"
echo
echo "Artifacts:"
echo "  ${OUT_DIR}/es256_private.pem"
echo "  ${OUT_DIR}/es256_public.pem"
echo "  ${OUT_DIR}/jwks.json"
echo "  ${OUT_DIR}/token_good.txt"
echo "  ${OUT_DIR}/token_bad_kid.txt"
echo "  ${OUT_DIR}/token_bad_sig.txt"
echo
echo "Optional exports for test_sse_e2e.sh:"
echo "  export MCP_JWT_GOOD_TOKEN=\"\$(cat ${OUT_DIR}/token_good.txt)\""
echo "  export MCP_JWT_BAD_KID_TOKEN=\"\$(cat ${OUT_DIR}/token_bad_kid.txt)\""
echo "  export MCP_JWT_BAD_SIG_TOKEN=\"\$(cat ${OUT_DIR}/token_bad_sig.txt)\""
echo "  # If device time is not synced, regenerate with:"
echo "  # JWT_TIME_PROFILE=clock_agnostic bash ${SCRIPT_DIR}/gen_es256_test_materials.sh"
echo
echo "sdkconfig updated:"
echo "  ${SDKCONFIG_PATH}"
echo
echo "Standalone safe command:"
echo "  JWKS_PATH='${OUT_DIR}/jwks.json' SDKCONFIG_PATH='${SDKCONFIG_PATH}' python3 - <<'PY'"
echo "  import json, os, pathlib"
echo "  jwks=pathlib.Path(os.environ['JWKS_PATH']).read_text(encoding='utf-8')"
echo "  esc=json.dumps(json.loads(jwks), separators=(',',':')).replace('\\\\','\\\\\\\\').replace('\"','\\\\\"')"
echo "  p=pathlib.Path(os.environ['SDKCONFIG_PATH'])"
echo "  k='CONFIG_MCP_HTTP_AUTH_JWKS_JSON_OVERRIDE='"
echo "  lines=p.read_text(encoding='utf-8').splitlines(keepends=True)"
echo "  nl=f'{k}\"{esc}\"\\n'"
echo "  for i,l in enumerate(lines):"
echo "      if l.startswith(k): lines[i]=nl; break"
echo "  else: lines.append(nl)"
echo "  p.write_text(''.join(lines), encoding='utf-8')"
echo "  print('ok')"
echo "  PY"
