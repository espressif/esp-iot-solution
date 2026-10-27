# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""Host-only regressions for Board Manager's idf.py extension environment."""

import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import types
import unittest
from unittest import mock


BUILD_APPS = Path(__file__).resolve().parents[1] / 'build_apps.py'


class FakeCMakeApp:
    def _build(self, **kwargs):
        self.parent_build_extra_actions = os.environ.get('IDF_EXTRA_ACTIONS_PATH')
        if getattr(self, 'fail_parent_build', False):
            raise RuntimeError('Simulated IDF build failure')


def load_build_apps():
    # Keep this test runnable without an installed ESP-IDF/idf-build-apps.
    pkg = types.ModuleType('idf_build_apps')
    pkg.__path__ = []
    pkg.build_apps = mock.Mock()
    pkg.find_apps = mock.Mock()
    pkg.setup_logging = mock.Mock()
    cmake = types.ModuleType('idf_build_apps.app')
    cmake.CMakeApp = FakeCMakeApp
    spec = importlib.util.spec_from_file_location('board_manager_build_apps_under_test', BUILD_APPS)
    module = importlib.util.module_from_spec(spec)
    with mock.patch.dict(sys.modules, {'idf_build_apps': pkg, 'idf_build_apps.app': cmake}):
        spec.loader.exec_module(module)
    return module


class BoardManagerExtraActionsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build_apps = load_build_apps()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.project = Path(self.tmp.name)
        self.bmgr = self.project / 'managed_components' / 'espressif__esp_board_manager'
        self.defaults = self.project / 'components' / 'gen_bmgr_codes' / 'board_manager.defaults'
        self.app = self.build_apps.CustomApp()
        self.app.work_dir = str(self.project)
        self.app.name = 'board_manager_fixture'
        self.app.target = 'esp32s3'
        self.app._sdkconfig_files = []
        self.app.clear_project_generated_files = mock.Mock()
        self.app.is_board_manager_project = mock.Mock(return_value=True)

        def download():
            self.bmgr.mkdir(parents=True)
            return True

        self.app._download_bmgr_component = mock.Mock(side_effect=download)

    def check_build(self, original, expected, *, generator_fails=False, parent_fails=False):
        seen_generator = []

        def generate(*args, **kwargs):
            self.assertIn('gen-bmgr-config', args[0])
            self.assertTrue(kwargs['check'])
            self.assertEqual(kwargs['cwd'], str(self.project))
            value = kwargs['env'].get('IDF_EXTRA_ACTIONS_PATH')
            seen_generator.append(value)
            self.assertEqual(value, expected)
            if generator_fails:
                raise subprocess.CalledProcessError(1, args[0])
            self.defaults.parent.mkdir(parents=True, exist_ok=True)
            self.defaults.write_text('CONFIG_TEST_BOARD=y\n', encoding='utf-8')

        new_env = {} if original is None else {'IDF_EXTRA_ACTIONS_PATH': original}
        self.app.fail_parent_build = parent_fails
        with (
            mock.patch.dict(os.environ, new_env, clear=True),
            mock.patch.object(self.build_apps, 'IDF_PATH', '/fake/esp-idf'),
            mock.patch.object(self.build_apps, 'BOARD_NAME', 'esp32_s3_box_3'),
            mock.patch.object(self.build_apps.subprocess, 'run', side_effect=generate),
        ):
            if generator_fails:
                with self.assertRaises(subprocess.CalledProcessError):
                    self.app._build()
            elif parent_fails:
                with self.assertRaisesRegex(RuntimeError, 'Simulated IDF build failure'):
                    self.app._build()
            else:
                self.app._build()
            # Restored on success and on either pre-hook/build failure.
            self.assertEqual(os.environ.get('IDF_EXTRA_ACTIONS_PATH'), original)

        self.assertEqual(seen_generator, [expected])
        if not generator_fails:
            self.assertEqual(self.app.parent_build_extra_actions, expected)
            self.assertEqual(self.app._sdkconfig_files, [str(self.defaults)])
        return expected

    def test_without_existing_extra_actions(self):
        self.check_build(None, str(self.bmgr))

    def test_empty_existing_extra_actions(self):
        self.check_build('', str(self.bmgr))

    def test_preserve_one_existing_extension(self):
        self.check_build('/custom/actions', '/custom/actions;' + str(self.bmgr))

    def test_preserve_multiple_existing_extensions_in_order(self):
        self.check_build('/first/actions;/second/actions', '/first/actions;/second/actions;' + str(self.bmgr))

    def test_do_not_append_existing_board_manager_twice(self):
        original = '/first/actions;' + str(self.bmgr) + ';/second/actions'
        self.check_build(original, original)

    def test_normalized_existing_board_manager_does_not_duplicate(self):
        original = '/first/actions;' + str(self.bmgr) + '/.;/second/actions'
        self.check_build(original, original)

    def test_restore_extensions_on_generator_failure(self):
        self.check_build('/custom/actions', '/custom/actions;' + str(self.bmgr), generator_fails=True)

    def test_restore_extensions_on_parent_build_failure(self):
        self.check_build('/custom/actions', '/custom/actions;' + str(self.bmgr), parent_fails=True)


if __name__ == '__main__':
    unittest.main()
