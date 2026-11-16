# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Parse and validate EAF/AAF files using the device decoder layout."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
import argparse
import json
import struct
from typing import Any

from encoder.names import CODEC_NAMES

EAF_MAGIC_HEAD = 0x5A5A
EAF_MAGIC_LEN = 2
EAF_FORMAT_MAGIC = 0x89
EAF_FORMAT_STRINGS = {b'EAF', b'AAF'}

EAF_FORMAT_OFFSET = 0
EAF_STR_OFFSET = 1
EAF_NUM_OFFSET = 4
EAF_CHECKSUM_OFFSET = 8
EAF_TABLE_LEN_OFFSET = 12
EAF_TABLE_OFFSET = 16

FRAME_VERSION_OFFSET = 3
FRAME_BIT_DEPTH_OFFSET = 9
FRAME_WIDTH_OFFSET = 10
FRAME_HEIGHT_OFFSET = 12
FRAME_BLOCKS_OFFSET = 14
FRAME_BLOCK_HEIGHT_OFFSET = 16
FRAME_BLOCK_LEN_TABLE_OFFSET = 18
FRAME_BLOCK_LEN_SIZE = 4
FRAME_PALETTE_ENTRY_SIZE = 4

BIT_DEPTHS = {4, 8, 24}
ENCODING_NAMES = CODEC_NAMES
DEFAULT_ISSUE_FRAME_LIMIT = 20


class Severity:
    INFO = 'info'
    WARNING = 'warning'
    ERROR = 'error'


@dataclass
class Issue:
    severity: str
    where: str
    message: str

    def to_dict(self) -> dict[str, str]:
        return {'severity': self.severity, 'where': self.where, 'message': self.message}


@dataclass
class BlockInfo:
    index: int
    offset: int
    length: int
    encoding: int | None = None
    encoding_name: str = 'UNKNOWN'
    decoded_size: int | None = None
    expected_size: int | None = None
    decode_capacity: int | None = None
    decode_ok: bool | None = None
    issues: list[Issue] = field(default_factory=list)

    def to_dict(self) -> dict[str, Any]:
        return {
            'index': self.index,
            'offset': self.offset,
            'length': self.length,
            'encoding': self.encoding,
            'encoding_name': self.encoding_name,
            'decoded_size': self.decoded_size,
            'expected_size': self.expected_size,
            'decode_capacity': self.decode_capacity,
            'decode_ok': self.decode_ok,
            'issues': [issue.to_dict() for issue in self.issues],
        }


@dataclass
class FrameInfo:
    index: int
    table_offset: int
    frame_offset: int
    frame_size: int
    abs_offset: int
    payload_offset: int
    payload_size: int
    magic: int | None = None
    frame_type: str = 'invalid'
    format: str = ''
    version: str = ''
    bit_depth: int | None = None
    width: int | None = None
    height: int | None = None
    blocks: int | None = None
    block_height: int | None = None
    num_colors: int = 0
    header_size: int | None = None
    block_lengths: list[int] = field(default_factory=list)
    palette_size: int = 0
    data_offset: int | None = None
    blocks_info: list[BlockInfo] = field(default_factory=list)
    issues: list[Issue] = field(default_factory=list)

    def to_dict(self) -> dict[str, Any]:
        return {
            'index': self.index,
            'table_offset': self.table_offset,
            'frame_offset': self.frame_offset,
            'frame_size': self.frame_size,
            'abs_offset': self.abs_offset,
            'payload_offset': self.payload_offset,
            'payload_size': self.payload_size,
            'magic': self.magic,
            'frame_type': self.frame_type,
            'format': self.format,
            'version': self.version,
            'bit_depth': self.bit_depth,
            'width': self.width,
            'height': self.height,
            'blocks': self.blocks,
            'block_height': self.block_height,
            'num_colors': self.num_colors,
            'header_size': self.header_size,
            'block_lengths': self.block_lengths,
            'palette_size': self.palette_size,
            'data_offset': self.data_offset,
            'blocks_info': [block.to_dict() for block in self.blocks_info],
            'issues': [issue.to_dict() for issue in self.issues],
        }


