#!/usr/bin/env python
#
# Checks that all example paths in the idf_component.yml file are valid
#
# Copyright 2023 Espressif Systems (Shanghai) PTE LTD
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
#

import yaml
import os
import sys

def check_example_paths(component_path):
    # Load the component.yml file
    component_yml = component_path + '/idf_component.yml'
    with open(component_yml, 'r') as f:
        component_data = yaml.safe_load(f)

    # Get the examples section from the loaded data
    examples = component_data.get('examples', [])

    for example in examples:
        path = example.get('path', '')
        absolute_path = os.path.normpath(os.path.join(component_path, path))
        # Check if the path is valid
        if not os.path.exists(absolute_path):
            print(f"Invalid example path: {absolute_path}")
            return False
        else:
            print(f"Found example path: {absolute_path}")
    return True

def check_component_paths(file_path):
    with open(file_path, 'r') as f:
        data = yaml.safe_load(f)
        directories = data['jobs']['upload_components']['steps'][1]['with']['directories'].strip().split(';')

        for directory in directories:
            if directory.strip():
                if not os.path.isdir(directory.strip()):
                    print(f"Invalid component path: {directory.strip()}")
                    return False
                else:
                    print(f"Found component path: {directory.strip()}")
                    valid = check_example_paths(directory.strip())
                    if not valid:
                        return False
    return True

if __name__ == '__main__':
    # Provide the path to the upload.yml file
    file_path = os.getenv('IOT_SOLUTION_PATH') + '/.github/workflows/upload_component.yml'
    valid = check_component_paths(file_path)

    if valid:
        print("All example paths are valid.")
    else:
        print("Invalid example paths found.")
        sys.exit(1)
