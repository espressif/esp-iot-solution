#!/usr/bin/env python
#
# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import logging
import os
import argparse
import sys
import re
import subprocess

def get_global_symbols(lines, type, undefined=False, symbol_types=None):
    """
    Extract global symbols from the given lines of a symbol table.

    :param lines: List of lines from the symbol table, each representing a symbol.
    :param type: Type of the input file ('e' for ELF files, 'l' for static libraries).
    :param undefined: If True, only extract undefined (UND) symbols; otherwise, extract all GLOBAL symbols.
    :param symbol_types: A list of symbol types to filter by (e.g., ['FUNC', 'OBJECT']). If None, no filtering by type.
    :return: List of symbol names if any match the criteria; otherwise, returns an empty list.
    """
    symbols = []

    if type == 'e':
        # Pattern for ELF files
        if not undefined:
            pattern = re.compile(
                r'^\s*\d+:\s+(\S+)\s+(\d+)\s+(FUNC|OBJECT)\s+GLOBAL\s+DEFAULT\s+(?:\d+|ABS|UND|COM|DEBUG)\s+(\S+)',
                re.MULTILINE
            )
        else:
            pattern = re.compile(
                r'^\s*\d*:\s*\w*\s*\d*\s*NOTYPE\s*GLOBAL\s*DEFAULT\s*UND\s+(\S*)',
                re.MULTILINE
            )

        for line in lines:
            match = pattern.match(line)
            if match:
                if not undefined:
                    address, size, symbol_type, symbol_name = match.groups()

                    # Filter by symbol type if specified
                    if symbol_types and symbol_type not in symbol_types:
                        continue

                    symbols.append(symbol_name)
                else:
                    symbol_name = match.group(1)
                    symbols.append(symbol_name)

    elif type == 'l':
        # Patterns for static libraries
        func_pattern = re.compile(r'^\s*[0-9a-fA-F]+\s+[TD]\s+(\S+)$')
        var_pattern = re.compile(r'^\s*[0-9a-fA-F]+\s+[BD]\s+(\S+)$')

        for line in lines:
            if not undefined:
                func_match = func_pattern.match(line)
                var_match = var_pattern.match(line)

                if func_match:
                    symbols.append(func_match.group(1))
                elif var_match:
                    symbols.append(var_match.group(1))

    return symbols

def save_c_file(symbols, output, symbol_table, exclude_symbols=None):
    """
    Write extern declarations and ESP_ELFSYM structure to a C file, excluding specified symbols.

    :param symbols: List of symbol names.
    :param output: Path to the output C file.
    :param exclude_symbols: List of symbol names to exclude; defaults to ['elf_find_sym'].
    """
    if exclude_symbols is None:
        exclude_symbols = ['elf_find_sym']  # Set default excluded symbols

    # Filter out excluded symbols
    filtered_symbols = [name for name in symbols if name not in exclude_symbols]

    # Build the content of the C file
    buf = '/*\n'
    buf += ' * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD\n'
    buf += ' *\n'
    buf += ' * SPDX-License-Identifier: Apache-2.0\n'
    buf += ' */\n\n'

    # Add standard library headers
    libc_headers = ['stddef']  # Add more headers as needed
    buf += '\n'.join([f'#include <{h}.h>' for h in libc_headers]) + '\n\n'

    # Add custom header
    buf += '#include "private/elf_symbol.h"\n\n'

    # Generate extern declarations if there are symbols
    if filtered_symbols:
        buf += '/* Extern declarations from ELF symbol table */\n\n'
        buf += '#pragma GCC diagnostic push\n'
        buf += '#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"\n'
        for symbol_name in filtered_symbols:
            buf += f'extern int {symbol_name};\n'
        buf += '#pragma GCC diagnostic pop\n\n'

    # Define the symbol table structure with dynamic variable name
    symbol_table_var = f'g_{symbol_table}_elfsyms'
    buf += f'/* Available ELF symbols table: {symbol_table_var} */\n'
    buf += f'\nconst struct esp_elfsym {symbol_table_var}[] = {{\n'

    # Generate ESP_ELFSYM_EXPORT entries
    if filtered_symbols:
        for symbol_name in filtered_symbols:
            buf += f'    ESP_ELFSYM_EXPORT({symbol_name}),\n'

    # End the symbol table
    buf += '    ESP_ELFSYM_END\n'
    buf += '};\n'

    # Write to the file
    with open(output, 'w+') as f:
        f.write(buf)