@dataclass
class EafFile:
    path: str
    size: int
    format_magic: int | None = None
    format_string: str = ''
    total_frames: int | None = None
    stored_checksum: int | None = None
    calculated_checksum: int | None = None
    table_len: int | None = None
    frames: list[FrameInfo] = field(default_factory=list)
    issues: list[Issue] = field(default_factory=list)

    @property
    def ok(self) -> bool:
        return not any(issue.severity == Severity.ERROR for issue in self.all_issues())

    def all_issues(self) -> list[Issue]:
        issues = list(self.issues)
        for frame in self.frames:
            issues.extend(frame.issues)
            for block in frame.blocks_info:
                issues.extend(block.issues)
        return issues

    def to_dict(self) -> dict[str, Any]:
        return {
            'ok': self.ok,
            'path': self.path,
            'size': self.size,
            'format_magic': self.format_magic,
            'format_string': self.format_string,
            'total_frames': self.total_frames,
            'animation_frame_count_device': max((self.total_frames or 0) - 1, 0),
            'stored_checksum': self.stored_checksum,
            'calculated_checksum': self.calculated_checksum,
            'table_len': self.table_len,
            'frames': [frame.to_dict() for frame in self.frames],
            'issues': [issue.to_dict() for issue in self.all_issues()],
        }


def add_issue(target: EafFile | FrameInfo | BlockInfo, severity: str, where: str, message: str) -> None:
    target.issues.append(Issue(severity, where, message))


def u16le(data: bytes, offset: int) -> int:
    return struct.unpack_from('<H', data, offset)[0]


def u32le(data: bytes, offset: int) -> int:
    return struct.unpack_from('<I', data, offset)[0]


def checksum(data: bytes) -> int:
    return sum(data) & 0xFFFFFFFF


def rle_decode(data: bytes, expected_size: int) -> tuple[bytes, list[str]]:
    out = bytearray()
    errors: list[str] = []
    if len(data) % 2 != 0:
        errors.append('RLE 数据长度为奇数；设备端会忽略最后 1 字节')
    pos = 0
    while pos + 1 < len(data):
        repeat_count = data[pos]
        repeat_value = data[pos + 1]
        pos += 2
        if len(out) + repeat_count > expected_size:
            errors.append(f'RLE 解压越界：{len(out) + repeat_count} > {expected_size}')
            break
        out.extend([repeat_value] * repeat_count)
    return bytes(out), errors


class HuffmanNode:
    __slots__ = ('left', 'right', 'symbol')

    def __init__(self) -> None:
        self.left: HuffmanNode | None = None
        self.right: HuffmanNode | None = None
        self.symbol: int | None = None


