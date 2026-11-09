#!/usr/bin/env python
#
# SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import sys
import struct
import argparse
import binascii
import hashlib
import os
import subprocess
import shutil
import json
import lzma

# src_file = 'hello-world.bin'
# compressed_file = 'hello-world.bin.xz'
# packed_compressed_file = 'hello-world.bin.xz.packed'
# siged_packed_compressed_file = 'hello-world.bin.xz.packed.signed'

binary_compress_type = {'none': 0, 'xz':1}
binary_delta_type = {'none': 0, 'ddelta': 1}
header_version = {'v1': 1, 'v2': 2, 'v3': 3}
# CRC of this prefix of the base firmware is stored in the v3 reserved field.
BASE_APP_CHECK_SIZE = 4096

SCRIPT_VERSION = '1.0.0'
ORIGIN_APP_IMAGE_HEADER_LEN = 288   # sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t). See esp_app_format.h
# esp_https_ota (IDF 5.x/6.x) reads IMAGE_HEADER_SIZE (1024) bytes before the
# response may finish. A short ddelta patch can be smaller than that, so pad the
# final downloadable file. Trailing bytes are outside header.length / MD5.
MIN_HTTPS_OTA_IMAGE_SIZE = 1024
# At present, we calculate the checksum of the first 4KB data of the old app.
OLD_APP_CHECK_DATA_SIZE = 4096

# v1 compressed data header:
# Note: Encryption_type field is deprecated, the field is reserved for compatibility.
# |------|---------|---------|----------|------------|------------|-----------|----------|----------------|----------------|--------------|------------|
# |      | Magic   | header  | Compress | delta      | Encryption | Reserved  | Firmware | The length of  | The MD5 of     | The CRC32 for| compressed |
# |      | number  | version | type     | type       | type       |           | version  | compressed data| compressed data| the header   | data       |
# |------|---------|---------|----------|------------|------------|-----------|----------|----------------|----------------|--------------|------------|
# | Size | 4 bytes | 1 byte  | 4 bits	| 4 bits     | 1 bytes	  | 1 bytes   | 32 bytes |  4 bytes	      | 32 bytes	   |  4 bytes     |            |
# |------|---------|---------|----------|------------|------------|-----------|----------|----------------|----------------|--------------|------------|
# | Data | String  |         |          |            |            |           | String   | little-endian  |   byte string  | little-endian|            |
# | type | ended   |         |          |            |            |           | ended    |   integer      |                |    integer   |            |
# |      |with ‘\0’|         |          |            |            |           | with ‘\0’|                |                |              | Binary data|
# |------|---------|---------|----------|------------|------------|-----------|----------|----------------|----------------|--------------|------------|
# | Data | “ESP”   |    1    |   0/1    | 0/1        |	          |           |          |                |                |              |            |
# |------|---------|---------|----------|------------|------------|-----------|----------|----------------|----------------|--------------|------------|

# v2 compressed data header
# Note: Encryption_type field is deprecated, the field is reserved for compatibility.
# |------|---------|---------|----------|------------|------------|-----------|----------|----------------|----------------|---------------|---------------|--------------|------------|
# |      | Magic   | header  | Compress | delta      | Encryption | Reserved  | Firmware | The length of  | The MD5 of     | base app check| The CRC32 for | The CRC32 for| compressed |
# |      | number  | version | type     | type       | type       |           | version  | compressed data| compressed data| data len      | base app data | the header   | data       |
# |------|---------|---------|----------|------------|------------|-----------|----------|----------------|----------------|---------------|---------------|--------------|------------|
# | Size | 4 bytes | 1 byte  | 4 bits	| 4 bits     | 1 bytes	  | 1 bytes   | 32 bytes |  4 bytes	      | 32 bytes	   |  4 bytes      |  4 bytes      |  4 bytes     |            |
# |------|---------|---------|----------|------------|------------|-----------|----------|----------------|----------------|---------------|---------------|--------------|------------|
# | Data | String  |         |          |            |            |           | String   | little-endian  |   byte string  | little-endian | little-endian | little-endian|            |
# | type | ended   |         |          |            |            |           | ended    |   integer      |                | integer       |    integer    |    integer   |            |
# |      |with ‘\0’|         |          |            |            |           | with ‘\0’|                |                |               |               |              | Binary data|
# |------|---------|---------|----------|------------|------------|-----------|----------|----------------|----------------|---------------|---------------|--------------|------------|
# | Data | “ESP”   |    1    |   0/1    | 0/1        |	          |           |          |                |                |               |               |              |            |
# |------|---------|---------|----------|------------|------------|-----------|----------|----------------|----------------|---------------|---------------|--------------|------------|

