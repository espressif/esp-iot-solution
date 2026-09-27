# SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import re
import sys
import subprocess
import os
import rtoml
import shutil
import yaml
from pathlib import Path

# Root toml object
toml_obj = {'esp_toml_version': 1.0, 'firmware_images_url': f'https://dl.espressif.com/AE/esp-iot-solution/', 'supported_apps': []}

PROJECT_ROOT = Path(__file__).parent.absolute()
PROJECT_CONFIG_FILE = os.path.join(PROJECT_ROOT, 'upload_project_config.yml')

class App:

    def __init__(self, app):
        # App directory
        self.app_dir = app
        if app:
            # Name of the app
            self.name = app.split('/')[-1]
        # build_info_dict['target'] (esp32, esp32s2, etc)
        # build_info_dict['sdkconfig']
        # build_info_dict['build_dir']
        # build_info_dict['idf_version']
        self.build_info = []
        # App version
        self.app_version = ''
        # App readme
        self.readme = ''
        # App description
        self.description = ''
        # App SDK count
        self.sdkcount = ''

current_app = App(None)

# Regex to get the app_dir
def get_app_dir(line):
    return re.search(r'"app_dir"\s*:"([^,]*)",', line).group(1) if re.search(r'"app_dir"\s*:([^,]*)",', line) else None

# Regex to get the target
def get_target(line):
    return re.search(r'"target"\s*:"([^,]*)",', line).group(1) if re.search(r'"target"\s*:"([^,]*)",', line) else None

# Regex to get the kit
def get_sdkconfig(line):
    return re.search(r'"config_name"\s*:"([^,]*)",', line).group(1) if re.search(r'"config_name"\s*:"([^,]*)",', line) else None

# Regex to get the build_dir
def get_build_dir(line):
    return re.search(r'"build_dir"\s*:"([^,]*)",', line).group(1) if re.search(r'"build_dir"\s*:"([^,]*)",', line) else None

def find_build_dir(
    app_path: str,
    build_dir: str,
) -> list:
    """
    Find a build directory under every available IDF version.

    Args:
        app_path: app path
        build_dir: build directory from idf_build_apps

    Returns:
        valid build directory
    """

    app_dir = Path(app_path)
    if not app_dir.is_dir():
        return []

    version_dirs = sorted(
        (
            path for path in app_dir.iterdir()
            if path.is_dir() and re.fullmatch(r'\d+\.\d+', path.name)
        ),
        key=lambda path: tuple(int(part) for part in path.name.split('.')),
    )
    found_build_dirs = []
    for version_dir in version_dirs:
        relative_path = os.path.join(version_dir.name, os.path.basename(build_dir))
        binary_path = app_dir / relative_path
        if binary_path.is_dir():
            idf_version = f'v{version_dir.name}'
            print(f'find valid binary path: {binary_path}, idf version: {idf_version}')
            found_build_dirs.append((relative_path, idf_version))
    return found_build_dirs


def find_configured_build_dir(app_path: str, target: str, config: str) -> list:
    """Find build artifacts selected by the LaunchPad upload configuration."""
    build_name = f'build_{target}' + (f'_{config}' if config else '')
    return find_build_dir(app_path, build_name)

def find_app_version(app_path: str)->str:
    pattern = r'(^|\n)version: "?([0-9]+).([0-9]+).([0-9]+)"?'

    for root, dirs, files in os.walk(f'{app_path}/main'):
        for file_name in files:
            if file_name == 'idf_component.yml':
                file_path = os.path.join(root, file_name)

                try:
                    with open(file_path, 'r') as file:
                        content = file.read()
                    match = re.search(pattern, content)
                    if match:
                        major_version = match.group(2)
                        minor_version = match.group(3)
                        patch_version = match.group(4)
                        return f'{major_version}.{minor_version}.{patch_version}'
                    else:
                        return ''
                except Exception as e:
                    return f'error: {str(e)}'
    return ''