def huffman_decode(data: bytes, expected_size: int) -> tuple[bytes, list[str]]:
    errors: list[str] = []
    if len(data) < 3:
        return b'', ['Huffman 数据太短，至少需要 2 字节 dict_size 和 1 字节 dict padding']

    dict_size = data[0] | (data[1] << 8)
    if len(data) < 2 + dict_size:
        return b'', [f'Huffman 字典越界：dict_size={dict_size}, 输入长度={len(data)}']

    dict_data = data[2 : 2 + dict_size]
    encoded = data[2 + dict_size :]
    if dict_size == 0:
        return b'', ['Huffman 字典为空']

    if not encoded:
        symbol_count = 0
        single_symbol = 0
        pos = 1
        while pos < len(dict_data):
            if pos + 2 > len(dict_data):
                errors.append('Huffman 单色字典条目不完整')
                break
            symbol = dict_data[pos]
            code_len = dict_data[pos + 1]
            pos += 2
            code_byte_len = (code_len + 7) // 8
            if pos + code_byte_len > len(dict_data):
                errors.append('Huffman 单色字典 code 越界')
                break
            pos += code_byte_len
            symbol_count += 1
            single_symbol = symbol
            if symbol_count > 1:
                break
        if symbol_count == 1:
            return bytes([single_symbol]) * expected_size, errors
        return b'', errors + ['Huffman encoded 数据为空，但字典不是单色字典']

    padding_bits = dict_data[0]
    if padding_bits > 7:
        errors.append(f'Huffman padding_bits 异常：{padding_bits}')

    root = HuffmanNode()
    pos = 1
    while pos < len(dict_data):
        if pos + 2 > len(dict_data):
            errors.append('Huffman 字典条目不完整')
            break
        symbol = dict_data[pos]
        code_len = dict_data[pos + 1]
        pos += 2
        if code_len == 0 or code_len > 64:
            errors.append(f'Huffman code_len 异常：{code_len}')
            break
        code_byte_len = (code_len + 7) // 8
        if pos + code_byte_len > len(dict_data):
            errors.append('Huffman 字典 code 越界')
            break
        code = int.from_bytes(dict_data[pos : pos + code_byte_len], 'big')
        pos += code_byte_len

        node = root
        for bit_pos in range(code_len - 1, -1, -1):
            if (code >> bit_pos) & 1:
                if node.right is None:
                    node.right = HuffmanNode()
                node = node.right
            else:
                if node.left is None:
                    node.left = HuffmanNode()
                node = node.left
        if node.symbol is not None:
            errors.append(f'Huffman code 重复，symbol={symbol}')
        node.symbol = symbol

    total_bits = len(encoded) * 8 - padding_bits
    if total_bits < 0:
        return b'', errors + ['Huffman padding_bits 大于 encoded 位数']

    out = bytearray()
    node = root
    for bit_index in range(total_bits):
        byte_idx = bit_index // 8
        bit_offset = 7 - (bit_index % 8)
        bit_val = (encoded[byte_idx] >> bit_offset) & 1
        node = node.right if bit_val else node.left
        if node is None:
            errors.append(f'Huffman 路径无效，bit={bit_index}')
            break
        if node.symbol is not None:
            out.append(node.symbol)
            if len(out) > expected_size:
                errors.append(f'Huffman 输出超过期望：{len(out)} > {expected_size}')
                break
            node = root
    return bytes(out), errors


def decode_block_payload(encoding: int, payload: bytes, expected_size: int) -> tuple[int, bool | None, list[str]]:
    if encoding == 0:
        decoded, errors = rle_decode(payload, expected_size)
        return len(decoded), not errors, errors
    if encoding == 1:
        tmp, errors = huffman_decode(payload, expected_size * 2)
        decoded, rle_errors = rle_decode(tmp, expected_size)
        errors.extend(rle_errors)
        return len(decoded), not errors, errors
    if encoding == 3:
        decoded, errors = huffman_decode(payload, expected_size)
        return len(decoded), not errors, errors
    if encoding in (2, 4, 5, 6, 7):
        return 0, None, []
    return 0, False, [f'未知 encoding type：0x{encoding:02X}']


