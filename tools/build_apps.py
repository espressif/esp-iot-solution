# SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

"""
This file is used in CI generate binary files for different kinds of apps
"""

import argparse
import sys
import os
import re
import logging
from pathlib import Path
import typing as t
from typing import ClassVar
import shutil
import subprocess
import yaml
import tempfile

from idf_build_apps import build_apps, find_apps, setup_logging
from idf_build_apps.app import CMakeApp

try:
    from idf_component_manager.dependencies import download_project_dependencies
    from idf_component_tools.manager import ManifestManager
    from idf_component_tools.utils import ProjectRequirements
except ImportError:
    download_project_dependencies = None
    ManifestManager = None
    ProjectRequirements = None

logger = logging.getLogger('idf_build_apps')
IDF_PATH = os.getenv('IDF_PATH', '')
BOARD_NAME = 'default'

PROJECT_ROOT = Path(__file__).parent.parent.absolute()
APPS_BUILD_PER_JOB = 30
IGNORE_WARNINGS = [
    r'memory region \`iram_loader_seg\' not declared',
    r'redeclaration of memory region \`iram_loader_seg\'',
    r'1/2 app partitions are too small',
    r'The current IDF version does not support using the gptimer API',
    r'DeprecationWarning: pkg_resources is deprecated as an API',
    r'Warning: The smallest app partition is nearly full',
    r'\'ADC_ATTEN_DB_11\' is deprecated',
    r'warning: unknown kconfig symbol*',
    r'warning High Performance Mode \(QSPI Flash > 80MHz\) is optional feature that depends on flash model. Read Docs First!',
    r'warning HPM-DC, which helps to run some flash > 80MHz by adjusting dummy cycles, is no longer enabled by default.',
    r'warning To enable this feature, your bootloader needs to have the support for it \(by explicitly selecting BOOTLOADER_FLASH_DC_AWARE\)',
    r'warning If your bootloader does not support it, select SPI_FLASH_HPM_DC_DISABLE to suppress the warning. READ DOCS FIRST!',
    r'warning: casting \'CvRNG\' {aka \'long long unsigned int\'} to \'cv::RNG&\' does not use \'cv::RNG::RNG\(uint64\)\' \[-Wcast-user-defined\]', # OpenCV warning: for examples/robot/ragtime_panther/follower
    r'WARNING: The following Kconfig variables were used in "if" clauses, but not',  # Due to the introduction of Kconfig and package manager dependencies, compilation warnings are generated
    r'.+MultiCommand.+',
]

