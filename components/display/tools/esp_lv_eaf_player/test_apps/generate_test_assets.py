# SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Generate tiny procedural fixtures for core and LVGL component tests."""

from __future__ import annotations

import struct
import base64
import sys
import zlib
from pathlib import Path


EAF4_HEADER = struct.Struct('<4sHHHHIIIIIIHHIII12s')
EAF4_INDEX = struct.Struct('<IIIIBBHI')


def write_legacy(path: Path) -> None:
    width = height = 16
    pixels = bytearray()
    for y in range(height):
        for x in range(width):
            pixels.append(1 + ((x // 4 + y // 4) & 1))

    rle = bytearray()
    run_value = pixels[0]
    run_length = 0
    for value in pixels:
        if value == run_value and run_length < 255:
            run_length += 1
        else:
            rle.extend((run_length, run_value))
            run_value = value
            run_length = 1
    rle.extend((run_length, run_value))

    palette = bytearray(256 * 4)
    palette[4:8] = bytes((0x20, 0x90, 0xF0, 0xFF))
    palette[8:12] = bytes((0xF0, 0x80, 0x20, 0xA0))
    block = b'\x00' + bytes(rle)  # codec 0x00: Palette RLE
    image = (
        b'_S\x00' + b'00V1.0' + bytes((8,))
        + struct.pack('<HHHHI', width, height, 1, height, len(block))
        + bytes(palette) + block
    )
    frame = b'\x5A\x5A' + image
    table = struct.pack('<II', len(frame), 0)
    table_and_data = table + frame
    outer = b'\x89EAF' + struct.pack(
        '<III', 1, sum(table_and_data) & 0xFFFFFFFF, len(table_and_data)
    )
    path.write_bytes(outer + table_and_data)


def write_eaf4(path: Path) -> None:
    width = height = 16
    frame = bytearray()
    for y in range(height):
        for x in range(width):
            frame.extend((x * 12, y * 12, 0xD0, 0x60 + ((x + y) & 0x7F)))

    # Frame 0: FULL. Frame 1: change one 4x4 tile using packed XOR (mode 4).
    delta = bytes((0x20, 0x00)) + bytes((0, 0, 0x3F, 0)) * 16
    second = bytearray(frame)
    for y in range(4, 8):
        for x in range(4, 8):
            second[(y * width + x) * 4 + 2] ^= 0x3F
    raws = (bytes(frame), delta)
    rebuilt = (bytes(frame), bytes(second))
    payloads = [zlib.compress(raw, 6) for raw in raws]
    index = bytearray()
    offset = EAF4_HEADER.size + 2 * EAF4_INDEX.size
    for mode, raw, decoded, payload in zip((0, 4), raws, rebuilt, payloads):
        index += EAF4_INDEX.pack(
            offset, len(payload), len(raw), zlib.crc32(decoded) & 0xFFFFFFFF,
            mode, 0, 0, zlib.crc32(payload) & 0xFFFFFFFF,
        )
        offset += len(payload)
    header = bytearray(EAF4_HEADER.pack(
        b'EAF4', 1, EAF4_HEADER.size, width, height, 2, 67, len(frame),
        EAF4_HEADER.size, EAF4_HEADER.size + len(index), offset,
        4, 4, 0, zlib.crc32(index) & 0xFFFFFFFF, 1, bytes(12),
    ))
    struct.pack_into('<I', header, 40, zlib.crc32(header) & 0xFFFFFFFF)
    path.write_bytes(header + index + b''.join(payloads))



def write_jpeg(path: Path, unaligned: bool = False, direct: bool = False) -> None:
    red = base64.b64decode('/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAUDBAQEAwUEBAQFBQUGBwwIBwcHBw8LCwkMEQ8SEhEPERETFhwXExQaFRERGCEYGh0dHx8fExciJCIeJBweHx7/2wBDAQUFBQcGBw4ICA4eFBEUHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh7/wAARCAAQACADASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDymiiivjz+kAooooA//9k=')
    green = base64.b64decode('/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAUDBAQEAwUEBAQFBQUGBwwIBwcHBw8LCwkMEQ8SEhEPERETFhwXExQaFRERGCEYGh0dHx8fExciJCIeJBweHx7/2wBDAQUFBQcGBw4ICA4eFBEUHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh7/wAARCAAQACADASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwBlFFFfj5+DhRRRQB//2Q==')
    small = base64.b64decode('/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAUDBAQEAwUEBAQFBQUGBwwIBwcHBw8LCwkMEQ8SEhEPERETFhwXExQaFRERGCEYGh0dHx8fExciJCIeJBweHx7/2wBDAQUFBQcGBw4ICA4eFBEUHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh7/wAARCAAQABADASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDymiiivjz+kD//2Q==')
    direct_red = base64.b64decode('/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAUDBAQEAwUEBAQFBQUGBwwIBwcHBw8LCwkMEQ8SEhEPERETFhwXExQaFRERGCEYGh0dHx8fExciJCIeJBweHx7/2wBDAQUFBQcGBw4ICA4eFBEUHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh7/wAARCAAQADADASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDzGiiivgT+ogooooAKKKKAP//Z')
    def image(block: bytes, width: int) -> bytes:
        return b'\x5a\x5a_S\x0000V1.0' + bytes([24]) + struct.pack('<HHHHI', width, 16, 1, 16, len(block)) + block
    if direct:
        frames = [image(b'\x02' + direct_red, 48)]
    elif unaligned:
        frames = [image(b'\x02' + small, 16)]
    else:
        pos = 2
        while green[pos + 1] != 0xda:
            pos += 2 + int.from_bytes(green[pos + 2:pos + 4], 'big')
        scan = green[pos + 2 + int.from_bytes(green[pos + 2:pos + 4], 'big'):-2]
        # First 16x16 tile and top-left quadrant of the second tile change.
        frames = [image(b'\x02' + red, 32), image(b'\x06\x03\x1f' + scan, 32)]
    offset = 0
    table = bytearray()
    for f in frames:
        table += struct.pack('<II', len(f), offset)
        offset += len(f)
    body = bytes(table) + b''.join(frames)
    path.write_bytes(b'\x89EAF' + struct.pack('<III', len(frames), sum(body) & 0xffffffff, len(body)) + body)

def main() -> int:
    if len(sys.argv) not in (3, 5, 6):
        print(f'usage: {sys.argv[0]} LEGACY_EAF LOSSLESS_EAF4', file=sys.stderr)
        return 2
    legacy_path, eaf4_path = map(Path, sys.argv[1:3])
    legacy_path.parent.mkdir(parents=True, exist_ok=True)
    eaf4_path.parent.mkdir(parents=True, exist_ok=True)
    write_legacy(legacy_path)
    write_eaf4(eaf4_path)
    if len(sys.argv) >= 5:
        write_jpeg(Path(sys.argv[3]))
        write_jpeg(Path(sys.argv[4]), unaligned=True)
    if len(sys.argv) == 6:
        write_jpeg(Path(sys.argv[5]), direct=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