def parse_frame(data: bytes, index: int, table_offset: int, frame_offset: int, frame_size: int,
                payload_base: int, verify_blocks: bool) -> FrameInfo:
    abs_offset = payload_base + frame_offset
    payload_offset = abs_offset + EAF_MAGIC_LEN
    payload_size = frame_size - EAF_MAGIC_LEN
    frame = FrameInfo(
        index=index,
        table_offset=table_offset,
        frame_offset=frame_offset,
        frame_size=frame_size,
        abs_offset=abs_offset,
        payload_offset=payload_offset,
        payload_size=payload_size,
    )

    where = f'frame[{index}]'
    if frame_size < EAF_MAGIC_LEN:
        add_issue(frame, Severity.ERROR, where, f'frame_size 太小：{frame_size}')
        return frame
    if abs_offset < payload_base:
        add_issue(frame, Severity.ERROR, where, f'frame_offset 回绕或指向 table 区域：{frame_offset}')
        return frame
    if abs_offset + frame_size > len(data):
        add_issue(frame, Severity.ERROR, where, f'帧越过文件尾：end={abs_offset + frame_size}, file_size={len(data)}')
        available = max(0, len(data) - abs_offset)
    else:
        available = frame_size

    if available < EAF_MAGIC_LEN:
        return frame

    frame.magic = u16le(data, abs_offset)
    if frame.magic != EAF_MAGIC_HEAD:
        add_issue(frame, Severity.ERROR, where, f'帧 magic 错误：0x{frame.magic:04X}，期望 0x{EAF_MAGIC_HEAD:04X}')
        return frame

    payload = data[payload_offset : min(payload_offset + payload_size, len(data))]
    if len(payload) < 2:
        add_issue(frame, Severity.ERROR, where, 'payload 太短，无法读取 frame format')
        return frame

    frame.format = payload[:2].decode('ascii', 'replace')
    if payload[:2] == b'_C':
        frame.frame_type = 'flag'
        return frame
    if payload[:2] != b'_S':
        add_issue(frame, Severity.ERROR, where, f'frame format 无效：{payload[:2]!r}，期望 _S 或 _C')
        return frame

    frame.frame_type = 'image'
    min_header = FRAME_BLOCK_LEN_TABLE_OFFSET
    if len(payload) < min_header:
        add_issue(frame, Severity.ERROR, where, f'_S header 太短：{len(payload)} < {min_header}')
        return frame

    frame.version = payload[FRAME_VERSION_OFFSET : FRAME_VERSION_OFFSET + 6].decode('ascii', 'replace')
    frame.bit_depth = payload[FRAME_BIT_DEPTH_OFFSET]
    frame.width = u16le(payload, FRAME_WIDTH_OFFSET)
    frame.height = u16le(payload, FRAME_HEIGHT_OFFSET)
    frame.blocks = u16le(payload, FRAME_BLOCKS_OFFSET)
    frame.block_height = u16le(payload, FRAME_BLOCK_HEIGHT_OFFSET)

    if frame.bit_depth not in BIT_DEPTHS:
        add_issue(frame, Severity.ERROR, where, f'bit_depth 无效：{frame.bit_depth}')
    if not frame.width or not frame.height or not frame.blocks or not frame.block_height:
        add_issue(
            frame,
            Severity.ERROR,
            where,
            f'width/height/blocks/block_height 不能为 0：{frame.width}x{frame.height}, blocks={frame.blocks}, block_height={frame.block_height}',
        )
        return frame

    expected_blocks = (frame.height + frame.block_height - 1) // frame.block_height
    if frame.blocks != expected_blocks:
        add_issue(frame, Severity.WARNING, where, f'blocks={frame.blocks} 与 height/block_height 推导值 {expected_blocks} 不一致')

    frame.num_colors = 0 if frame.bit_depth == 24 else 1 << frame.bit_depth
    frame.palette_size = frame.num_colors * FRAME_PALETTE_ENTRY_SIZE
    block_table_size = frame.blocks * FRAME_BLOCK_LEN_SIZE
    frame.header_size = FRAME_BLOCK_LEN_TABLE_OFFSET + block_table_size + frame.palette_size
    frame.data_offset = frame.header_size

    if frame.header_size > len(payload):
        add_issue(frame, Severity.ERROR, where, f'帧 header 越过 payload：header_size={frame.header_size}, payload_size={len(payload)}')
        return frame

    for block_index in range(frame.blocks):
        pos = FRAME_BLOCK_LEN_TABLE_OFFSET + block_index * FRAME_BLOCK_LEN_SIZE
        frame.block_lengths.append(u32le(payload, pos))

    data_size = len(payload) - frame.data_offset
    block_sum = sum(frame.block_lengths)
    if block_sum != data_size:
        severity = Severity.ERROR if block_sum > data_size else Severity.WARNING
        add_issue(frame, severity, where, f'block_len 总和 {block_sum} 与实际 block data 大小 {data_size} 不一致')

    offset = frame.data_offset
    for block_index, block_len in enumerate(frame.block_lengths):
        block_where = f'frame[{index}].block[{block_index}]'
        block = BlockInfo(index=block_index, offset=offset, length=block_len)
        frame.blocks_info.append(block)
        if block_len == 0:
            block.decode_ok = True  # Empty delta block preserves its reference pixels.
            continue
        if offset + block_len > len(payload):
            add_issue(block, Severity.ERROR, block_where, f'block 越过 payload：end={offset + block_len}, payload_size={len(payload)}')
            offset += block_len
            continue
        block_data = payload[offset : offset + block_len]
        block.encoding = block_data[0]
        block.encoding_name = ENCODING_NAMES.get(block.encoding, 'UNKNOWN')
        if block.encoding not in ENCODING_NAMES:
            add_issue(block, Severity.ERROR, block_where, f'未知 codec ID：0x{block.encoding:02X}')
        valid_height = frame.block_height
        if (block_index + 1) * frame.block_height > frame.height:
            valid_height = frame.height - block_index * frame.block_height
        expected_size = frame.width * valid_height
        decode_capacity = frame.width * frame.block_height
        if frame.bit_depth == 24:
            expected_size *= 2 if block.encoding == 2 else 3
            decode_capacity *= 2 if block.encoding == 2 else 3
        elif frame.bit_depth == 4:
            expected_size = (expected_size + 1) // 2
            decode_capacity = (decode_capacity + 1) // 2
        block.expected_size = expected_size
        block.decode_capacity = decode_capacity
        if verify_blocks and block.encoding in ENCODING_NAMES:
            decoded_size, decode_ok, errors = decode_block_payload(block.encoding, block_data[1:], decode_capacity)
            block.decoded_size = decoded_size
            block.decode_ok = decode_ok
            for error in errors:
                severity = Severity.WARNING if decode_ok is None else Severity.ERROR
                add_issue(block, severity, block_where, error)
            if decode_ok is True and decoded_size < expected_size:
                add_issue(block, Severity.WARNING, block_where, f'解码输出大小 {decoded_size} 小于有效像素大小 {expected_size}')
            if decode_ok is True and decoded_size > decode_capacity:
                add_issue(block, Severity.ERROR, block_where, f'解码输出大小 {decoded_size} 超过设备端 block buffer {decode_capacity}')
        offset += block_len

    return frame


