#!/usr/bin/env python
#
# Checks that all example paths in the idf_component.yml file are valid
#
# SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

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
            print(f'Invalid example path: {absolute_path}')
            return False
        else:
            print(f'Found example path: {absolute_path}')
    return True

def check_component_paths(file_path):
    with open(file_path, 'r') as f:
        data = yaml.safe_load(f)
        directories = data['jobs']['upload_components']['steps'][2]['with']['directories'].strip().split(';')

        for directory in directories:
            if directory.strip():
                if not os.path.isdir(directory.strip()):
                    print(f'Invalid component path: {directory.strip()}')
                    return False
                else:
                    print(f'Found component path: {directory.strip()}')
                    valid = check_example_paths(directory.strip())
                    if not valid:
                        return False
    return True

if __name__ == '__main__':
    # Provide the path to the upload.yml file
    file_path = os.getenv('IOT_SOLUTION_PATH') + '/.github/workflows/upload_component.yml'
    valid = check_component_paths(file_path)

    if valid:
        print('All example paths are valid.')
    else:
        print('Invalid example paths found.')
        sys.exit(1)
