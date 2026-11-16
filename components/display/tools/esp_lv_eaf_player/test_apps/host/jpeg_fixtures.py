# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""JPEG fixture construction helpers, independent of the component package."""
def split_jpeg_header_and_scan(jb: bytes) -> tuple[bytes, int, bytes]:
    """Return (header_up_to_sos_data, sof0_offset, entropy_scan_without_eoi)."""
    if len(jb) < 4 or jb[0] != 0xFF or jb[1] != 0xD8:
        raise ValueError('Not a valid JPEG')
    pos = 2
    sof0_offset = -1
    n = len(jb)
    while pos + 4 <= n:
        if jb[pos] != 0xFF:
            pos += 1
            continue
        while pos < n and jb[pos] == 0xFF:
            pos += 1
        if pos >= n:
            break
        marker = jb[pos]
        pos += 1
        if marker in (0xD8, 0xD9) or (0xD0 <= marker <= 0xD7):
            continue
        seg_len = (jb[pos] << 8) | jb[pos + 1]
        if marker == 0xC0:
            sof0_offset = pos - 2
        if marker == 0xDA:
            hdr_end = pos + seg_len
            scan = jb[hdr_end:]
            if len(scan) >= 2 and scan[-2:] == b'\xFF\xD9':
                scan = scan[:-2]
            if sof0_offset < 0:
                raise ValueError('SOF0 not found before SOS')
            return jb[:hdr_end], sof0_offset, scan
        pos += seg_len
    raise ValueError('SOS marker not found in JPEG')

def rebuild_abbreviated_jpeg(hdr_template: bytes, sof0_offset: int, width: int, height: int, scan: bytes) -> bytes:
    hdr = bytearray(hdr_template)
    hdr[sof0_offset + 5] = (height >> 8) & 0xFF
    hdr[sof0_offset + 6] = height & 0xFF
    hdr[sof0_offset + 7] = (width >> 8) & 0xFF
    hdr[sof0_offset + 8] = width & 0xFF
    return bytes(hdr) + scan + b'\xFF\xD9'