def parse_bytes(data: bytes, path: str = '<memory>', verify_blocks: bool = True) -> EafFile:
    info = EafFile(path=path, size=len(data))
    if len(data) < EAF_TABLE_OFFSET:
        add_issue(info, Severity.ERROR, 'file', f'文件太短：{len(data)} < {EAF_TABLE_OFFSET}')
        return info

    info.format_magic = data[EAF_FORMAT_OFFSET]
    info.format_string = data[EAF_STR_OFFSET : EAF_STR_OFFSET + 3].decode('ascii', 'replace')
    info.total_frames = struct.unpack_from('<i', data, EAF_NUM_OFFSET)[0]
    info.stored_checksum = u32le(data, EAF_CHECKSUM_OFFSET)
    info.table_len = u32le(data, EAF_TABLE_LEN_OFFSET)

    if info.format_magic != EAF_FORMAT_MAGIC:
        add_issue(info, Severity.ERROR, 'file.header', f'format magic 错误：0x{info.format_magic:02X}，期望 0x{EAF_FORMAT_MAGIC:02X}')
    if data[EAF_STR_OFFSET : EAF_STR_OFFSET + 3] not in EAF_FORMAT_STRINGS:
        add_issue(info, Severity.ERROR, 'file.header', f'format string 错误：{info.format_string!r}，期望 EAF 或 AAF')
    if info.total_frames is None or info.total_frames <= 0:
        add_issue(info, Severity.ERROR, 'file.header', f'total_frames 无效：{info.total_frames}')
        return info
    if info.total_frames > 10000:
        add_issue(info, Severity.WARNING, 'file.header', f'total_frames 异常偏大：{info.total_frames}')

    table_bytes = info.total_frames * 8
    if EAF_TABLE_OFFSET + table_bytes > len(data):
        add_issue(info, Severity.ERROR, 'frame_table', f'frame table 越过文件尾：end={EAF_TABLE_OFFSET + table_bytes}, file_size={len(data)}')
        return info

    if info.table_len is None:
        return info
    if EAF_TABLE_OFFSET + info.table_len > len(data):
        add_issue(info, Severity.ERROR, 'file.header', f'table_len 越过文件尾：end={EAF_TABLE_OFFSET + info.table_len}, file_size={len(data)}')
        checksum_region = data[EAF_TABLE_OFFSET:]
    else:
        checksum_region = data[EAF_TABLE_OFFSET : EAF_TABLE_OFFSET + info.table_len]
    info.calculated_checksum = checksum(checksum_region)
    if info.calculated_checksum != info.stored_checksum:
        add_issue(info, Severity.ERROR, 'file.header', f'checksum 不匹配：stored=0x{info.stored_checksum:08X}, calc=0x{info.calculated_checksum:08X}')

    payload_base = EAF_TABLE_OFFSET + table_bytes
    for index in range(info.total_frames):
        table_offset = EAF_TABLE_OFFSET + index * 8
        frame_size = u32le(data, table_offset)
        frame_offset = u32le(data, table_offset + 4)
        frame = parse_frame(data, index, table_offset, frame_offset, frame_size, payload_base, verify_blocks)
        info.frames.append(frame)

    covered_end = payload_base
    for frame in info.frames:
        covered_end = max(covered_end, frame.abs_offset + max(frame.frame_size, 0))
    expected_end = EAF_TABLE_OFFSET + info.table_len
    if info.table_len and covered_end != expected_end:
        add_issue(info, Severity.WARNING, 'file.layout', f'frame 覆盖结束位置 {covered_end} 与 table_len 结束位置 {expected_end} 不一致')
    if len(data) > expected_end:
        add_issue(info, Severity.INFO, 'file.layout', f'文件尾有 {len(data) - expected_end} 字节不在 checksum/table_len 范围内')

    return info


