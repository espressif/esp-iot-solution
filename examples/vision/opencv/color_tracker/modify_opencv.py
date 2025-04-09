# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import os
import shutil

# Define file paths
script_dir = os.path.dirname(os.path.abspath(__file__))
source_file = os.path.join(script_dir, 'main', 'CMakeLists.txt.opencv')
target_file = os.path.join(script_dir, 'managed_components', 'espressif__opencv', 'CMakeLists.txt')

# Check if the source file exists
if not os.path.exists(source_file):
    print(f"Error: Source file '{source_file}' not found!")
    exit(1)

# Check if the target file exists
if not os.path.exists(target_file):
    print(f"Error: Target file '{target_file}' not found!")
    exit(1)

# Read the content from the source file
with open(source_file, 'r', encoding='utf-8') as src:
    content = src.read()

# Replace the content of the target file
with open(target_file, 'w', encoding='utf-8') as tgt:
    tgt.write(content)

print(f"Successfully replaced '{target_file}' with the content from '{source_file}'.")
