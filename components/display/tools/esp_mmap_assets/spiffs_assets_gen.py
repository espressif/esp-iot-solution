# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import io
import os
import argparse
import shutil
import math
import sys
import time

sys.dont_write_bytecode = True

from PIL import Image
from datetime import datetime

header_file = 'assets_generate.h'

def compute_checksum(data):
    checksum = sum(data) & 0xFFFF
    return checksum

def sort_key(filename):
    basename, extension = os.path.splitext(filename)
    return extension, basename

def convert_image_to_simg(input_file):
    SPLIT_HEIGHT = 16
    OUTPUT_FILE_NAME = ''

    input_dir, input_filename = os.path.split(input_file)
    base_filename, ext = os.path.splitext(input_filename)
    OUTPUT_FILE_NAME = base_filename

    try:
        im = Image.open(input_file)
    except Exception as e:
        print('Error:', e)
        sys.exit(0)

    width, height = im.size

    lenbuf = []
    block_size = SPLIT_HEIGHT
    spilts = math.ceil(height/block_size)

    print('Input:')
    print('\t' + os.path.basename(input_file))
    print('\tRES = ' + str(width) + ' x ' + str(height))
    print('\tspilts = ', spilts)

    for i in range(spilts):
        if i < spilts - 1:
            crop = im.crop((0, i * block_size, width, (i + 1) * block_size))
        else:
            crop = im.crop((0, i * block_size, width, height))

        output_path = os.path.join(input_dir, str(i) + ext)
        crop.save(output_path, quality=90)

    sjpeg_data = bytearray()

    for i in range(spilts):
        with open(os.path.join(input_dir, str(i) + ext), 'rb') as f:
            a = f.read()
        sjpeg_data += a
        lenbuf.append(len(a))
        os.remove(os.path.join(input_dir, str(i) + ext))

    header = bytearray()

    if ext.lower() == '.jpg':
        header += bytearray('_SJPG__'.encode('UTF-8'))
    elif ext.lower() == '.png':
        header += bytearray('_SPNG__'.encode('UTF-8'))

    # 6 BYTES VERSION
    header += bytearray(('\x00V1.00\x00').encode('UTF-8'))

    # WIDTH 2 BYTES
    header += width.to_bytes(2, byteorder='little')

    # HEIGHT 2 BYTES
    header += height.to_bytes(2, byteorder='little')

    # NUMBER OF ITEMS 2 BYTES
    header += spilts.to_bytes(2, byteorder='little')

    # NUMBER OF ITEMS 2 BYTES
    header += SPLIT_HEIGHT.to_bytes(2, byteorder='little')

    for item_len in lenbuf:
        # WIDTH 2 BYTES
        header += item_len.to_bytes(2, byteorder='little')

    sjpeg = header + sjpeg_data

    if ext.lower() == '.jpg':
        output_file_path = os.path.join(input_dir, OUTPUT_FILE_NAME + '.sjpg')
    elif ext.lower() == '.png':
        output_file_path = os.path.join(input_dir, OUTPUT_FILE_NAME + '.spng')
    with open(output_file_path, 'wb') as f:
        f.write(sjpeg)

    print('Completed, saved as:', os.path.basename(output_file_path), '\n')

