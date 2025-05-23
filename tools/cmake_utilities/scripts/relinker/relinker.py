#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import logging
import argparse
import csv
import os
import subprocess
import sys
import re
from io import StringIO
import configuration

sys.path.append(os.environ['IDF_PATH'] + '/tools/ldgen')
sys.path.append(os.environ['IDF_PATH'] + '/tools/ldgen/ldgen')
from entity import EntityDB

espidf_objdump = None
espidf_version = None

def lib_secs(lib, file, lib_path):
    new_env = os.environ.copy()
    new_env['LC_ALL'] = 'C'
    try:
        dump_output = subprocess.check_output([espidf_objdump, '-h', lib_path], env=new_env).decode()
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Command '{e.cmd}' failed with exit code {e.returncode}")

    dump = StringIO(dump_output)
    dump.name = lib

    sections_infos = EntityDB()
    sections_infos.add_sections_info(dump)

    secs = sections_infos.get_sections(lib, file.split('.')[0] + '.c')
    if not secs:
        secs = sections_infos.get_sections(lib, file.split('.')[0])
        if not secs:
            raise ValueError(f'Failed to get sections from lib {lib_path}')

    return secs

def filter_secs(secs_a, secs_b):
    return [s_a for s_a in secs_a if any(s_b in s_a for s_b in secs_b)]

def strip_secs(secs_a, secs_b):
    secs = list(set(secs_a) - set(secs_b))
    secs.sort()
    return secs

def func2sect(func):
    if ' ' in func:
        func_l = func.split(' ')
    else:
        func_l = [func]

    secs = []
    for l in func_l:
        if '.iram1.' not in l:
            secs.append(f'.literal.{l}')
            secs.append(f'.text.{l}')
        else:
            secs.append(l)
    return secs

class filter_c:
    def __init__(self, file):
        with open(file) as f:
            lines = f.read().splitlines()
        self.libs_desc = ''
        self.libs = ''
        for line in lines:
            if ') .iram1 EXCLUDE_FILE(*' in line and ') .iram1.*)' in line:
                desc = r'\(EXCLUDE_FILE\((.*)\) .iram1 '
                self.libs_desc = re.search(desc, line)[1]
                self.libs = self.libs_desc.replace('*', '')
                return

    def match(self, lib):
        if lib in self.libs:
            print(f'Remove lib {lib}')
            return True
        return False

    def add(self):
        return self.libs_desc

class target_c:
    def __init__(self, lib, lib_path, file, fsecs):
        self.lib = lib
        self.file = file
        self.lib_path = lib_path
        self.fsecs = func2sect(fsecs)
        self.desc = f'*{lib}:{file.split(".")[0]}.*'

        secs = lib_secs(lib, file, lib_path)
        if '.iram1.' in self.fsecs[0]:
            self.secs = filter_secs(secs, ('.iram1.',))
        else:
            self.secs = filter_secs(secs, ('.iram1.', '.text.', '.literal.'))
        self.isecs = strip_secs(self.secs, self.fsecs)

        self.has_exclude_iram = True
        self.has_exclude_dram = True

    def __str__(self):
        return (
            f'lib={self.lib}\n'
            f'file={self.file}\n'
            f'lib_path={self.lib_path}\n'
            f'desc={self.desc}\n'
            f'secs={self.secs}\n'
            f'fsecs={self.fsecs}\n'
            f'isecs={self.isecs}\n'
        )

class target2_c:
    def __init__(self, lib, file, iram_secs):
        self.lib = lib
        self.file = file
        self.fsecs = iram_secs
        if file != '*':
            self.desc = f'*{lib}:{file.split(".")[0]}.*'
        else:
            self.desc = f'*{lib}:'
        self.isecs = iram_secs

        self.has_exclude_iram = False
        self.has_exclude_dram = False

    def __str__(self):
        return (
            f'lib={self.lib}\n'
            f'file={self.file}\n'
            f'lib_path={self.lib_path}\n'
            f'desc={self.desc}\n'
            f'secs={self.secs}\n'
            f'fsecs={self.fsecs}\n'
            f'isecs={self.isecs}\n'
        )