# v3 compressed data header:
# |------|---------|---------|----------|------------|-----------|----------------|----------------|--------------|------------|
# |      | Magic   | header  | Compress | Reserved   | Reserved  | The length of  | The MD5 of     | The CRC32 for| compressed |
# |      | number  | version | type     |            |           | compressed data| compressed data| the header   | data       |
# |------|---------|---------|----------|------------|-----------|----------------|----------------|--------------|------------|
# | Size | 4 bytes | 1 byte  | 4 bits	| 4 bits     | 8+2 bytes |  4 bytes	      |   16 bytes	   |  4 bytes     |            |
# delta_type occupies the high 4 bits of the compress byte (0=none, 1=ddelta).
# The 10-byte reserved field is: uint32 base_len, uint32 base_crc32, 2 zero bytes.
# Pure compression keeps the historical reserved filler so existing v3 images stay valid.
# |------|---------|---------|----------|------------|-----------|----------------|----------------|--------------|------------|
# | Data | String  | integer |          |            |           | little-endian  |   byte string  | little-endian|            |
# | type | ended   |         |          |            |           |   integer      |                |    integer   |            |
# |      |with ‘\0’|         |          |            |           |                |                |              | Binary data|
# |------|---------|---------|----------|------------|-----------|----------------|----------------|--------------|------------|
# | Data | “ESP”   |     3   |   0/1    |            |           |                |                |              |            |
# |------|---------|---------|----------|------------|-----------|----------------|----------------|--------------|------------|

def xz_compress(store_directory, in_file):
    compressed_file = ''.join([in_file,'.xz'])
    if(os.path.exists(compressed_file)):
        os.remove(compressed_file)

    xz_compressor_filter = [
        {'id': lzma.FILTER_LZMA2, 'preset': 6, 'dict_size': 64*1024},
    ]
    with open(in_file, 'rb') as src_f:
        data = src_f.read()
        with lzma.open(compressed_file, 'wb', format=lzma.FORMAT_XZ, check=lzma.CHECK_CRC32, filters=xz_compressor_filter) as f:
            f.write(data)
            f.close()

    if not os.path.exists(os.path.join(store_directory, os.path.split(compressed_file)[1])):
        shutil.copy(compressed_file, store_directory)
        print('copy xz file done')

def secure_boot_sign(sign_key, in_file, out_file):
    ret = subprocess.call('espsecure.py sign_data --version 2 --keyfile {} --output {} {}'.format(sign_key, out_file, in_file), shell = True)
    if ret:
        raise Exception('sign failed')

def get_app_name():
    with open('flasher_args.json') as f:
        try:
            flasher_args = json.load(f)
            return flasher_args['app']['file']
        except Exception as e:
            print(e)

    return ''

def get_script_version():
    return SCRIPT_VERSION

def _esp_ddelta_usable(module):
    return module is not None and hasattr(module, 'encode_file')

def _drop_unusable_esp_ddelta():
    loaded = sys.modules.get('esp_ddelta')
    if loaded is None or _esp_ddelta_usable(loaded):
        return
    for name in list(sys.modules):
        if name == 'esp_ddelta' or name.startswith('esp_ddelta.'):
            del sys.modules[name]