# Remove the app from the json
def remove_app_from_config(apps):
    with open(PROJECT_CONFIG_FILE, 'r') as file:
        config_apps = yaml.safe_load(file)
    apps_by_dir = {}
    for app in apps:
        # remove './' in string
        if app['app_dir'].startswith('./'):
            app_dir = app['app_dir'][2:]
        else:
            app_dir = app['app_dir']

        if app_dir in config_apps:
            apps_by_dir[app_dir] = app

    new_apps = []
    for app_dir, example in config_apps.items():
        app = apps_by_dir.get(app_dir)
        if app is None:
            app = App(app_dir).__dict__
            app['app_version'] = find_app_version(app_dir)

        existing_builds = {
            (info['target'], info['sdkconfig'], info['build_dir'])
            for info in app['build_info']
        }
        for target, target_config in example.items():
            if target in ('description', 'readme') or not isinstance(target_config, dict):
                continue
            for sdkconfig in target_config.get('sdkconfig', []):
                for build_dir, idf_version in find_configured_build_dir(app_dir, target, sdkconfig):
                    build_key = (target, sdkconfig, build_dir)
                    if build_key not in existing_builds:
                        app['build_info'].append({
                            'target': target,
                            'sdkconfig': sdkconfig,
                            'build_dir': build_dir,
                            'idf_version': idf_version,
                        })
                        existing_builds.add(build_key)

        matched_build_info = []
        sdkcount = {}
        for build_info in app['build_info']:
            target = build_info.get('target')
            if (
                example.get(target) and
                build_info.get('sdkconfig') in example.get(target, {}).get('sdkconfig', [])
            ):
                if build_info.get('sdkconfig') == 'defaults':
                    build_info['sdkconfig'] = target + '_generic'
                matched_build_info.append(build_info)

                if not sdkcount.get(f'developKits.{target}'):
                    sdkcount[f'developKits.{target}'] = 1
                else:
                    sdkcount[f'developKits.{target}'] += 1
        if not matched_build_info:
            continue
        app['build_info'] = matched_build_info
        app['sdkcount'] = sdkcount
        if example.get('readme'):
            app['readme'] = example['readme']
        if example.get('description'):
            app['description'] = example['description']
        new_apps.append(app)
    return new_apps

# Squash the json into a list of apps
def squash_json(input_str):
    global current_app
    # Split the input into lines
    lines = input_str.splitlines()
    output_list = []
    for line in lines:
        # Get the app_dir
        app = get_app_dir(line)
        # If its a not a None and not the same as the current app
        if app != None:

            # Save the previous app
            if current_app.app_dir != app:
                if current_app.app_dir and current_app.build_info:
                    output_list.append(current_app.__dict__)
                current_app = App(app)

            current_app.app_version = find_app_version(current_app.app_dir)
            build_dir = find_build_dir(current_app.app_dir, get_build_dir(line))
            for dir in build_dir:
                build_info_dict = {}
                build_info_dict['target'] = get_target(line)
                build_info_dict['sdkconfig'] = get_sdkconfig(line)
                build_info_dict['build_dir'] = dir[0]
                build_info_dict['idf_version'] = dir[1]
                current_app.build_info.append(build_info_dict)

    # Append last app
    output_list.append(current_app.__dict__)
    return output_list

# Merge binaries for each app
def merge_binaries(apps):
    os.makedirs('binaries', exist_ok=True)
    for app in apps:
        # If we are merging binaries for kits
        for build_info in app['build_info']:
            target = build_info['target']
            sdkconfig = build_info['sdkconfig']
            build_dirs = build_info['build_dir']
            idf_version = build_info['idf_version']
            app_version = app['app_version']
            sdkcount = app['sdkcount']
            if sdkcount.get(f'developKits.{target}') == 1:
                sdkconfig = ''
            bin_name = f'{app["name"]}-{target}' + (f'-{sdkconfig}' if sdkconfig else '') + (f'-{app_version}' if app_version else '') + (f'-{idf_version}' if idf_version else '') + '.bin'
            cmd = ['esptool.py', '--chip', target, 'merge_bin', '-o', bin_name, '@flash_args']
            cwd = os.path.join(app.get('app_dir'), build_dirs)
            print(f'Processing {cwd}')
            try:
                print(f'Merged binaries for {bin_name}')
                subprocess.run(cmd, cwd=cwd)
            except subprocess.CalledProcessError as e:
                print(f'Failed to merge binaries for {bin_name}: {e}')
                continue

            try:
                shutil.move(f'{cwd}/{bin_name}', 'binaries')
            except shutil.Error:
                os.remove(f'binaries/{bin_name}')
                shutil.move(f'{cwd}/{bin_name}', 'binaries')