# Remove specific functions from IRAM
class relink_c:
    def __init__(self, input_file, library_file, object_file, function_file, sdkconfig_file, missing_function_info):
        self.filter = filter_c(input_file)

        libraries = configuration.generator(library_file, object_file, function_file, sdkconfig_file, missing_function_info, espidf_objdump)
        self.targets = []
        for lib_name in libraries.libs:
            lib = libraries.libs[lib_name]

            if self.filter.match(lib.name):
                continue

            for obj_name in lib.objs:
                obj = lib.objs[obj_name]
                target = target_c(lib.name, lib.path, obj.name, ' '.join(obj.sections()))
                self.targets.append(target)

        self.__transform__()

    def __transform__(self):
        iram1_exclude = []
        iram1_include = []
        flash_include = []

        for t in self.targets:
            secs = filter_secs(t.fsecs, ('.iram1.', ))
            if len(secs) > 0:
                iram1_exclude.append(t.desc)

            secs = filter_secs(t.isecs, ('.iram1.', ))
            if len(secs) > 0:
                iram1_include.append(f'    {t.desc}({" ".join(secs)})')

            secs = t.fsecs
            if len(secs) > 0:
                flash_include.append(f'    {t.desc}({" ".join(secs)})')

        self.iram1_exclude = f'    *(EXCLUDE_FILE({self.filter.add()} {" ".join(iram1_exclude)}) .iram1.*) *(EXCLUDE_FILE({self.filter.add()} {" ".join(iram1_exclude)}) .iram1)'
        self.iram1_include = '\n'.join(iram1_include)
        self.flash_include = '\n'.join(flash_include)

        logging.debug(f'IRAM1 Exclude: {self.iram1_exclude}')
        logging.debug(f'IRAM1 Include: {self.iram1_include}')
        logging.debug(f'Flash Include: {self.flash_include}')

    def __replace__(self, lines):
        def is_iram_desc(line):
            return '*(.iram1 .iram1.*)' in line or (') .iram1 EXCLUDE_FILE(*' in line and ') .iram1.*)' in line)

        iram_start = False
        flash_done = False
        flash_text = '(.stub)'

        if espidf_version == '5.0':
            flash_text = '(.stub .gnu.warning'

        new_lines = []
        for line in lines:
            if '.iram0.text :' in line:
                logging.debug('start to process .iram0.text')
                iram_start = True
                new_lines.append(line)
            elif '.dram0.data :' in line:
                logging.debug('end to process .iram0.text')
                iram_start = False
                new_lines.append(line)
            elif is_iram_desc(line):
                if iram_start:
                    new_lines.extend([self.iram1_exclude, self.iram1_include])
                else:
                    new_lines.append(line)
            elif flash_text in line:
                if not flash_done:
                    new_lines.extend([self.flash_include, line])
                    flash_done = True
                else:
                    new_lines.append(line)
            else:
                new_lines.append(self._replace_func(line) if iram_start else line)

        return new_lines

    def _replace_func(self, line):
        for t in self.targets:
            if t.desc in line:
                S = '.literal .literal.* .text .text.*'
                if S in line:
                    if len(t.isecs) > 0:
                        return line.replace(S, ' '.join(t.isecs))
                    else:
                        return ''

                S = f'{t.desc}({" ".join(t.fsecs)})'
                if S in line:
                    return ''

                replaced = False
                for s in t.fsecs:
                    s2 = s + ' '
                    if s2 in line:
                        line = line.replace(s2, '')
                        replaced = True
                    s2 = s + ')'
                    if s2 in line:
                        line = line.replace(s2, ')')
                        replaced = True
                if '( )' in line or '()' in line:
                    return ''
                if replaced:
                    return line

            index = f'*{t.lib}:(EXCLUDE_FILE'
            if index in line and t.file.split('.')[0] not in line:
                for m in self.targets:
                    index = f'*{m.lib}:(EXCLUDE_FILE'
                    if index in line and m.file.split('.')[0] not in line:
                        line = line.replace('EXCLUDE_FILE(', f'EXCLUDE_FILE({m.desc} ')
                        if len(m.isecs) > 0:
                            line += f'\n    {m.desc}({" ".join(m.isecs)})'
                return line

        return line

    def save(self, input_file, output_file, target):
        with open(input_file) as f:
            lines = f.read().splitlines()
        lines = self.__replace__(lines)
        with open(output_file, 'w+') as f:
            f.write('\n'.join(lines))

