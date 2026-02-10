#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
#generate the components versions table in README.md and README_CN.md
import os
import yaml

If_release_branch = False

def get_components_versions(path):
    components = {}
    for root, dirs, files in os.walk(path):
        for file in files:
            if file == 'idf_component.yml':
                file_path = os.path.join(root, file)
                if 'test_apps' in file_path or 'benchmark' in file_path:
                    #print(f"Skipped {file_path} (in test_apps)")
                    continue

                # Parse from the yml file, get the component version
                # Parse the folder name from root path to get the component name
                with open(file_path, 'r') as f:
                    data = yaml.load(f, Loader=yaml.FullLoader)
                    component_name = root.split('/')[-1]
                    component_version = data['version']
                    components[component_name] = component_version
    return components

components = get_components_versions(os.path.join(os.path.dirname(__file__), '../components'))
components.update(get_components_versions(os.path.join(os.path.dirname(__file__), '../tools')))
#sort the components by name from A to Z
components = dict(sorted(components.items(), key=lambda item: item[0]))

# Load the upload_component.yml file
with open(os.path.join(os.path.dirname(__file__), '../.github/workflows/upload_component.yml'), 'r') as file:
    data = yaml.safe_load(file)
# Extract the directories string
upload_directories = data['jobs']['upload_components']['steps'][2]['with']['directories']
# Split the directories string into a list of components
upload_components = [component.split('/')[-1] for component in upload_directories.split(';') if component]

# If components are not found in the upload_component.yml file, print a warning
# and remove them from the components dictionary
for component in list(components.keys()):
    if component not in upload_components:
        print(f'\033[93mWarning: {component} not found in upload_component.yml\033[0m')
        components.pop(component)

#generate markdown format table,

#component links format like https://components.espressif.com/components/espressif/aht20
#badge links format like https://components.espressif.com/components/espressif/aht20/badge.svg

# for release branch using different badge
#component links format like https://components.espressif.com/components/espressif/aht20/versions/0.1.0~1
#bagde links format like https://img.shields.io/badge/Beta-0.1.0~1-yellow
#if the version major is 0, the badge color is yellow, and the version is Beta
#otherwise, the badge color is Blue, and the version is Stable

release_table_items = ''
for component, version in components.items():
    if If_release_branch:
        release_table_items += f"| [{component}](https://components.espressif.com/components/espressif/{component}/versions/{version}) | [![{version}](https://img.shields.io/badge/{'Beta' if version.split('.')[0] == '0' else 'Stable'}-{version}-{'yellow' if version.split('.')[0] == '0' else 'blue'})](https://components.espressif.com/components/espressif/{component}/versions/{version}) |\n"
    else:
        release_table_items += f'| [{component}](https://components.espressif.com/components/espressif/{component}) | [![Component Registry](https://components.espressif.com/components/espressif/{component}/badge.svg)](https://components.espressif.com/components/espressif/{component}) |\n'

release_table = '| Component | Version |\n| --- | --- |\n' + release_table_items
release_table_cn = '| 组件 | 版本 |\n| --- | --- |\n' + release_table_items

#update the README.md file, rewrite the file with table between <center> and </center>
readme_paths = [
    os.path.join(os.path.dirname(__file__), '../README.md'),
    os.path.join(os.path.dirname(__file__), '../README_CN.md')
]

for readme_path in readme_paths:
    with open(readme_path, 'r') as f:
        readme = f.read()
        start = readme.find('<center>')
        end = readme.find('</center>')
        if 'README_CN.md' in readme_path:
            readme = readme[:start+8] + '\n\n' + release_table_cn + '\n' + readme[end:]
        else:
            readme = readme[:start+8] + '\n\n' + release_table + '\n' + readme[end:]
    with open(readme_path, 'w') as f:
        f.write(readme)