# Write a single app to the toml file
def write_app(app):
    # If we are working with kits
    for build_info in app['build_info']:
        target = build_info['target']
        sdkconfig = build_info['sdkconfig']
        idf_version = build_info['idf_version']
        app_version = app['app_version']
        sdkcount = app['sdkcount']
        if sdkcount.get(f'developKits.{target}') == 1:
            sdkconfig = ''
        bin_name = f'{app["name"]}-{target}' + (f'-{sdkconfig}' if sdkconfig else '') + (f'-{app_version}' if app_version else '') + (f'-{idf_version}' if idf_version else '') + '.bin'
        support_app = f'{app["name"]}'
        if not toml_obj.get(support_app):
            toml_obj[support_app] = {}
            toml_obj[support_app]['android_app_url'] = ''
            toml_obj[support_app]['ios_app_url'] = ''
            if app.get('readme'):
                toml_obj[support_app]['readme.text'] = app['readme']
            if app.get('description'):
                toml_obj[support_app]['description'] = app['description']

        if not toml_obj[support_app].get('chipsets'):
            toml_obj[support_app]['chipsets'] = [f'{target}']
        elif f'{target}' not in toml_obj[support_app]['chipsets']:
                toml_obj[support_app]['chipsets'].append(f'{target}')

        if sdkcount.get(f'developKits.{target}') > 1:
            if not toml_obj[support_app].get(f'developKits.{target}'):
                toml_obj[support_app][f'developKits.{target}'] = [f'{sdkconfig}']
            else:
                toml_obj[support_app][f'developKits.{target}'].append(f'{sdkconfig}')
        if sdkcount.get(f'developKits.{target}') == 1:
            toml_obj[support_app][f'image.{target}'] = bin_name
        else:
            toml_obj[support_app][f'image.{sdkconfig}'] = bin_name

# Create the config.toml file
def create_config_toml(apps):
    for app in apps:
        toml_obj['supported_apps'].extend([f'{app["name"]}'])
        write_app(app)

        with open('binaries/config.toml', 'w') as toml_file:
            rtoml.dump(toml_obj, toml_file)

        # This is a workaround to remove the quotes around the image.<string> in the config.toml file as dot is not allowed in the key by default
        with open('binaries/config.toml', 'r') as toml_file:
            fixed = unquote_config_keys(toml_file.read())

        with open('binaries/config.toml', 'w') as toml_file:
            toml_file.write(fixed)

def unquote_config_keys(text):
    # Define the regular expression pattern to find "image.<string>"
    pattern = r'"((image|readme|developKits)\.[\w-]+)"'
    # Use re.sub() to replace the matched pattern with image.<string>
    result = re.sub(pattern, r'\1', text)
    return result

def main(input_file):
    """Merge selected build artifacts and create the LaunchPad manifest."""
    with open(input_file, 'r') as file:
        apps = squash_json(file.read())
    if os.path.exists(PROJECT_CONFIG_FILE):
        print(f'Reading {PROJECT_CONFIG_FILE}')
        apps = remove_app_from_config(apps)
    print('merge binaries with', apps)
    merge_binaries(apps)
    print('create config.toml...')
    create_config_toml(apps)
    print('create config.toml success')


if __name__ == '__main__':
    main(sys.argv[1])
