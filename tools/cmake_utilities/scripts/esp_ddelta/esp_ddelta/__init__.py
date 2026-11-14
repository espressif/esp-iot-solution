# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Uncompressed ddelta encode/decode.

The patch is the raw DDELTA40 stream. Compression (xz) and the custom OTA
header are applied by gen_custom_ota.py, not by this package.
"""

import os
import tempfile

from esp_ddelta._core import decode_paths, encode_paths

__all__ = [
    'encode',
    'decode',
    'encode_file',
    'decode_file',
]


def encode(old: bytes, new: bytes) -> bytes:
    """Build an uncompressed ddelta patch from old to new."""
    with tempfile.TemporaryDirectory(prefix='esp_ddelta_') as directory:
        old_path = os.path.join(directory, 'old')
        new_path = os.path.join(directory, 'new')
        patch_path = os.path.join(directory, 'patch')
        with open(old_path, 'wb') as handle:
            handle.write(old)
        with open(new_path, 'wb') as handle:
            handle.write(new)
        encode_paths(old_path, new_path, patch_path)
        with open(patch_path, 'rb') as handle:
            return handle.read()


def decode(old: bytes, patch: bytes) -> bytes:
    """Apply an uncompressed ddelta patch to old and return the new bytes."""
    with tempfile.TemporaryDirectory(prefix='esp_ddelta_') as directory:
        old_path = os.path.join(directory, 'old')
        patch_path = os.path.join(directory, 'patch')
        new_path = os.path.join(directory, 'new')
        with open(old_path, 'wb') as handle:
            handle.write(old)
        with open(patch_path, 'wb') as handle:
            handle.write(patch)
        decode_paths(old_path, patch_path, new_path)
        with open(new_path, 'rb') as handle:
            return handle.read()


def encode_file(old_path: str, new_path: str, patch_path: str) -> None:
    """Write an uncompressed ddelta patch for two files."""
    encode_paths(old_path, new_path, patch_path)


def decode_file(old_path: str, patch_path: str, new_path: str) -> None:
    """Apply an uncompressed ddelta patch file."""
    decode_paths(old_path, patch_path, new_path)