class CustomApp(CMakeApp):
    build_system: t.Literal['custom'] = 'custom'  # Must be unique to identify your custom app type
    MANIFEST_NAMES: ClassVar[t.Tuple[str, ...]] = ('idf_component.yml', 'idf_component.yaml')
    BMGR_KEYS: ClassVar[t.Tuple[str, ...]] = ('espressif/esp_board_manager', 'esp_board_manager')

    def _build(
        self,
        *,
        manifest_rootpath: t.Optional[str] = None,
        modified_components: t.Optional[t.List[str]] = None,
        modified_files: t.Optional[t.List[str]] = None,
        check_app_dependencies: bool = False,
    ) -> None:
        # Remove managed_components and dependencies.lock to avoid conflicts
        shutil.rmtree(os.path.join(self.work_dir, 'managed_components'), ignore_errors=True)
        if os.path.isfile(os.path.join(self.work_dir, 'dependencies.lock')):
            os.remove(os.path.join(self.work_dir, 'dependencies.lock'))
        board_name = BOARD_NAME.strip()
        if board_name != 'default':
            if self.is_board_manager_project():
                self._pre_hook(board_name)
            else:
                logger.warning(
                    "Board '%s' specified but app '%s' has no esp_board_manager dependency; building as normal.",
                    board_name,
                    self.name,
                )
        super()._build(
            manifest_rootpath=manifest_rootpath,
            modified_components=modified_components,
            modified_files=modified_files,
            check_app_dependencies=check_app_dependencies,
        )

    def _get_project_manifest_paths(self) -> t.List[Path]:
        proj = Path(self.work_dir).absolute()
        manifest_paths = []

        main_dir = proj / 'main'
        manifest_paths.extend(main_dir / name for name in self.MANIFEST_NAMES if (main_dir / name).is_file())

        components_dir = proj / 'components'
        if components_dir.is_dir():
            for component_dir in sorted(path for path in components_dir.iterdir() if path.is_dir()):
                manifest_paths.extend(
                    component_dir / name
                    for name in self.MANIFEST_NAMES
                    if (component_dir / name).is_file()
                )

        return manifest_paths

    def _find_bmgr_dependency(self) -> t.Optional[t.Tuple[Path, str, t.Any]]:
        for manifest_path in self._get_project_manifest_paths():
            try:
                with open(manifest_path, 'r', encoding='utf-8') as f:
                    deps = (yaml.safe_load(f) or {}).get('dependencies', {})
            except (OSError, yaml.YAMLError) as e:
                logger.warning('Failed to read %s: %s', manifest_path, e)
                continue

            if not isinstance(deps, dict):
                continue

            bmgr_key = next((key for key in self.BMGR_KEYS if key in deps), None)
            if bmgr_key:
                return manifest_path, bmgr_key, deps[bmgr_key]

        return None

    def is_board_manager_project(self) -> bool:
        return self._find_bmgr_dependency() is not None

    def clear_project_generated_files(self) -> None:
        proj_path = Path(self.work_dir).absolute()
        shutil.rmtree(proj_path / 'managed_components', ignore_errors=True)
        shutil.rmtree(proj_path / 'components' / 'gen_bmgr_codes', ignore_errors=True)
        (proj_path / 'board_manager.defaults').unlink(missing_ok=True)

    def _download_bmgr_component(self) -> bool:
        """
        Extract the esp_board_manager dependency from the manifest file and download it.
        Returns True if successful, False otherwise.
        """
        if not all([yaml, download_project_dependencies, ManifestManager, ProjectRequirements]):
            logger.warning('idf-component-manager is not available, cannot download board manager component')
            return False

        proj = Path(self.work_dir).absolute()
        managed_path = proj / 'managed_components'
        lock_path = proj / 'dependencies.lock'

        bmgr_dep = self._find_bmgr_dependency()
        if not bmgr_dep:
            logger.warning(
                'esp_board_manager dependency not found in project manifests under %s/main or %s/components/*',
                proj,
                proj,
            )
            return False

        manifest_path, bmgr_key, bmgr_value = bmgr_dep

        managed_path.mkdir(parents=True, exist_ok=True)

        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                temp_manifest = Path(temp_dir) / 'idf_component.yml'
                with open(temp_manifest, 'w', encoding='utf-8') as f:
                    yaml.dump({'dependencies': {bmgr_key: bmgr_value}}, f)

                manifest = ManifestManager(str(temp_dir), 'temp').load()
                download_project_dependencies(
                    ProjectRequirements([manifest]),
                    str(lock_path),
                    str(managed_path),
                )
                logger.info('Successfully downloaded esp_board_manager component from %s to %s', manifest_path, managed_path)
                return True
        except Exception as e:
            logger.error('Error downloading esp_board_manager component: %s', e)
            return False

    def _pre_hook(self, board_name: str) -> None:
        logger.info(
            "Pre build hook for app '%s' at '%s' for target '%s', board '%s'",
            self.name,
            self.work_dir,
            self.target,
            board_name,
        )
        self.clear_project_generated_files()
        if not board_name:
            logger.info('No board name specified, skip the pre build hook')
            return
        if not IDF_PATH:
            logger.warning('IDF_PATH is not set; skip board manager config generation.')
            return

        # Download board manager component if needed
        bmgr_path = Path(self.work_dir).absolute() / 'managed_components' / 'espressif__esp_board_manager'
        if not bmgr_path.exists():
            logger.info('Board manager component not found, downloading...')
            if not self._download_bmgr_component():
                logger.error('Failed to download board manager component, cannot generate config')
                return

        # Set environment variable for IDF_EXTRA_ACTIONS_PATH
        env = os.environ.copy()
        env['IDF_EXTRA_ACTIONS_PATH'] = str(bmgr_path)

        subprocess.run(
            [
                sys.executable,
                f"{IDF_PATH}/tools/idf.py",
                'gen-bmgr-config',
                '-c',
                str(Path(self.work_dir).absolute() / 'boards'),
                '-b',
                board_name,
            ],
            cwd=self.work_dir,
            env=env,
            check=True,
        )

def _get_idf_version():
    if os.environ.get('IDF_VERSION'):
        return os.environ.get('IDF_VERSION')
    version_path = os.path.join(os.environ['IDF_PATH'], 'tools/cmake/version.cmake')
    regex = re.compile(r'^\s*set\s*\(\s*IDF_VERSION_([A-Z]{5})\s+(\d+)')
    ver = {}
    with open(version_path) as f:
        for line in f:
            m = regex.match(line)
            if m:
                ver[m.group(1)] = m.group(2)
    return '{}.{}'.format(int(ver['MAJOR']), int(ver['MINOR']))

def get_cmake_apps(
    paths,
    target,
    config_rules_str,
    ignore_warnings,
    recursive,
    default_build_targets,
):  # type: (List[str], str, str, bool, bool, List[str]) -> List[App]
    idf_ver = _get_idf_version()
    apps = find_apps(
        paths,
        recursive=recursive,
        target=target,
        build_dir=f'{idf_ver}/build_@t_@w',
        config_rules_str=config_rules_str,
        build_log_filename='build_log.txt',
        size_json_filename='size.json',
        check_warnings=not ignore_warnings,
        no_preserve=False,
        default_build_targets=default_build_targets,
        manifest_files=[
            str(Path(PROJECT_ROOT) /'components'/'.build-rules.yml'),
            str(Path(PROJECT_ROOT) /'examples'/'.build-rules.yml'),
            str(Path(PROJECT_ROOT) /'tools'/'.build-rules.yml'),
        ],
        build_system=CustomApp,
    )
    return apps