def import_esp_ddelta():
    # Prefer an extension already installed for this interpreter. The script
    # directory is sys.path[0] and contains an esp_ddelta source tree, which
    # would otherwise shadow site-packages or PYTHONPATH.
    loaded = sys.modules.get('esp_ddelta')
    if _esp_ddelta_usable(loaded):
        return loaded

    script_dir = os.path.abspath(os.path.dirname(__file__))
    pkg_root = os.path.join(script_dir, 'esp_ddelta')
    package_init = os.path.join(pkg_root, 'esp_ddelta', '__init__.py')
    shadowed = {script_dir, os.path.abspath(pkg_root)}
    saved_path = sys.path[:]
    sys.path = [entry for entry in sys.path if os.path.abspath(entry or os.curdir) not in shadowed]
    try:
        _drop_unusable_esp_ddelta()
        try:
            import esp_ddelta
        except ImportError:
            esp_ddelta = None
        if _esp_ddelta_usable(esp_ddelta):
            return esp_ddelta
    finally:
        sys.path = saved_path

    # Not installed. Fall back to the source tree next to this script.
    _drop_unusable_esp_ddelta()
    if os.path.isfile(package_init) and pkg_root not in sys.path:
        sys.path.insert(0, pkg_root)
    try:
        import esp_ddelta
    except ImportError as exc:
        raise ImportError(
            'esp_ddelta is not built for this Python. Install it with: '
            'python -m pip install -e tools/cmake_utilities/scripts/esp_ddelta'
        ) from exc
    if not _esp_ddelta_usable(esp_ddelta):
        raise ImportError(
            'esp_ddelta at {} has no encode_file. The extension module _core was not built for this Python. '
            'Install it with: python -m pip install -e tools/cmake_utilities/scripts/esp_ddelta'.format(
                getattr(esp_ddelta, '__file__', esp_ddelta))
        )
    return esp_ddelta

