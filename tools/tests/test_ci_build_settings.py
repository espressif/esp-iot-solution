# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""Host-only checks for CI build limits and application partition sizing."""

import csv
from pathlib import Path
import unittest

import yaml


ROOT = Path(__file__).resolve().parents[2]


class GitLabCILoader(yaml.SafeLoader):
    """Parse GitLab !reference syntax without evaluating CI inheritance."""


GitLabCILoader.add_constructor('!reference', lambda loader, node: loader.construct_sequence(node))


class CIBuildRegressionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with (ROOT / '.gitlab/ci/build.yml').open(encoding='utf-8') as stream:
            cls.jobs = yaml.load(stream, Loader=GitLabCILoader)

    def test_opencv_caps_outer_ninja_and_nested_cmake(self):
        variables = self.jobs['build_example_vision_opencv_color_tracker']['variables']
        self.assertEqual(variables['IDF_PY_BUILD_JOBS'], '2')
        self.assertEqual(variables['CMAKE_BUILD_PARALLEL_LEVEL'], '2')

    def test_imu_gesture_limits_tflite_compilation_parallelism(self):
        variables = self.jobs['build_components_imu_gesture_test_apps']['variables']
        self.assertEqual(variables['IDF_PY_BUILD_JOBS'], '2')

    def test_imu_gesture_full_matrix_is_split_without_losing_targets(self):
        regular = self.jobs['build_components_imu_gesture_test_apps']
        inference = self.jobs['build_components_imu_gesture_inference_test_apps']
        self.assertEqual(
            regular['variables']['EXAMPLE_DIR'].split(),
            [
                'components/imu_gesture/test_apps/knob',
                'components/imu_gesture/test_apps/space_switch',
            ],
        )
        self.assertIn('.build_idf_active_release_version', regular['extends'])
        self.assertEqual(inference['variables']['EXAMPLE_DIR'], 'components/imu_gesture/test_apps/inference')
        self.assertEqual(inference['variables']['IDF_PY_BUILD_JOBS'], '2')
        matrix = inference['parallel']['matrix'][0]
        self.assertEqual(
            matrix['IMAGE'],
            [
                'espressif/idf:release-v5.3',
                'espressif/idf:release-v5.4',
                'espressif/idf:release-v5.5',
            ],
        )
        self.assertEqual(matrix['IMU_BUILD_PARTITION'], ['1', '2'])
        self.assertIn('--parallel-count 2', inference['variables']['EXAMPLE_APPEND_ARGS'])
        self.assertIn('--parallel-index ${IMU_BUILD_PARTITION}', inference['variables']['EXAMPLE_APPEND_ARGS'])

    def test_ramfs_p4_uses_known_working_function_ev_board(self):
        with (ROOT / '.gitlab/ci/target_test.yml').open(encoding='utf-8') as stream:
            tests = yaml.load(stream, Loader=GitLabCILoader)
        general = tests['components_test_ramfs']
        p4 = tests['components_test_ramfs_p4']
        self.assertNotIn('esp32p4', [cell['IDF_TARGET'] for cell in general['parallel']['matrix']])
        self.assertEqual(p4['extends'], ['components_test_ramfs'])
        self.assertEqual(
            p4['parallel']['matrix'],
            [{'IDF_TARGET': 'esp32p4', 'IDF_VERSION': '5.5', 'ENV_TAG': 'rev_default'}],
        )
        self.assertEqual(p4['tags'], ['esp32p4', 'rev_default', 'SH-AE-TEST-R17'])
        self.assertEqual(p4['retry'], {'max': 2, 'when': ['script_failure']})

    def test_imu_inference_limits_ninja_on_idf_5_2_to_5_4(self):
        source = (ROOT / 'components/imu_gesture/test_apps/inference/CMakeLists.txt').read_text(
            encoding='utf-8'
        )
        self.assertIn('set_property(GLOBAL PROPERTY JOB_POOLS imu_compile_pool=2)', source)
        self.assertIn('set(CMAKE_JOB_POOL_COMPILE imu_compile_pool)', source)
        self.assertLess(source.index('set(CMAKE_JOB_POOL_COMPILE'), source.index('project('))

    def test_mcp_client_factory_partition_has_room_for_binary(self):
        path = ROOT / 'examples/mcp/mcp_client/partitions.csv'
        with path.open(newline='', encoding='utf-8') as stream:
            rows = [
                [field.strip() for field in row]
                for row in csv.reader(line for line in stream if not line.lstrip().startswith('#'))
            ]
        factory = [row for row in rows if row and row[0] == 'factory']
        self.assertEqual(len(factory), 1)
        offset = 0x10000
        size = int(factory[0][4], 0)
        # Master pipeline #1714502 generated 0x110080 bytes for esp32c6,
        # overflowing the previous 0x110000-byte factory partition.
        self.assertGreaterEqual(size, 0x140000)
        self.assertEqual(size % 0x10000, 0)
        self.assertLessEqual(offset + size, 0x400000)


if __name__ == '__main__':
    unittest.main()