def main(args):  # type: (argparse.Namespace) -> None
    default_build_targets = args.default_build_targets.split(',') if args.default_build_targets else None
    global BOARD_NAME
    BOARD_NAME = (args.board or '').strip()
    apps = get_cmake_apps(args.paths, args.target, args.config, args.ignore_warnings, args.recursive, default_build_targets)

    if args.find:
        if args.output:
            os.makedirs(os.path.dirname(os.path.realpath(args.output)), exist_ok=True)
            with open(args.output, 'w') as fw:
                for app in apps:
                    fw.write(app.to_json() + '\n')
        else:
            for app in apps:
                print(app)

        sys.exit(0)

    if args.exclude_apps:
        apps_to_build = [app for app in apps if app.name not in args.exclude_apps]
    else:
        apps_to_build = apps[:]

    logger.info('Found %d apps after filtering', len(apps_to_build))
    logger.info(
        'Suggest setting the parallel count to %d for this build job',
        len(apps_to_build) // APPS_BUILD_PER_JOB + 1,
    )

    ret_code = build_apps(
        apps_to_build,
        parallel_count=args.parallel_count,
        parallel_index=args.parallel_index,
        dry_run=False,
        collect_size_info=args.collect_size_info,
        keep_going=True,
        check_warnings=not args.ignore_warnings,
        ignore_warning_strs=IGNORE_WARNINGS,
        copy_sdkconfig=True,
        no_preserve=False,
    )

    sys.exit(ret_code)


if __name__ == '__main__':
    def str2bool(v):
        if isinstance(v, bool):
            return v
        if v.lower() in ('yes', 'true', 't', 'y', '1'):
            return True
        elif v.lower() in ('no', 'false', 'f', 'n', '0'):
            return False
        else:
            raise argparse.ArgumentTypeError('Boolean value expected.')

    parser = argparse.ArgumentParser(
        description='Build all the apps for different test types. Will auto remove those non-test apps binaries',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument('paths', nargs='*', help='Paths to the apps to build.')
    parser.add_argument(
        '-t', '--target',
        default='all',
        help='Build apps for given target. could pass "all" to get apps for all targets',
    )
    parser.add_argument(
        '--config',
        default=['sdkconfig.defaults=defaults', 'sdkconfig.ci.*=', '=defaults'],
        action='append',
        help='Adds configurations (sdkconfig file names) to build. This can either be '
        'FILENAME[=NAME] or FILEPATTERN. FILENAME is the name of the sdkconfig file, '
        'relative to the project directory, to be used. Optional NAME can be specified, '
        'which can be used as a name of this configuration. FILEPATTERN is the name of '
        'the sdkconfig file, relative to the project directory, with at most one wildcard. '
        'The part captured by the wildcard is used as the name of the configuration.',
    )
    parser.add_argument(
        '--parallel-count', default=1, type=int, help='Number of parallel build jobs.'
    )
    parser.add_argument(
        '--parallel-index',
        default=1,
        type=int,
        help='Index (1-based) of the job, out of the number specified by --parallel-count.',
    )
    parser.add_argument(
        '--collect-size-info',
        type=argparse.FileType('w'),
        help='If specified, the test case name and size info json will be written to this file',
    )
    parser.add_argument(
        '--exclude-apps',
        nargs='*',
        help='Exclude build apps',
    )
    parser.add_argument(
        '--default-build-targets',
        default=None,
        help='default build targets used in manifest files',
    )
    parser.add_argument(
        '-v', '--verbose',
        action='count', default=0,
        help='Show verbose log message',
    )
    parser.add_argument(
        '--find',
        action='store_true',
        help='Find the buildable applications. If enable this option, build options will be ignored.',
    )
    parser.add_argument(
        '-o', '--output',
        help='Print the found apps to the specified file instead of stdout'
    )
    parser.add_argument(
        '-r', '--recursive',
        type=str2bool,
        nargs='?',
        default=True,
        const=True,
        help='Build apps recursively',
    )
    parser.add_argument(
        '--ignore-warnings',
        action='store_true',
        help='Ignore warnings when building apps',
    )
    parser.add_argument(
        '--board',
        default='default',
        help='Board Manager board name. Empty value disables Board Manager flow.',
    )

    arguments = parser.parse_args()
    if not arguments.paths:
        arguments.paths = [PROJECT_ROOT]
    setup_logging(verbose=arguments.verbose)  # Info
    main(arguments)
