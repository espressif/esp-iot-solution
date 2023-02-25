#!/usr/bin/env python3
#
# Copyright 2022 Espressif Systems (Shanghai) PTE LTD
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import csv
import os
import subprocess
import sys
import re
from io import StringIO

OPT_MIN_LEN = 7

espidf_objdump = None

class sdkconfig_c:
    def __init__(self, path):
        lines = open(path).read().splitlines()
        config = dict()
        for l in lines:
            if len(l) > OPT_MIN_LEN and l[0] != '#':
                mo = re.match( r'(.*)=(.*)', l, re.M|re.I)
                if mo:
                    config[mo.group(1)]=mo.group(2).replace('"', '')
        self.config = config
    
    def index(self, i):
        return self.config[i]
    
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
    def read_dump_info(self, path):
        new_env = os.environ.copy()
        new_env['LC_ALL'] = 'C'
        try:
            dump = StringIO(subprocess.check_output([espidf_objdump, '-t', path], env=new_env).decode())
            return dump.readlines()
        except subprocess.CalledProcessError as e:
            raise RuntimeError('cmd:%s result:%s'%(e.cmd, e.returncode))

    def get_func_section(self, dump, func):
        for l in dump:
            if ' %s'%(func) in l and '*UND*' not in l:
                m = re.match(r'(\S*)\s*([glw])\s*([F|O])\s*(\S*)\s*(\S*)\s*(\S*)\s*', l, re.M|re.I)
                if m and m[6] == func:
                    return m[4].replace('.text.', '')
        raise RuntimeError('%s failed to find section'%(func))

    def __init__(self, name, path, libray):
        self.name = name
        self.path = path
        self.libray = libray
        self.dump = self.read_dump_info(self.path)
        self.funcs = dict()
    
    def append(self, func):
        self.funcs[func] = self.get_func_section(self.dump, func)
    
    def functions(self):
        nlist = list()
        for i in self.funcs:
            nlist.append(i)
        return nlist
    
    def sections(self):
        nlist = list()
        for i in self.funcs:
            nlist.append(self.funcs[i])
        return nlist

class library_c:
    def __init__(self, name, path):
        self.name = name
        self.path = path
        self.objs = dict()

    def append(self, obj, path, func):
        if obj not in self.objs:
            self.objs[obj] = object_c(obj, path, self.name)
        self.objs[obj].append(func)

class libraries_c:
    def __init__(self):
        self.libs = dict()

    def append(self, lib, lib_path, obj, obj_path, func):
        if lib not in self.libs:
            self.libs[lib] = library_c(lib, lib_path)
        self.libs[lib].append(obj, obj_path, func)
    
    def dump(self):
        for libname in self.libs:
            lib = self.libs[libname]
            for objname in lib.objs:
                obj = lib.objs[objname]
                print('%s, %s, %s, %s'%(libname, objname, obj.path, obj.funcs))

class paths_c:
    def __init__(self):
        self.paths = dict()
    
    def append(self, lib, obj, path):
        if '$IDF_PATH' in path:
            path = path.replace('$IDF_PATH', os.environ['IDF_PATH'])

        if lib not in self.paths:
            self.paths[lib] = dict()
        self.paths[lib][obj] = path
    
    def index(self, lib, obj):
        if lib not in self.paths:
            return None
        if '*' in self.paths[lib]:
            obj = '*'
        return self.paths[lib][obj]

def generator(library_file, object_file, function_file, sdkconfig_file, objdump='riscv32-esp-elf-objdump'):
    global espidf_objdump 
    espidf_objdump = objdump

    sdkconfig = sdkconfig_c(sdkconfig_file)

    lib_paths = paths_c()
    for p in csv.DictReader(open(library_file, 'r')):
        lib_paths.append(p['library'], '*', p['path'])

    obj_paths = paths_c()
    for p in csv.DictReader(open(object_file, 'r')):
        obj_paths.append(p['library'], p['object'], p['path'])

    libraries = libraries_c()
    for d in csv.DictReader(open(function_file, 'r')):
        if d['option'] and sdkconfig.check(d['option']) == False:
            print('skip %s(%s)'%(d['function'], d['option']))
            continue
        lib_path = lib_paths.index(d['library'], '*')
        obj_path = obj_paths.index(d['library'], d['object'])
        if not obj_path:
            obj_path = lib_path
        libraries.append(d['library'], lib_path, d['object'], obj_path, d['function'])
    return libraries

def main():
    argparser = argparse.ArgumentParser(description='Libraries management')

    argparser.add_argument(
        '--library', '-l',
        help='Library description file',
        type=str)

    argparser.add_argument(
        '--object', '-b',
        help='Object description file',
        type=str)

    argparser.add_argument(
        '--function', '-f',
        help='Function description file',
        type=str)

    argparser.add_argument(
        '--sdkconfig', '-s',
        help='sdkconfig file',
        type=str)

    args = argparser.parse_args()

    libraries = generator(args.library, args.object, args.function, args.sdkconfig)
    # libraries.dump()

if __name__ == '__main__':
    main()
