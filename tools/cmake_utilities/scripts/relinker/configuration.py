#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import argparse
import csv
import os
import subprocess
import sys
import re
from io import StringIO

OPT_MIN_LEN = 7

espidf_objdump = None
espidf_missing_function_info = True

class sdkconfig_c:
    def __init__(self, path):
        with open(path) as f:
            lines = f.read().splitlines()
        config = {}
        for l in lines:
            if len(l) > OPT_MIN_LEN and l[0] != '#':
                mo = re.match(r'(.*)=(.*)', l, re.M | re.I)
                if mo:
                    config[mo.group(1)] = mo.group(2).replace('"', '')
        self.config = config

    def index(self, key):
        return self.config[key]

    def check(self, options):
        options = options.replace(' ', '')
        if '&&' in options:
            for i in options.split('&&'):
                if i[0] == '!':
                    i = i[1:]
                    if i in self.config:
                        return False
                else:
                    if i not in self.config:
                        return False
        else:
            i = options
            if i[0] == '!':
                i = i[1:]
                if i in self.config:
                    return False
            else:
                if i not in self.config:
                    return False
        return True

class object_c:
    def read_dump_info(self, paths):
        new_env = os.environ.copy()
        new_env['LC_ALL'] = 'C'
        dumps = []
        print('paths:', paths)
        for path in paths:
            try:
                dump_output = subprocess.check_output([espidf_objdump, '-t', path], env=new_env).decode()
                dumps.append(StringIO(dump_output).readlines())
            except subprocess.CalledProcessError as e:
                raise RuntimeError(f"Command '{e.cmd}' failed with exit code {e.returncode}")
        return dumps

    def get_func_section(self, dumps, func):
        for dump in dumps:
            for line in dump:
                if f' {func}' in line and '*UND*' not in line:
                    m = re.match(r'(\S*)\s*([glw])\s*([F|O])\s*(\S*)\s*(\S*)\s*(\S*)\s*', line, re.M | re.I)
                    if m and m[6] == func:
                        return m.group(4).replace('.text.', '')
        if espidf_missing_function_info:
            print(f'{func} failed to find section')
            return None
        else:
            raise RuntimeError(f'{func} failed to find section')

    def __init__(self, name, paths, library):
        self.name = name
        self.library = library
        self.funcs = {}
        self.paths = paths
        self.dumps = self.read_dump_info(paths)
        self.section_all = False

    def append(self, func):
        section = None

        if func == '.text.*':
            section = '.literal .literal.* .text .text.*'
            self.section_all = True
        elif func == '.iram1.*':
            section = '.iram1 .iram1.*'
            self.section_all = True
        elif func == '.wifi0iram.*':
            section = '.wifi0iram .wifi0iram.*'
            self.section_all = True
        elif func == '.wifirxiram.*':
            section = '.wifirxiram .wifirxiram.*'
            self.section_all = True
        else:
            section = self.get_func_section(self.dumps, func)

        if section is not None:
            self.funcs[func] = section

    def functions(self):
        nlist = []
        for i in self.funcs:
            nlist.append(i)
        return nlist

    def sections(self):
        nlist = []
        for i in self.funcs:
            nlist.append(self.funcs[i])
        return nlist

class library_c:
    def __init__(self, name, path):
        self.name = name
        self.path = path
        self.objs = {}

    def append(self, obj, path, func):
        if obj not in self.objs:
            self.objs[obj] = object_c(obj, path, self.name)
        self.objs[obj].append(func)

class libraries_c:
    def __init__(self):
        self.libs = {}

    def append(self, lib, lib_path, obj, obj_path, func):
        if lib not in self.libs:
            self.libs[lib] = library_c(lib, lib_path)
        self.libs[lib].append(obj, obj_path, func)

    def dump(self):
        for libname in self.libs:
            lib = self.libs[libname]
            for objname in lib.objs:
                obj = lib.objs[objname]
                print(f'{libname}, {objname}, {obj.path}, {obj.funcs}')

class paths_c:
    def __init__(self):
        self.paths = {}

    def append(self, lib, obj, path):
        if '$IDF_PATH' in path:
            path = path.replace('$IDF_PATH', os.environ['IDF_PATH'])

        if lib not in self.paths:
            self.paths[lib] = {}
        if obj not in self.paths[lib]:
            self.paths[lib][obj] = []
        self.paths[lib][obj].append(path)

    def index(self, lib, obj):
        if lib not in self.paths:
            return None
        if '*' in self.paths[lib]:
            obj = '*'
        return self.paths[lib].get(obj)

def generator(library_file, object_file, function_file, sdkconfig_file, missing_function_info, objdump='riscv32-esp-elf-objdump'):
    global espidf_objdump, espidf_missing_function_info
    espidf_objdump = objdump
    espidf_missing_function_info = missing_function_info

    sdkconfig = sdkconfig_c(sdkconfig_file)

    lib_paths = paths_c()
    with open(library_file, newline='') as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            lib_paths.append(row['library'], '*', row['path'])

    obj_paths = paths_c()
    with open(object_file, newline='') as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            obj_paths.append(row['library'], row['object'], row['path'])

    libraries = libraries_c()
    with open(function_file, newline='') as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            if row['option'] and not sdkconfig.check(row['option']):
                print(f'skip {row["function"]}({row["option"]})')
                continue
            lib_path = lib_paths.index(row['library'], '*')
            obj_path = obj_paths.index(row['library'], row['object'])
            if not obj_path:
                obj_path = lib_path
            libraries.append(row['library'], lib_path[0], row['object'], obj_path, row['function'])
    return libraries

def main():
    argparser = argparse.ArgumentParser(description='Libraries management')

    argparser.add_argument(
        '--library', '-l',
        help='Library description file',
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

    args = argparser.parse_args()

    libraries = generator(args.library, args.object, args.function, args.sdkconfig, espidf_missing_function_info)
    # libraries.dump()

if __name__ == '__main__':
    main()