# Link specific functions to IRAM
class relink2_c:
    def __init__(self, input_file, library_file, object_file, function_file, sdkconfig_file, missing_function_info):
        self.filter = filter_c(input_file)

        rodata_exclude = []
        iram_exclude = []
        rodata = []
        iram = []

        libraries = configuration.generator(library_file, object_file, function_file, sdkconfig_file, missing_function_info, espidf_objdump)
        self.targets = []

        for lib_name in libraries.libs:
            lib = libraries.libs[lib_name]

            if self.filter.match(lib.name):
                continue

            for obj_name in lib.objs:
                obj = lib.objs[obj_name]
                if not obj.section_all:
                    self.targets.append(target_c(lib.name, lib.path, obj.name, ' '.join(obj.sections())))
                else:
                    self.targets.append(target2_c(lib.name, obj.name, obj.sections()))

        for target in self.targets:
            rodata.append(f'    {target.desc}(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)')
            if target.has_exclude_dram:
                rodata_exclude.append(target.desc)
            iram.append(f'    {target.desc}({" ".join(target.fsecs)})')
            if target.has_exclude_iram:
                iram_exclude.append(target.desc)

        self.rodata_ex = f'    *(EXCLUDE_FILE({" ".join(rodata_exclude)}) .rodata EXCLUDE_FILE({" ".join(rodata_exclude)}) .rodata.* EXCLUDE_FILE({" ".join(rodata_exclude)}) .sdata2 EXCLUDE_FILE({" ".join(rodata_exclude)}) .sdata2.* EXCLUDE_FILE({" ".join(rodata_exclude)}) .srodata EXCLUDE_FILE({" ".join(rodata_exclude)}) .srodata.*)'
        self.rodata_in = '\n'.join(rodata)

        self.iram_ex = f'    *(EXCLUDE_FILE({" ".join(iram_exclude)}) .iram1 EXCLUDE_FILE({" ".join(iram_exclude)}) .iram1.*)'
        self.iram_in = '\n'.join(iram)

        logging.debug(f'RODATA In: {self.rodata_in}')
        logging.debug(f'RODATA Ex: {self.rodata_ex}')
        logging.debug(f'IRAM In: {self.iram_in}')
        logging.debug(f'IRAM Ex: {self.iram_ex}')

    def save(self, input_file, output_file, target):
        with open(input_file) as f:
            lines = f.read().splitlines()
        lines = self.__replace__(lines, target)
        with open(output_file, 'w+') as f:
            f.write('\n'.join(lines))

    def __replace__(self, lines, target):
        iram_start = False
        new_lines = []
        iram_cache = []

        flash_text = '*(.stub)'
        iram_text = '*(.iram1 .iram1.*)'
        if espidf_version == '5.3' and target == 'esp32c2':
            iram_text = '_iram_text_start = ABSOLUTE(.);'
        elif espidf_version == '5.0':
            flash_text = '*(.stub .gnu.warning'

        for line in lines:
            if not iram_start:
                if iram_text in line:
                    new_lines.append(f'{self.iram_in}\n')
                    iram_start = True
                elif ' _text_start = ABSOLUTE(.);' in line:
                    new_lines.append(f'{line}\n{self.iram_ex}')
                    continue
                elif flash_text in line:
                    new_lines.extend(iram_cache)
                    iram_cache = []
            else:
                if '} > iram0_0_seg' in line:
                    iram_start = False

            if not iram_start:
                new_lines.append(line)
            else:
                skip_str = ['libriscv.a:interrupt', 'libriscv.a:vectors']
                if not any(s in line for s in skip_str):
                    iram_cache.append(line)

        return new_lines

def main():
    argparser = argparse.ArgumentParser(description='Relinker script generator')

    argparser.add_argument(
        '--input', '-i',
        help='Linker template file',
        type=str,
        required=True)

    argparser.add_argument(
        '--output', '-o',
        help='Output linker script',
        type=str,
        required=True)

    argparser.add_argument(
        '--library', '-l',
        help='Library description directory',
        type=str,
        required=True)

    argparser.add_argument(
        '--object', '-b',
        help='Object description file',
        type=str,
        required=True)

    argparser.add_argument(
        '--function', '-f',
        help='Function description file',
        type=str,
        required=True)

    argparser.add_argument(
        '--sdkconfig', '-s',
        help='sdkconfig file',
        type=str,
        required=True)

    argparser.add_argument(
        '--target', '-t',
        help='target chip',
        type=str,
        required=True)

    argparser.add_argument(
        '--version', '-v',
        help='IDF version',
        type=str,
        required=True)

    argparser.add_argument(
        '--objdump', '-g',
        help='GCC objdump command',
        type=str,
        required=True)

    argparser.add_argument(
        '--debug', '-d',
        help='Debug level (option is \'debug\')',
        default='no',
        type=str)

    argparser.add_argument(
        '--missing_function_info',
        help='Print error information instead of throwing exception when missing function',
        action='store_true')

    argparser.add_argument(
        '--link_to_iram',
        help='True: Link specific functions to IRAM, False: Remove specific functions from IRAM',
        action='store_true')

    args = argparser.parse_args()

    if args.debug == 'debug':
        logging.basicConfig(level=logging.DEBUG)

    logging.debug(f'input:    {args.input}')
    logging.debug(f'output:   {args.output}')
    logging.debug(f'library:  {args.library}')
    logging.debug(f'object:   {args.object}')
    logging.debug(f'function: {args.function}')
    logging.debug(f'version:  {args.version}')
    logging.debug(f'sdkconfig:{args.sdkconfig}')
    logging.debug(f'objdump:  {args.objdump}')
    logging.debug(f'debug:    {args.debug}')
    logging.debug(f'missing_function_info: {args.missing_function_info}')

    global espidf_objdump
    espidf_objdump = args.objdump

    global espidf_version
    espidf_version = args.version

    if not args.link_to_iram:
        relink = relink_c(args.input, args.library, args.object, args.function, args.sdkconfig, args.missing_function_info)
    else:
        relink = relink2_c(args.input, args.library, args.object, args.function, args.sdkconfig, args.missing_function_info)
    relink.save(args.input, args.output, args.target)

if __name__ == '__main__':
    main()