def parse_file(path: str | Path, verify_blocks: bool = True) -> EafFile:
    source = Path(path)
    return parse_bytes(source.read_bytes(), str(source), verify_blocks=verify_blocks)


def inspect_file(path: str | Path, verify_blocks: bool = True) -> dict[str, Any]:
    return parse_file(path, verify_blocks=verify_blocks).to_dict()


def frame_has_issues(frame: FrameInfo) -> bool:
    return bool(frame.issues) or any(block.issues for block in frame.blocks_info)


def format_values(values: list[str]) -> str:
    return ','.join(values) if values else '-'


def print_field(name: str, value: str | int | None) -> None:
    print(f"  {name:<18}: {value if value is not None else '-'}")


def print_text(info: EafFile, verbose: bool, print_frames: bool) -> None:
    status = 'OK' if info.ok else 'FAIL'
    print(f'{status} {info.path}')
    image_frames = sum(1 for frame in info.frames if frame.frame_type == 'image')
    flag_frames = sum(1 for frame in info.frames if frame.frame_type == 'flag')
    sizes = sorted({f'{frame.width}x{frame.height}' for frame in info.frames if frame.frame_type == 'image'})
    bit_depths = sorted({f'{frame.bit_depth}bit' for frame in info.frames if frame.frame_type == 'image'})
    encodings = sorted({block.encoding_name for frame in info.frames for block in frame.blocks_info})
    print_field('format', info.format_string or '-')
    print_field('bytes', info.size)
    print_field('frames', info.total_frames or 0)
    print_field('device_frames', max((info.total_frames or 0) - 1, 0))
    print_field('image_frames', image_frames)
    print_field('flag_frames', flag_frames)
    print_field('size', format_values(sizes))
    print_field('depth', format_values(bit_depths))
    print_field('encoding', format_values(encodings))
    if info.stored_checksum is not None:
        print_field('checksum_stored', f'0x{info.stored_checksum:08X}')
        print_field('checksum_calc', f'0x{(info.calculated_checksum or 0):08X}')
        print_field('table_len', info.table_len)
    for issue in info.issues:
        print(f'  [{issue.severity}] {issue.where}: {issue.message}')
    printed_issue_frames = 0
    hidden_issue_frames = 0
    for frame in info.frames:
        has_issues = frame_has_issues(frame)
        if not (verbose or print_frames or has_issues):
            continue
        if not verbose and not print_frames and has_issues and printed_issue_frames >= DEFAULT_ISSUE_FRAME_LIMIT:
            hidden_issue_frames += 1
            continue
        if has_issues:
            printed_issue_frames += 1
        line = f'  frame[{frame.index}] off={frame.abs_offset} size={frame.frame_size} type={frame.frame_type}'
        if frame.frame_type == 'image':
            encodings = ','.join(sorted({block.encoding_name for block in frame.blocks_info}))
            line += f' {frame.width}x{frame.height} {frame.bit_depth}bit blocks={frame.blocks} bh={frame.block_height} enc={encodings}'
        print(line)
        if verbose:
            for issue in frame.issues:
                print(f'    [{issue.severity}] {issue.where}: {issue.message}')
            for block in frame.blocks_info:
                print(
                    f'    block[{block.index}] off={block.offset} len={block.length} '
                    f'enc={block.encoding_name} decoded={block.decoded_size}/{block.expected_size} '
                    f'cap={block.decode_capacity} ok={block.decode_ok}'
                )
                for issue in block.issues:
                    print(f'      [{issue.severity}] {issue.where}: {issue.message}')
        else:
            for issue in frame.issues:
                print(f'    [{issue.severity}] {issue.where}: {issue.message}')
            for block in frame.blocks_info:
                for issue in block.issues:
                    print(f'    [{issue.severity}] {issue.where}: {issue.message}')
    if hidden_issue_frames:
        print(f'  ... hidden {hidden_issue_frames} more issue frame(s), use -v or --frames for full details')


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description='Inspect EAF/AAF files using the ESP device decoder layout.')
    parser.add_argument('files', nargs='+', help='EAF/AAF file(s) to inspect')
    parser.add_argument('--json', action='store_true', help='print JSON report')
    parser.add_argument('--no-block-decode', action='store_true', help='skip RLE/Huffman block decode checks')
    parser.add_argument('--frames', action='store_true', help='print every frame summary')
    parser.add_argument('-v', '--verbose', action='store_true', help='print per-block details')
    args = parser.parse_args(argv)

    reports = []
    exit_code = 0
    for file_path in args.files:
        try:
            info = parse_file(file_path, verify_blocks=not args.no_block_decode)
        except OSError as exc:
            info = EafFile(path=file_path, size=0)
            add_issue(info, Severity.ERROR, 'file', str(exc))
        reports.append(info)
        if not info.ok:
            exit_code = 1

    if args.json:
        payload: Any = [report.to_dict() for report in reports]
        if len(payload) == 1:
            payload = payload[0]
        print(json.dumps(payload, ensure_ascii=False, indent=2))
    else:
        for report in reports:
            print_text(report, args.verbose, args.frames)

    return exit_code


if __name__ == '__main__':
    raise SystemExit(main())