def pack_models(model_path, assets_c_path, out_file):
    merged_data = bytearray()
    file_info_list = []

    file_list = sorted(os.listdir(model_path), key=sort_key)
    for filename in file_list:
        file_path = os.path.join(model_path, filename)
        file_name = os.path.basename(file_path)
        file_size = os.path.getsize(file_path)

        try:
            img = Image.open(file_path)
            width, height = img.size
        except Exception as e:
            # print("Error:", e)
            _, file_extension = os.path.splitext(file_path)
            if file_extension.lower() in ['.sjpg', '.spng']:
                offset = 14
                with open(file_path, 'rb') as f:
                    f.seek(offset)
                    width_bytes = f.read(2)
                    height_bytes = f.read(2)
                    width = int.from_bytes(width_bytes, byteorder='little')
                    height = int.from_bytes(height_bytes, byteorder='little')
            else:
                width, height = 0, 0

        file_info_list.append((file_name, len(merged_data), file_size, width, height))

        with open(file_path, 'rb') as bin_file:
            bin_data = bin_file.read()

        merged_data.extend(bin_data)

    total_files = len(file_info_list)

    mmap_table = bytearray()
    for file_name, offset, file_size, width, height in file_info_list:
        fixed_name = file_name.ljust(32, '\0')[:32]
        mmap_table.extend(fixed_name.encode('utf-8'))
        mmap_table.extend(file_size.to_bytes(4, byteorder='little'))
        mmap_table.extend(offset.to_bytes(4, byteorder='little'))
        mmap_table.extend(width.to_bytes(4, byteorder='little'))
        mmap_table.extend(height.to_bytes(4, byteorder='little'))

    combined_data = mmap_table + merged_data
    combined_checksum = compute_checksum(combined_data)
    header_data = total_files.to_bytes(4, byteorder='little') + combined_checksum.to_bytes(4, byteorder='little')
    final_data = header_data + combined_data

    with open(out_file, 'wb') as output_bin:
        output_bin.write(final_data)

    os.makedirs(assets_c_path, exist_ok=True)
    current_year = datetime.now().year

    file_path = os.path.join(assets_c_path, header_file)
    with open(file_path, 'w') as output_header:
        output_header.write('/*\n')
        output_header.write(' * SPDX-FileCopyrightText: 2022-{} Espressif Systems (Shanghai) CO LTD\n'.format(current_year))
        output_header.write(' *\n')
        output_header.write(' * SPDX-License-Identifier: Apache-2.0\n')
        output_header.write(' */\n\n')
        output_header.write('/**\n')
        output_header.write(' * @file\n')
        output_header.write(" * @brief This file was generated by esp_mmap_assets, don't modify it\n")
        output_header.write(' */\n\n')
        output_header.write('#pragma once\n\n')
        output_header.write("#include \"esp_mmap_assets.h\"\n\n")
        output_header.write(f'#define TOTAL_MMAP_FILES      {total_files}\n')
        output_header.write(f'#define MMAP_CHECKSUM         0x{combined_checksum:04X}\n\n')
        output_header.write('enum MMAP_FILES {\n')

        for i, (file_name, _, _, _, _) in enumerate(file_info_list):
            enum_name = file_name.replace('.', '_')
            output_header.write(f'    MMAP_{enum_name.upper()} = {i},   /*!< {file_name} */\n')

        output_header.write('};\n')

    print(f'All bin files have been merged into {out_file}')


def parse_hex(value):
    try:
        return int(value, 16)
    except ValueError:
        raise argparse.ArgumentTypeError(f'Invalid hex value: {value}')

def copy_assets_to_build(assets_path, target_path, support_spng, support_sjpg, support_format):
    """
    Copy assets to target_path based on sdkconfig
    """
    format_string = support_format
    spng_enable = True if support_spng == 'ON' else False
    sjpg_enable = True if support_sjpg == 'ON' else False

    format_list = format_string.split(',')
    format_tuple = tuple(format_list)
    for filename in os.listdir(assets_path):
        if any(filename.endswith(suffix) for suffix in format_tuple):
            shutil.copyfile(os.path.join(assets_path, filename), os.path.join(target_path, filename))
            if filename.endswith('.jpg') and sjpg_enable:
                convert_image_to_simg(os.path.join(target_path, filename))
                os.remove(os.path.join(target_path, filename))
            elif filename.endswith('.png') and spng_enable:
                convert_image_to_simg(os.path.join(target_path, filename))
                os.remove(os.path.join(target_path, filename))
        else:
            print(f'No match found for file: {filename}, format_tuple: {format_tuple}')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Assert generator tool')
    parser.add_argument('-d1', '--project_path')
    parser.add_argument('-d2', '--main_path')
    parser.add_argument('-d3', '--assets_path')
    parser.add_argument('-d4', '--size', type=parse_hex)
    parser.add_argument('-d5', '--image_file')
    parser.add_argument('-d6', '--support_spng')
    parser.add_argument('-d7', '--support_sjpg')
    parser.add_argument('-d8', '--support_format')

    args = parser.parse_args()

    print('--support_spng:',  args.support_spng)
    print('--support_sjpg:',  args.support_sjpg)
    print('--support_format:',  args.support_format)

    image_file = args.image_file
    target_path = os.path.dirname(image_file)

    if os.path.exists(target_path):
        shutil.rmtree(target_path)
    os.makedirs(target_path)

    copy_assets_to_build(args.assets_path, target_path, args.support_spng, args.support_sjpg, args.support_format)
    pack_models(target_path, args.main_path, image_file)

    total_size = os.path.getsize(os.path.join(target_path, image_file))
    recommended_size = int(math.ceil(total_size/1024))
    partition_size = int(math.ceil(args.size/1024))

    if args.size <= total_size:
        print('Given assets partition size: %dK' % (partition_size))
        print('Recommended assets partition size: %dK' % (recommended_size))
        print('\033[1;31mError:\033[0m assets partition size is smaller than recommended.')
        sys.exit(1)
