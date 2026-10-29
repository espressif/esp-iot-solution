#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0


import logging
import argparse
import os
import subprocess
import sys
import struct
from io import StringIO

def parse_int(v, keywords={}):
    for letter, multiplier in [('k', 1024), ('m', 1024 * 1024)]:
        if v.lower().endswith(letter):
            return parse_int(v[:-1], keywords) * multiplier
    return int(v, 0)

def relink(args):
    logging.debug('input:       %s'%(args.input))
    logging.debug('output:      %s'%(args.output))
    logging.debug('components:  %s'%(args.components))

    found = False
    lines = open(args.input).read().splitlines()
    for i in range(0, len(lines)):
        l = lines[i]
        if '    _text_start = ABSOLUTE(.);' in l:
            components_list = args.components.split()

            link_desc = '    _gprof_text_start = ABSOLUTE(.);\n'
            for c in components_list:
                link_desc += '    *lib%s.a:(.text .text.* .literal .literal.*)\n'%(c)
            link_desc += '    _gprof_text_end = ABSOLUTE(.);'
            lines[i] = l + '\n\n' + link_desc
            found = True
            break

    if found == False:
        raise Exception('text start index is not found')

    open(args.output, 'w+').write('\n'.join(lines))

def build_esptool_cmd():
    esptool_cmd = [sys.executable, '-m', 'esptool', '--after', 'no-reset']
    port = os.environ.get('ESPPORT') or os.environ.get('IDF_PORT')
    if port:
        esptool_cmd.extend(['--port', port])
    return esptool_cmd

def read_flash(esptool_cmd, offset, size, output, env):
    cmd = [*esptool_cmd, 'read_flash', str(offset), str(size), output]
    print('cmd: ', cmd)
    subprocess.check_output(cmd, env=env)

def validate_gprof_size(size, max_size):
    if size == 0xFFFFFFFF:
        raise RuntimeError(
            'GProf partition appears erased. Run the profiling app first and, '
            'if multiple boards are connected, set ESPPORT or IDF_PORT to the target port.'
        )
    if size == 0:
        raise RuntimeError('GProf partition contains empty profile data. Run the profiling app first.')
    if max_size is not None and size > max_size:
        raise RuntimeError(
            'Invalid GProf data size: %d bytes exceeds the gprof partition payload capacity of %d bytes'
            % (size, max_size)
        )

def dump(args):
    logging.debug('elf:             %s'%(args.elf))
    logging.debug('output:          %s'%(args.output))
    logging.debug('gcc:             %s'%(args.gcc))
    logging.debug('graphic:         %s'%(args.graphic))

    offset = parse_int(args.partition_offset, {'x': 16})
    logging.debug('partition offset: %x'%(offset))
    partition_size = parse_int(args.partition_size) if args.partition_size else None
    logging.debug('partition size:   %s'%('unknown' if partition_size is None else '%x'%(partition_size)))

    READ_LEN = 4096
    HEADER_LEN = 16
    new_env = os.environ.copy()
    new_env['LC_ALL'] = 'C'
    esptool_cmd = build_esptool_cmd()
    read_flash(esptool_cmd, offset, READ_LEN, args.output, new_env)

    buf = open(args.output, 'rb').read()
    if len(buf) < HEADER_LEN:
        raise RuntimeError('Failed to read GProf partition header')
    size = struct.unpack('I', buf[0:4])[0]
    validate_gprof_size(size, None if partition_size is None else partition_size - HEADER_LEN)
    if size > READ_LEN - HEADER_LEN:
        read_flash(esptool_cmd, offset + HEADER_LEN, size, args.output, new_env)
        buf = open(args.output, 'rb').read()
    else:
        buf = buf[HEADER_LEN:size + HEADER_LEN]
    open(args.output, 'wb').write(buf)

    objcopy = args.gcc.replace('gcc', 'objcopy')
    cmd = [objcopy, args.elf, '--rename-section', '.flash.text=.text']
    StringIO(subprocess.check_output(cmd, env=new_env).decode())

    gprof = args.gcc.replace('gcc', 'gprof')
    if not args.graphic:
        cmd = [gprof, args.elf, '-b', args.output]
        dump_info = StringIO(subprocess.check_output(cmd, env=new_env).decode())
        print(dump_info.getvalue())
    else:
        proj_name = args.elf.replace('.elf', '')
        png = proj_name + '.gprof.png'
        png_data = '.' + png + '.bin'

        cmd = [gprof, args.elf, '-b', args.output]
        dump_info = StringIO(subprocess.check_output(cmd, env=new_env).decode())
        open(png_data, 'w').write(dump_info.getvalue())

        cmd = ['gprof2dot', '-n0', '-e0', png_data]
        dump_info = StringIO(subprocess.check_output(cmd, env=new_env).decode())
        open(png_data, 'w').write(dump_info.getvalue())

        cmd = ['dot', '-Tpng', '-o', png, png_data]
        dump_info = StringIO(subprocess.check_output(cmd, env=new_env).decode())

def main():
    argparser = argparse.ArgumentParser(description='Gprof profile data parser')

    argparser.add_argument(
        '--debug', '-d',
        help='Debug level(option is \'debug\')',
        default='no',
        type=str)

    subparsers = argparser.add_subparsers(
        dest='operation')

    parser_relink = subparsers.add_parser(
        'relink',
        help='Transform link script')

    parser_relink.add_argument(
        '--input', '-i',
        help='Linker template file',
        type=str
    )

    parser_relink.add_argument(
        '--output', '-o',
        help='Output linker script',
        type=str)

    parser_relink.add_argument(
        '--components', '-c',
        help='Components for GProf',
        type=str)

    parser_dump = subparsers.add_parser(
        'dump',
        help='Dump gprof information')

    parser_dump.add_argument(
        '--elf',
        help='Porject ELF file',
        type=str
    )

    parser_dump.add_argument(
        '--output',
        help='Output gprof file',
        type=str
    )

    parser_dump.add_argument(
        '--gcc',
        help='GCC toolchain',
        type=str
    )

    parser_dump.add_argument(
        '--graphic',
        help='Generate graphical output',
        action='store_true',
        default=False
    )

    parser_dump.add_argument(
        '--partition-offset',
        help='Offset of the gprof partition, in bytes',
        type=str  # accept hex strings
    )

    parser_dump.add_argument(
        '--partition-size',
        help='Size of the gprof partition, in bytes',
        type=str  # accept hex strings
    )

    args = argparser.parse_args()

    if args.debug == 'debug':
        logging.basicConfig(level=logging.DEBUG)

    operation_func = globals()[args.operation]
    operation_func(args)

if __name__ == '__main__':
    main()
