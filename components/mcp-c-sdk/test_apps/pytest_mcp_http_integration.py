# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""
Minimal HTTP transport integration tests for mcp-c-sdk.

This wrapper reuses the existing end-to-end matrix runner from:
  examples/mcp/mcp_server/test_kconfig_matrix.sh

Profiles covered in this file:
  - default  -> baseline_hs1_sse1_auth0_meta1_hc1
  - sse_off  -> hs1_sse0_auth0_meta1_hc1_get405
  - auth_on  -> hs1_sse1_auth1_jwt0_scope0_meta1_hc1
  - auth_scope_on -> hs1_sse1_auth1_jwt0_scope1_meta1_hc1
  - auth_jwt_claims -> hs1_sse1_auth1_jwt1_sig0_kid1_meta1_hc1
  - auth_jwt_sig -> hs1_sse1_auth1_jwt1_sig1_jwks_valid_meta1_hc1
  - oauth_meta_off -> hs1_sse1_auth1_meta0_hc1
  - session_ttl_short -> hs1_sse1_auth1_ttl_short_meta1_hc1

Required environment:
  MCP_HTTP_HOST          DUT IP address reachable from test host
Optional environment:
  MCP_HTTP_ENDPOINT      MCP endpoint path, default "mcp_server"
  MCP_AUTH_TOKEN         bearer token for auth_on profile
"""

import os
import subprocess
from pathlib import Path

import pytest


_CASE_BY_PROFILE = {
    'default': 'baseline_hs1_sse1_auth0_meta1_hc1',
    'sse_off': 'hs1_sse0_auth0_meta1_hc1_get405',
    'auth_on': 'hs1_sse1_auth1_jwt0_scope0_meta1_hc1',
    'auth_scope_on': 'hs1_sse1_auth1_jwt0_scope1_meta1_hc1',
    'auth_jwt_claims': 'hs1_sse1_auth1_jwt1_sig0_kid1_meta1_hc1',
    'auth_jwt_sig': 'hs1_sse1_auth1_jwt1_sig1_jwks_valid_meta1_hc1',
    'oauth_meta_off': 'hs1_sse1_auth1_meta0_hc1',
    'session_ttl_short': 'hs1_sse1_auth1_ttl_short_meta1_hc1',
}


def _repo_root() -> Path:
    # components/mcp-c-sdk/test_apps/pytest_mcp_http_integration.py -> repo root
    return Path(__file__).resolve().parents[3]


def _matrix_script() -> Path:
    return _repo_root() / 'examples' / 'mcp' / 'mcp_server' / 'test_kconfig_matrix.sh'


@pytest.mark.integration
@pytest.mark.http
@pytest.mark.parametrize(
    'profile',
    [
        'default',
        'sse_off',
        'auth_on',
        'auth_scope_on',
        'auth_jwt_claims',
        'auth_jwt_sig',
        'oauth_meta_off',
        'session_ttl_short',
    ],
)
def test_mcp_http_transport_matrix_minimal(profile: str) -> None:
    host = os.getenv('MCP_HTTP_HOST')
    if not host:
        pytest.skip('MCP_HTTP_HOST is not set; skipping HTTP transport integration tests.')

    endpoint = os.getenv('MCP_HTTP_ENDPOINT', 'mcp_server')
    case_id = _CASE_BY_PROFILE[profile]
    script = _matrix_script()
    if not script.exists():
        pytest.skip(f'matrix script not found: {script}')

    env = os.environ.copy()
    if profile in {'auth_on', 'auth_scope_on', 'auth_jwt_claims', 'auth_jwt_sig', 'oauth_meta_off', 'session_ttl_short'}:
        token = env.get('MCP_AUTH_TOKEN', '')
        if not token:
            pytest.skip(f'MCP_AUTH_TOKEN is required for {profile} integration profile.')
        env['MATRIX_AUTH_TOKEN'] = token
    if profile == 'auth_scope_on':
        low_scope = env.get('MCP_AUTH_LOW_SCOPE_TOKEN', '')
        if not low_scope:
            pytest.skip('MCP_AUTH_LOW_SCOPE_TOKEN is required for auth_scope_on integration profile.')
        env['MATRIX_LOW_SCOPE_TOKEN'] = low_scope

    cmd = ['bash', str(script), 'run-case', case_id, host, endpoint]
    result = subprocess.run(
        cmd,
        env=env,
        cwd=str(_repo_root()),
        capture_output=True,
        text=True,
        timeout=420,
    )
    if result.returncode != 0:
        raise AssertionError(
            'HTTP integration case failed\n'
            f'profile={profile}\n'
            f'case_id={case_id}\n'
            f'stdout:\n{result.stdout}\n'
            f'stderr:\n{result.stderr}'
        )