def base_app_crc(base_file):
    with open(base_file, 'rb') as base_f:
        data = base_f.read(BASE_APP_CHECK_SIZE)
    if len(data) == 0:
        raise Exception('base firmware is empty')
    return len(data), binascii.crc32(data) & 0xffffffff

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-hv', '--header_ver', nargs='?', choices = ['v1', 'v2', 'v3'],
            default='v3', help='the version of the packed file header [default:v3]')
    parser.add_argument('-c', '--compress_type', nargs= '?', choices = ['none', 'xz'],
            default='xz', help='compressed type [default:xz]')
    parser.add_argument('-d', '--delta_type', nargs='?', choices=['none', 'ddelta'],
            default='none', help='delta type [default:none]. ddelta requires header v3 and --base_file')
    parser.add_argument('-b', '--base_file', nargs='?',
            default='', help='base firmware currently on the device (required for ddelta)')
    parser.add_argument('-i', '--in_file', nargs = '?',
            default='', help='the new app firmware')
    parser.add_argument('--sign_key', nargs = '?',
            default='', help='the sign key used for secure boot')
    parser.add_argument('-fv', '--fw_ver', nargs='?',
            default='', help='the version of the compressed data(this field is deprecated in v3)')
    parser.add_argument('--add_app_header', action='store_true', help='add app header to use native esp_ota_* & esp_https_ota_* APIs')
    parser.add_argument('--pad_https_ota', action='store_true',
            help='if the final file is shorter than 1024 bytes, pad it with zeros so esp_https_ota can read IMAGE_HEADER_SIZE')
    parser.add_argument('-v', '--version', action='version', version=get_script_version(), help='the version of the script')

    args = parser.parse_args()

    compress_type = args.compress_type
    delta_type = args.delta_type
    base_file = args.base_file
    firmware_ver = args.fw_ver
    src_file = args.in_file
    sign_key = args.sign_key
    header_ver = args.header_ver
    add_app_header = args.add_app_header
    pad_https_ota = args.pad_https_ota

    if delta_type != 'none' and header_ver != 'v3':
        raise Exception('ddelta packing uses v3 reserved fields; pass -hv v3')
    if delta_type != 'none' and base_file == '':
        raise Exception('ddelta requires --base_file')

    if src_file == '':
        origin_app_name = get_app_name()
        if(origin_app_name == ''):
            print('get origin app name fail')
            return

        if os.path.exists(origin_app_name):
            src_file = origin_app_name
        else:
            print('origin app.bin not found')
            return

    print('src file is: {}'.format(src_file))
    new_app_file = src_file

    # rebuild the cpmpressed_app directory
    cpmoressed_app_directory = 'custom_ota_binaries'
    if os.path.exists(cpmoressed_app_directory):
        shutil.rmtree(cpmoressed_app_directory)

    os.mkdir(cpmoressed_app_directory)
    print('The compressed file will store in {}'.format(cpmoressed_app_directory))

    if delta_type == 'ddelta':
        esp_ddelta = import_esp_ddelta()
        uncompressed_patch = os.path.join(cpmoressed_app_directory, 'patch')
        esp_ddelta.encode_file(base_file, new_app_file, uncompressed_patch)
        src_file = uncompressed_patch
        print('uncompressed ddelta patch: {}'.format(uncompressed_patch))

    #step1: compress
    if compress_type == 'xz':
        xz_compress(cpmoressed_app_directory, os.path.abspath(src_file))

        origin_app_name = os.path.split(src_file)[1]

        compressed_file_name = ''.join([origin_app_name, '.xz'])
        compressed_file = os.path.join(cpmoressed_app_directory, compressed_file_name)
    else:
        compressed_file = ''.join(src_file)

    print('compressed file is: {}'.format(compressed_file))

    #step2: packet the compressed image header
    # App header always comes from the new firmware, not from the patch payload.
    with open(new_app_file, 'rb') as s_f:
        src_image_header = bytearray(s_f.read(ORIGIN_APP_IMAGE_HEADER_LEN))
        src_image_header[1] = 0x00

    packed_file = ''.join([compressed_file,'.packed'])
    with open(compressed_file, 'rb') as src_f:
        data = src_f.read()
        f_len = src_f.tell()
        # magic number
        bin_data = struct.pack('4s', b'ESP')
        # header version
        bin_data += struct.pack('B', header_version[header_ver])
        # Compress type in the low 4 bits, delta type in the high 4 bits.
        type_byte = (binary_delta_type[delta_type] << 4) | binary_compress_type[compress_type]
        bin_data += struct.pack('B', type_byte)
        print('compressed type: {}'.format(type_byte))
        # in header v1/v2, there is a field "Encryption type", this field has been deprecated in v3
        if (header_version[header_ver] < 3):
            bin_data += struct.pack('B', 0)
            # Reserved
            bin_data += struct.pack('?', 0)
            # Firmware version
            bin_data += struct.pack('32s', firmware_ver.encode())
        else:
            # 10-byte reserved field. Pure compression keeps the historical filler.
            # ddelta stores base_len (u32) + base_crc32 (u32) + 2 zero bytes.
            if delta_type == 'ddelta':
                base_len, base_crc = base_app_crc(base_file)
                bin_data += struct.pack('<II', base_len, base_crc)
                bin_data += struct.pack('2s', b'\x00\x00')
                print('base check len: {}, crc32: {:08x}'.format(base_len, base_crc))
            else:
                bin_data += struct.pack('10s', b'0')
        # The length of the compressed data
        bin_data += struct.pack('<I', f_len)
        print('compressed data len: {}'.format(f_len))
        # The MD5 for the compressed data
        if (header_version[header_ver] < 3):
            bin_data += struct.pack('32s', hashlib.md5(data).digest())
            if (header_version[header_ver] == 2):
                # Todo, if it's diff OTA, write base app check data len
                bin_data += struct.pack('<I', 0)
                # Todo, if it's diff OTA, write base app crc32 checksum
                bin_data += struct.pack('<I', 0)
        else:
            bin_data += struct.pack('16s', hashlib.md5(data).digest())
        # The CRC32 for the header
        bin_data += struct.pack('<I', binascii.crc32(bin_data, 0x0))

        # write compressed data
        bin_data += data
        with open(packed_file, 'wb') as dst_f:
            # write compressed image header and compressed dada
            dst_f.write(bin_data)

    print('packed file is: {}'.format(packed_file))

    #step3: if need sign, then sign the packed image
    if sign_key != '':
        signed_file = ''.join([packed_file,'.signed'])
        secure_boot_sign(sign_key, packed_file, signed_file)
        print('signed_file is: {}'.format(signed_file))
    else:
        signed_file = ''.join(packed_file)

    if (header_version[header_ver] == 3) and add_app_header:
        with open(signed_file, 'rb+') as src_f:
            packed_data = src_f.read()
            src_f.seek(0)
            # write origin app image header
            src_f.write(src_image_header)
            # write compressed image header and compressed dada
            src_f.write(packed_data)
            print('app image header has been added')

    # Optional. Trailing zeros sit outside header.length / MD5.
    if pad_https_ota:
        final_size = os.path.getsize(signed_file)
        if final_size < MIN_HTTPS_OTA_IMAGE_SIZE:
            pad_len = MIN_HTTPS_OTA_IMAGE_SIZE - final_size
            with open(signed_file, 'ab') as dst_f:
                dst_f.write(b'\x00' * pad_len)
            print('padded {} with {} zero bytes for esp_https_ota (final size {})'.format(
                signed_file, pad_len, MIN_HTTPS_OTA_IMAGE_SIZE))

if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        print(e)
        sys.exit(2)
