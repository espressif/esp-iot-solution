# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

from pathlib import Path

import generateFiles
import yaml


def test_find_build_dir_discovers_new_idf_versions(tmp_path: Path) -> None:
    app_dir = tmp_path / 'example'
    (app_dir / '5.4' / 'build_esp32p4_defaults').mkdir(parents=True)
    (app_dir / '6.1' / 'build_esp32p4_defaults').mkdir(parents=True)
    (app_dir / 'latest' / 'build_esp32p4_defaults').mkdir(parents=True)

    assert generateFiles.find_build_dir(str(app_dir), 'build_esp32p4_defaults') == [
        (str(Path('5.4') / 'build_esp32p4_defaults'), 'v5.4'),
        (str(Path('6.1') / 'build_esp32p4_defaults'), 'v6.1'),
    ]


def test_find_configured_build_dir_uses_target_and_config(tmp_path: Path) -> None:
    app_dir = tmp_path / 'example'
    (app_dir / '6.1' / 'build_esp32s31_defaults').mkdir(parents=True)

    assert generateFiles.find_configured_build_dir(
        str(app_dir), 'esp32s31', 'defaults'
    ) == [(str(Path('6.1') / 'build_esp32s31_defaults'), 'v6.1')]


def test_upload_config_discovers_build_missing_from_find_output(
    tmp_path: Path, monkeypatch
) -> None:
    app_dir = tmp_path / 'usb_extend_screen'
    (app_dir / '6.1' / 'build_esp32s31_defaults').mkdir(parents=True)
    manifest_dir = app_dir / 'main'
    manifest_dir.mkdir()
    (manifest_dir / 'idf_component.yml').write_text('version: 0.3.0\n', encoding='utf-8')

    upload_config = tmp_path / 'upload_project_config.yml'
    upload_config.write_text(
        yaml.safe_dump({
            str(app_dir): {
                'esp32s31': {'sdkconfig': ['defaults']},
                'description': 'USB extension screen',
            },
        }),
        encoding='utf-8',
    )
    monkeypatch.setattr(generateFiles, 'PROJECT_CONFIG_FILE', str(upload_config))

    apps = generateFiles.remove_app_from_config([])

    assert len(apps) == 1
    assert apps[0]['app_version'] == '0.3.0'
    assert apps[0]['build_info'] == [{
        'target': 'esp32s31',
        'sdkconfig': 'esp32s31_generic',
        'build_dir': str(Path('6.1') / 'build_esp32s31_defaults'),
        'idf_version': 'v6.1',
    }]
