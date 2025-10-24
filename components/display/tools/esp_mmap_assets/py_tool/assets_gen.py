# SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""
Standalone build script for esp_mmap_assets
This script allows you to build assets without CMake
"""

import os
import sys
import json
import argparse
import subprocess
from pathlib import Path

# Color codes for terminal output
GREEN = '\033[1;32m'
YELLOW = '\033[1;33m'
RED = '\033[1;31m'
RESET = '\033[0m'

def check_and_install_dependencies():
    """Check and install required Python dependencies"""
    python_exe = sys.executable
    # Dependencies with their module names for checking
    dependencies = [
        ('Pillow', 'PIL'),
        ('numpy', 'numpy'),
        ('qoi-conv', 'qoi-conv.qoi'),
        ('packaging', 'packaging')
    ]

    for package_name, module_name in dependencies:
        # Check if module is installed
        check_cmd = [python_exe, '-c', f'import importlib; importlib.import_module("{module_name}")']

        try:
            result = subprocess.run(
                check_cmd,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=5
            )

            if result.returncode != 0:
                print(f'{YELLOW}{package_name} not found. Installing...{RESET}')
                install_cmd = [python_exe, '-m', 'pip', 'install', '-U', package_name]

                install_result = subprocess.run(
                    install_cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True
                )

                if install_result.returncode != 0:
                    print(f'{RED}Failed to install {package_name}. Please install it manually:{RESET}')
                    print(f'  pip install {package_name}')
                    sys.exit(1)
                else:
                    print(f'{GREEN}{package_name} successfully installed.{RESET}')
            else:
                print(f'{GREEN}✓ {package_name} is installed{RESET}')
        except subprocess.TimeoutExpired:
            print(f'{RED}Timeout checking {package_name}{RESET}')
            sys.exit(1)
        except Exception as e:
            print(f'{RED}Error checking {package_name}: {e}{RESET}')
            sys.exit(1)

def get_script_dir():
    """Get the directory where this script is located"""
    return Path(__file__).parent.absolute()

def find_spiffs_gen_script():
    """Find the spiffs_assets_gen.py script"""
    script_dir = get_script_dir()
    spiffs_gen = script_dir / 'spiffs_assets_gen.py'

    if not spiffs_gen.exists():
        print(f'{RED}Error: spiffs_assets_gen.py not found at {spiffs_gen}{RESET}')
        sys.exit(1)

    return spiffs_gen

def create_default_config(args):
    """Create a temporary configuration file from command line arguments"""
    # Validate assets path
    assets_path = os.path.abspath(args.assets_path)
    if not os.path.exists(assets_path):
        print(f'{RED}Error: Assets directory does not exist: {assets_path}{RESET}')
        print(f'{YELLOW}Please create the directory or check the path.{RESET}')
        sys.exit(1)

    if not os.path.isdir(assets_path):
        print(f'{RED}Error: Assets path is not a directory: {assets_path}{RESET}')
        sys.exit(1)

    # Get absolute paths
    output_path = os.path.abspath(args.output)
    output_dir = os.path.dirname(output_path)

    # Auto-detect pjpg processor path
    script_dir = get_script_dir()
    pjpg_processor_path = script_dir / 'png_processor.py'
    if not pjpg_processor_path.exists():
        pjpg_processor_path = ''
    else:
        pjpg_processor_path = str(pjpg_processor_path)

    # Set default values
    config = {
        'project_dir': os.getcwd(),
        'include_path': os.path.join(output_dir, 'include'),
        'assets_path': assets_path,
        'image_file': output_path,  # Use full path
        'lvgl_ver': '9.0.0',
        'assets_size': args.partition_size or '0x1000000',  # Default 16MB
        'support_format': args.support_format or '.png,.jpg',
        'name_length': str(args.name_length or 32),
        'split_height': str(args.split_height or 0),
        'support_spng': args.support_spng,
        'support_sjpg': args.support_sjpg,
        'support_qoi': args.support_qoi,
        'support_sqoi': args.support_sqoi,
        'support_pjpg': args.support_pjpg,
        'support_raw': False,
        'support_raw_dither': 'false',
        'support_raw_bgr': 'false',
        'support_raw_ff': 'BIN',
        'support_raw_cf': 'ARGB8888',
        'app_bin_path': '',
        'pjpg_processor': pjpg_processor_path,
        'header_asset_name': args.partition_name or 'assets',
        'source_bin_path': ''
    }

    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)

    # Save config to a temporary file in the output directory
    config_path = os.path.join(output_dir, '.build_config_tmp.json')
    with open(config_path, 'w') as f:
        json.dump(config, f, indent=4)

    return config_path

def validate_config(config_path):
    """Validate that the config file exists and is valid JSON"""
    if not os.path.exists(config_path):
        print(f'{RED}Error: Config file not found: {config_path}{RESET}')
        sys.exit(1)

    try:
        with open(config_path, 'r') as f:
            config = json.load(f)
        return config
    except json.JSONDecodeError as e:
        print(f'{RED}Error: Invalid JSON in config file: {e}{RESET}')
        sys.exit(1)

def run_spiffs_gen(config_path, operation):
    """Run the spiffs_assets_gen.py script with the specified operation"""
    spiffs_gen = find_spiffs_gen_script()
    python_exe = sys.executable

    cmd = [python_exe, str(spiffs_gen), '--config', config_path]

    if operation == 'build':
        cmd.append('--build')
    elif operation == 'copy':
        cmd.append('--copy')
    elif operation == 'merge':
        cmd.append('--merge')
    else:
        print(f'{RED}Error: Invalid operation: {operation}{RESET}')
        sys.exit(1)

    try:
        result = subprocess.run(cmd, check=True)
        print('-' * 60)
        print(f"{GREEN}✓ Operation '{operation}' completed successfully!{RESET}")
        return result.returncode
    except subprocess.CalledProcessError as e:
        print('-' * 60)
        print(f"{RED}✗ Operation '{operation}' failed with exit code {e.returncode}{RESET}")
        sys.exit(e.returncode)

def main():
    # Check and install dependencies first
    print(f'{YELLOW}Checking dependencies...{RESET}')
    check_and_install_dependencies()
    print()

    parser = argparse.ArgumentParser(
        description='Standalone build tool for esp_mmap_assets',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Quick build with basic parameters
  %(prog)s --assets-path ./assets --output ./build/assets.bin

  # Build with split PNG format
  %(prog)s --assets-path ./assets --output ./build/assets.bin \\
           --support-spng --split-height 16

  # Build with split JPEG format
  %(prog)s --assets-path ./assets --output ./build/assets.bin \\
           --support-sjpg --split-height 16
        """
    )

    # Required parameters
    config_group = parser.add_argument_group('Required Parameters')
    config_group.add_argument('--assets-path', type=str, required=True,
                             help='Path to assets directory')
    config_group.add_argument('--output', type=str, required=True,
                             help='Output binary file path')

    # Optional parameters
    optional_group = parser.add_argument_group('Optional Parameters')
    optional_group.add_argument('--partition-size', type=str,
                               help='Partition size in hex (e.g., 0x100000)')
    optional_group.add_argument('--support-format', type=str,
                               help='Supported file formats (e.g., .png,.jpg)')
    optional_group.add_argument('--name-length', type=int,
                               help='Maximum filename length')
    optional_group.add_argument('--partition-name', type=str,
                               help='Partition name for header generation')

    # Format conversion options
    format_group = parser.add_argument_group('Format Conversion Options')
    format_group.add_argument('--support-spng', action='store_true',
                             help='Enable split PNG support')
    format_group.add_argument('--support-sjpg', action='store_true',
                             help='Enable split JPEG support')
    format_group.add_argument('--support-qoi', action='store_true',
                             help='Enable QOI format support')
    format_group.add_argument('--support-sqoi', action='store_true',
                             help='Enable split QOI support')
    format_group.add_argument('--support-pjpg', action='store_true',
                             help='Enable progressive JPEG support')
    format_group.add_argument('--split-height', type=int,
                             help='Split height for image processing')

    args = parser.parse_args()

    # Always use build operation
    operation = 'build'

    # Create temporary config file from parameters
    config_path = create_default_config(args)
    config = validate_config(config_path)

    # Print configuration summary
    print(f'{YELLOW}Configuration Summary:{RESET}')
    print(f"  Assets Path: {config.get('assets_path', 'N/A')}")
    print(f"  Output File: {config.get('image_file', 'N/A')}")
    if config.get('support_spng') or config.get('support_sjpg') or config.get('support_sqoi'):
        print(f"  Split Height: {config.get('split_height', 'N/A')}")
    print()

    # Run the operation
    run_spiffs_gen(config_path, operation)

if __name__ == '__main__':
    main()