def main():
    """
    Main function to parse command-line arguments and process the input file's symbol table.
    """
    parser = argparse.ArgumentParser(description='Extract all public functions from an application project', prog='symbols')

    parser.add_argument(
        '--output-file', '-of',
        help='Custom output file path with filename (overrides --output)',
        default=None
    )

    parser.add_argument(
        '--input', '-i',
        help='Input file name with full path',
        required=True
    )

    parser.add_argument(
        '--undefined', '-u',
        action='store_true',
        help='If set, only extract undefined (UND) symbols; otherwise, extract all GLOBAL symbols.',
        default=False
    )

    parser.add_argument(
        '--exclude', '-e',
        nargs='+',
        help='Symbols to exclude from the generated C file (e.g., memcpy __ltdf2). Default: elf_find_sym',
        default=[]  # User can extend this list
    )

    parser.add_argument(
        '--type', '-t',
        choices=['e', 'l'],
        required=True,
        help='Type of the input file: "elf" for ELF file, "lib" for static library (.a)'
    )

    parser.add_argument(
        '--debug', '-d',
        help='Debug level(option is \'debug\')',
        default='no',
        type=str)

    args = parser.parse_args()

    # Configure logging
    if args.debug == 'debug':
        logging.basicConfig(level=logging.DEBUG, format='%(asctime)s - %(levelname)s - %(message)s')
    else:
        logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

    # Get the absolute path of the current file
    cur_dir = os.path.dirname(os.path.abspath(__file__))

    if args.type == 'e':
        extracted_part = 'customer'
    elif args.type == 'l':
        # Convert relative path to absolute path
        input_abs_path = os.path.abspath(args.input)

        # Get the base name of the input file (without extension)
        input_basename = os.path.basename(input_abs_path)

        # Use a regular expression to extract the middle part
        match = re.search(r'^lib(.+)\.a$', input_basename)
        if match:
            extracted_part = match.group(1)
        else:
            logging.error('Invalid input file name format. Expected format: lib<name>.a')
            sys.exit(1)

    # Determine the output file path
    if args.output_file:
        # Use the custom file path provided by the user
        elfsym_file_dir = os.path.abspath(args.output_file)
        output_dir = os.path.dirname(elfsym_file_dir)
        output_abs_path = os.path.abspath(args.output_file)
        output_basename = os.path.basename(output_abs_path)
        extracted_part = os.path.splitext(output_basename)[0]
    else:
        # Use the default behavior: generate the file in the parent directory's 'src' folder
        parent_dir = os.path.dirname(cur_dir)
        output_dir = os.path.join(parent_dir, 'src')  # Default directory is 'src' under the parent directory
        output_file_name = f'esp_all_symbol.c'
        elfsym_file_dir = os.path.join(output_dir, output_file_name)

    # Ensure the output directory exists
    os.makedirs(output_dir, exist_ok=True)

    # Set default excluded symbols and allow user to extend the list
    exclude_symbols = ['elf_find_sym', 'g_customer_elfsyms'] + args.exclude

    if args.type == 'e':
        cmd = ['readelf', '-s', '-W', args.input]
    elif args.type == 'l':
        cmd = ['nm', '--defined-only', '-g', args.input]

    # Execute the readelf or nm command for static libraries
    try:
        result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=True)
        lines = result.stdout.splitlines()
    except subprocess.CalledProcessError as e:
        logging.error(f'Error executing command: {e.stderr}')
        sys.exit(1)
    except FileNotFoundError:
        logging.error('nm command not found. Please ensure it is installed and available in your PATH.')
        sys.exit(1)

    # Extract global symbols from ELF file or static library
    symbols = get_global_symbols(lines, type=args.type, undefined=args.undefined, symbol_types=['FUNC', 'OBJECT'])

    if not symbols:
        logging.warning('No global symbols found in the input file.')
        sys.exit(0)

    logging.debug('symbols:    %s'%(cmd))
    logging.debug('symbols:    %s'%(symbols))
    logging.debug('elfsym_file_dir:    %s'%(elfsym_file_dir))
    logging.debug('extracted_part:    %s'%(extracted_part))
    logging.debug('exclude_symbols:    %s'%(exclude_symbols))

    # Save the C file
    try:
        save_c_file(symbols, elfsym_file_dir, extracted_part, exclude_symbols=exclude_symbols)
        logging.info(f"C file with extern declarations and symbol table has been saved to '{elfsym_file_dir}'.")
    except Exception as e:
        logging.error(f'Error writing C file: {e}')
        sys.exit(1)

def _main():
    """
    Wrapper for the main function to catch and handle runtime errors.
    """
    try:
        main()
    except RuntimeError as e:
        logging.error(f'A fatal error occurred: {e}')
        sys.exit(2)

if __name__ == '__main__':
    _main()
