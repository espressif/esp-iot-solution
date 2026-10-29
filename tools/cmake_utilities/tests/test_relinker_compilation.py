# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""Tests for build-graph-dependent relinker rules (old/new ESP-IDF layouts)."""

import contextlib
import csv
import io
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts' / 'relinker'))
import configuration


BT_OBJECT = 'esp-idf/bt/CMakeFiles/__idf_bt.dir/porting/mem/bt_osi_mem.c.obj'
NIMBLE_OBJECT = 'esp-idf/bt/CMakeFiles/__idf_bt.dir/host/nimble/port/src/esp_nimble_mem.c.obj'
CONTROLLER_OBJECT = 'esp-idf/bt/CMakeFiles/__idf_bt.dir/controller/esp32c3/bt.c.obj'
BT_FUNCTIONS = (
    'bt_osi_mem_calloc',
    'bt_osi_mem_malloc',
    'bt_osi_mem_malloc_internal',
    'bt_osi_mem_free',
)
NIMBLE_FUNCTIONS = ('nimble_mem_calloc', 'nimble_mem_malloc', 'nimble_mem_free')


class RelinkerCompiledObjectTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.work_dir = Path(self.temp.name)
        self.library = self._write('library.csv', 'library,path\nlibbt.a,./esp-idf/bt/libbt.a\n')
        self.objects = self._write(
            'object.csv',
            'library,object,path\n'
            f'libbt.a,bt_osi_mem.c.obj,{BT_OBJECT}\n'
            f'libbt.a,esp_nimble_mem.c.obj,{NIMBLE_OBJECT}\n'
            f'libbt.a,bt.c.obj,{CONTROLLER_OBJECT}\n',
        )
        functions = 'library,object,function,option,if_compiled\n'
        functions += ''.join(
            f'libbt.a,bt_osi_mem.c.obj,{name},CONFIG_BT_ENABLED,true\n'
            for name in BT_FUNCTIONS
        )
        functions += ''.join(
            f'libbt.a,esp_nimble_mem.c.obj,{name},'
            'CONFIG_BT_NIMBLE_ENABLED && !CONFIG_BT_NIMBLE_LOW_SPEED_MODE,true\n'
            for name in NIMBLE_FUNCTIONS
        )
        functions += 'libbt.a,bt.c.obj,esp_reset_rpa_moudle,CONFIG_BT_ENABLED,\n'
        self.functions = self._write('function.csv', functions)
        self.sdkconfig = self._write('sdkconfig', 'CONFIG_BT_ENABLED=y\nCONFIG_BT_NIMBLE_ENABLED=y\n')
        self.compile_commands = self.work_dir / 'compile_commands.json'

    def _write(self, filename, content):
        path = self.work_dir / filename
        path.write_text(content, encoding='utf-8')
        return path

    def _write_commands(self, *outputs, use_output_field=True):
        commands = []
        for output in outputs:
            entry = {
                'directory': str(self.work_dir),
                'file': '/idf/components/bt/porting/mem/bt_osi_mem.c',
                'command': f'cc -c source.c -o {output}',
            }
            if use_output_field:
                entry['output'] = output
            commands.append(entry)
        self.compile_commands.write_text(json.dumps(commands), encoding='utf-8')

    def _generate(self, compile_commands_file=None):
        symbol_lines = [
            f'00000000 g F .iram1.{name} 00000010 {name}\n'
            for name in (*BT_FUNCTIONS, *NIMBLE_FUNCTIONS, 'esp_reset_rpa_moudle')
        ]
        with mock.patch.object(
            configuration.subprocess,
            'check_output',
            return_value=''.join(symbol_lines).encode(),
        ) as run_objdump:
            with contextlib.redirect_stdout(io.StringIO()):
                libs = configuration.generator(
                    str(self.library),
                    str(self.objects),
                    str(self.functions),
                    str(self.sdkconfig),
                    True,
                    'objdump',
                    compile_commands_file,
                )
        return libs, run_objdump

    def test_old_idf_keeps_relocations_before_object_file_exists(self):
        self._write_commands(BT_OBJECT, CONTROLLER_OBJECT)
        self.assertFalse((self.work_dir / BT_OBJECT).exists())
        with mock.patch('os.getcwd', return_value=str(self.work_dir)):
            libs, objdump = self._generate(str(self.compile_commands))
        funcs = libs.libs['libbt.a'].objs['bt_osi_mem.c.obj'].functions()
        self.assertEqual(set(funcs), set(BT_FUNCTIONS))
        self.assertEqual(objdump.call_count, 2)

    def test_new_idf_relocates_nimble_host_functions(self):
        self._write_commands(NIMBLE_OBJECT, CONTROLLER_OBJECT)
        with mock.patch('os.getcwd', return_value=str(self.work_dir)):
            libs, objdump = self._generate(str(self.compile_commands))
        objects = libs.libs['libbt.a'].objs
        self.assertEqual(list(objects), ['esp_nimble_mem.c.obj', 'bt.c.obj'])
        self.assertEqual(set(objects['esp_nimble_mem.c.obj'].functions()), set(NIMBLE_FUNCTIONS))
        self.assertNotIn('bt_osi_mem.c.obj', objects)
        self.assertEqual(objdump.call_count, 2)
        self.assertEqual(objdump.call_args_list[0].args[0][2], NIMBLE_OBJECT)

    def test_missing_old_and_new_objects_skips_both_optional_rule_sets(self):
        self._write_commands(CONTROLLER_OBJECT)
        with mock.patch('os.getcwd', return_value=str(self.work_dir)):
            libs, objdump = self._generate(str(self.compile_commands))
        self.assertEqual(list(libs.libs['libbt.a'].objs), ['bt.c.obj'])
        self.assertEqual(objdump.call_count, 1)

    def test_low_speed_mode_keeps_nimble_functions_in_default_sections(self):
        self._write_commands(NIMBLE_OBJECT, CONTROLLER_OBJECT)
        self.sdkconfig.write_text(
            'CONFIG_BT_ENABLED=y\nCONFIG_BT_NIMBLE_ENABLED=y\n'
            'CONFIG_BT_NIMBLE_LOW_SPEED_MODE=y\n',
            encoding='utf-8',
        )
        with mock.patch('os.getcwd', return_value=str(self.work_dir)):
            libs, objdump = self._generate(str(self.compile_commands))
        self.assertEqual(list(libs.libs['libbt.a'].objs), ['bt.c.obj'])
        self.assertEqual(objdump.call_count, 1)

    def test_older_compilation_database_without_output_field(self):
        self._write_commands(BT_OBJECT, use_output_field=False)
        outputs = configuration.compiled_object_paths(str(self.compile_commands))
        self.assertIn(str(self.work_dir / BT_OBJECT), outputs)

    def test_missing_compile_commands_is_an_error_for_conditional_rule(self):
        with self.assertRaisesRegex(ValueError, 'if_compiled requires'):
            self._generate()

    def test_expected_object_missing_remains_a_build_error(self):
        self._write_commands(BT_OBJECT)
        with mock.patch('os.getcwd', return_value=str(self.work_dir)):
            with mock.patch.object(
                configuration.subprocess,
                'check_output',
                side_effect=subprocess.CalledProcessError(1, ['objdump']),
            ):
                with self.assertRaisesRegex(RuntimeError, 'failed with exit code 1'):
                    configuration.generator(
                        str(self.library), str(self.objects), str(self.functions),
                        str(self.sdkconfig), True, 'objdump', str(self.compile_commands),
                    )

    def test_legacy_four_column_config_needs_no_compilation_database(self):
        self.functions.write_text(
            'library,object,function,option\n'
            'libbt.a,bt.c.obj,esp_reset_rpa_moudle,CONFIG_BT_ENABLED\n',
            encoding='utf-8',
        )
        libs, objdump = self._generate()
        self.assertEqual(list(libs.libs['libbt.a'].objs), ['bt.c.obj'])
        self.assertEqual(objdump.call_count, 1)

    def test_repository_csv_maps_both_idf_layouts(self):
        directory = (
            Path(__file__).resolve().parents[1]
            / 'scripts' / 'relinker' / 'examples' / 'iram_strip' / 'esp32c3' / '5.3'
        )
        with (directory / 'function.csv').open(newline='', encoding='utf-8') as stream:
            rules = list(csv.DictReader(stream))
        with (directory / 'object.csv').open(newline='', encoding='utf-8') as stream:
            paths = {row['object']: row['path'] for row in csv.DictReader(stream)}

        old_rules = [row for row in rules if row['object'] == 'bt_osi_mem.c.obj']
        new_rules = [row for row in rules if row['object'] == 'esp_nimble_mem.c.obj']
        self.assertEqual({row['function'] for row in old_rules}, set(BT_FUNCTIONS))
        self.assertEqual({row['function'] for row in new_rules}, set(NIMBLE_FUNCTIONS))
        self.assertEqual(paths['bt_osi_mem.c.obj'], BT_OBJECT)
        self.assertEqual(paths['esp_nimble_mem.c.obj'], NIMBLE_OBJECT)
        self.assertTrue(all(row['if_compiled'] == 'true' for row in old_rules + new_rules))
        self.assertTrue(
            all('!CONFIG_BT_NIMBLE_LOW_SPEED_MODE' in row['option'] for row in new_rules)
        )

    def test_disabled_bluetooth_skips_conditional_rules(self):
        self.sdkconfig.write_text('', encoding='utf-8')
        libs, objdump = self._generate()
        self.assertEqual(libs.libs, {})
        objdump.assert_not_called()


if __name__ == '__main__':
    unittest.main()
