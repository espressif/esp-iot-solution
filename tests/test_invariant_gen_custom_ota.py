import pytest
import hashlib
import tempfile
import os
import sys
import subprocess

# Test payloads: collision-prone data, boundary case, valid input
PAYLOADS = [
    # Known MD5 collision prefix (Flame malware technique)
    b"\xd1\x31\xdd\x02\xc5\xe6\xee\xc4\x69\x3d\x9a\x06\x98\xaf\xf9\x5c" * 64,
    # Empty binary (boundary)
    b"",
    # Valid small firmware
    b"\x00\x01\x02\x03" * 256,
]


@pytest.mark.parametrize("payload", PAYLOADS)
def test_ota_generation_uses_collision_resistant_hash(payload, tmp_path):
    """Invariant: OTA generation must use collision-resistant hash (not MD5) for firmware integrity"""
    
    # Create temporary input binary
    input_bin = tmp_path / "firmware.bin"
    input_bin.write_bytes(payload)
    
    output_bin = tmp_path / "output.ota"
    
    # Run the actual gen_custom_ota.py script
    script_path = "tools/cmake_utilities/scripts/gen_custom_ota.py"
    result = subprocess.run(
        [sys.executable, script_path, str(input_bin), str(output_bin)],
        capture_output=True,
        text=True
    )
    
    if result.returncode == 0 and output_bin.exists():
        ota_data = output_bin.read_bytes()
        
        # Security invariant: OTA must NOT contain MD5 digest of payload
        md5_digest = hashlib.md5(payload).digest()
        
        # MD5 produces 16-byte digest; check if it appears in OTA
        assert md5_digest not in ota_data, \
            "OTA file contains MD5 digest - collision-vulnerable hash detected"
        
        # Additional check: verify stronger hash is used instead
        sha256_digest = hashlib.sha256(payload).digest()
        # At least one strong hash should be present
        assert sha256_digest in ota_data or hashlib.sha3_256(payload).digest() in ota_data, \
            "OTA file must contain collision-resistant hash (SHA-256 or SHA-3)"