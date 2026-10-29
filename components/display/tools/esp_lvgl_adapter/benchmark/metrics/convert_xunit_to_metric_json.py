#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import os
import json
import argparse
import xml.etree.ElementTree as ET
from datetime import datetime
from collections import OrderedDict


def parse_args() -> argparse.Namespace:

    parser = argparse.ArgumentParser(description='Convert XUNIT_RESULT.xml to metrics JSON')
    parser.add_argument('xunit_path', help='Path to XUNIT_RESULT.xml')
    return parser.parse_args()


def get_env_strs() -> tuple[str, str]:

    gitlab_pipeline_id = os.environ.get('CI_PIPELINE_ID', '')
    gitlab_project_id = os.environ.get('CI_PROJECT_ID', '')
    return gitlab_pipeline_id, gitlab_project_id


def parse_testsuite_timestamp(ts: str) -> int:

    # Convert ISO 8601 to epoch seconds; fall back to current time on failure
    try:
        return int(datetime.fromisoformat(ts).timestamp())
    except Exception:
        return int(datetime.now().timestamp())


def load_xunit_properties(xunit_path: str) -> dict:

    tree = ET.parse(xunit_path)
    root = tree.getroot()

    # Find first testcase node
    testcase = None
    testsuite = None
    for ts in root.iter('testsuite'):
        testsuite = ts
        for tc in ts.iter('testcase'):
            testcase = tc
            break
        if testcase is not None:
            break

    if testcase is None:
        raise RuntimeError('No <testcase> found in XUnit XML')

    # Extract properties from testcase
    props = {}
    for props_node in testcase.findall('properties'):
        for prop in props_node.findall('property'):
            name = prop.get('name')
            value = prop.get('value')
            if name is not None:
                props.setdefault(name, []).append(value)

    # Attach testsuite timestamp if present
    if testsuite is not None:
        props['testsuite_timestamp'] = [testsuite.get('timestamp', '')]

    return props


def get_all_resolutions() -> list:

    # Keep in sync with k_test_resolutions in test_esp_lv_adapter_fps.c
    return [
        (128, 64), (160, 80),
        (128, 128), (128, 160), (240, 135), (240, 240), (240, 320),
        (320, 240), (320, 320), (320, 480),
        (360, 360), (368, 448), (390, 390), (400, 240), (400, 400),
        (412, 412), (454, 454), (480, 272), (480, 320), (480, 480),
        (600, 600), (640, 360), (640, 480),
        (720, 480), (720, 720), (720, 1280), (800, 256), (800, 480),
        (800, 600), (960, 540), (1024, 576), (1024, 600), (1024, 768),
        (1080, 1920), (1152, 864), (1280, 400), (1280, 480), (1280, 720),
        (1280, 768), (1280, 800), (1280, 1024), (1360, 768), (1366, 768),
        (1400, 1050), (1440, 900), (1600, 900), (1680, 1050),
        (1920, 360), (1920, 540), (1920, 720), (1920, 1080), (1920, 1200),
        (2048, 1080),
    ]


def sort_resolutions(res_list: list) -> list:

    # Sort by pixel count, then width, then height
    return sorted(res_list, key=lambda wh: (wh[0] * wh[1], wh[0], wh[1]))


def make_resolution_key(width: int, height: int) -> str:

    return f'{width}x{height}'


def build_results(props: dict) -> tuple[OrderedDict, int, str]:

    # Parse all fps_benchmark properties
    parsed_by_res: dict[str, dict] = {}
    pixel_format = ''
    for value in props.get('fps_benchmark', []):
        try:
            bench = json.loads(value)
        except Exception:
            continue
        res_key = bench.get('resolution')
        if not res_key:
            continue
        # Extract color_format from first fps_benchmark entry
        if not pixel_format:
            pixel_format = bench.get('pixel_format', '')
        # Normalize numeric fields to numbers
        result_obj = {
            'avg': bench.get('avg'),
            'min': bench.get('min'),
            'p10': bench.get('p10'),
            'p25': bench.get('p25'),
            'p50': bench.get('p50'),
            'p75': bench.get('p75'),
            'p90': bench.get('p90'),
            'max': bench.get('max'),
            'samples': bench.get('samples'),
            'filtered': bench.get('filtered'),
        }
        parsed_by_res[res_key] = result_obj

    all_res = sort_resolutions(get_all_resolutions())
    ordered = OrderedDict()
    for w, h in all_res:
        key = make_resolution_key(w, h)
        ordered[key] = parsed_by_res.get(key, None)

    data_count = sum(1 for v in ordered.values() if v is not None)
    return ordered, data_count, pixel_format


def main() -> None:

    args = parse_args()
    xunit_path = args.xunit_path

    gitlab_pipeline_id, gitlab_project_id = get_env_strs()

    props = load_xunit_properties(xunit_path)

    # metadata basics
    idf_version = (props.get('idf_version', ['']) or [''])[0]
    target = (props.get('idf_target', ['']) or [''])[0]
    ts = (props.get('testsuite_timestamp', ['']) or [''])[0]
    test_time = parse_testsuite_timestamp(ts) if ts else int(datetime.now().timestamp())

    result_obj, data_count, pixel_format = build_results(props)

    metric_data = OrderedDict()
    metric_data['esp_idf_version'] = idf_version
    metric_data['target'] = target
    metric_data['test_time'] = test_time
    metric_data['pixel_format'] = pixel_format
    metric_data['data_count'] = data_count

    metric_data.update(result_obj)

    metric_name = 'AE LVGL Framerate Test'
    out = OrderedDict()
    out[metric_name] = metric_data

    print(json.dumps(out, ensure_ascii=False, separators=(',', ':')))


if __name__ == '__main__':
    main()
