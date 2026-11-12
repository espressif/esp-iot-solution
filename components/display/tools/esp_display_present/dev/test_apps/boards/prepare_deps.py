# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Copy pinned, locally cached drivers; no network or BSP service initialization."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

PACKAGES = {
    'esp_lcd_touch': ('1.2.1_3f85a7d9', 'esp_lcd_touch.c', 'driver esp_lcd', None),
    'esp_lcd_ili9341': ('2.1.0_840387ee', 'esp_lcd_ili9341.c', 'esp_lcd esp_driver_gpio', (2, 1, 0)),
    'esp_lcd_co5300': ('2.2.0_1a5cbaa0', 'esp_lcd_co5300.c', 'esp_lcd esp_driver_gpio', (2, 2, 0)),
    'esp_lcd_gc9503': ('3.0.1_6940bf99', 'esp_lcd_gc9503.c', 'esp_lcd esp_driver_gpio esp_lcd_panel_io_additions', (3, 0, 1)),
    'esp_lcd_panel_io_additions': ('1.0.1~1_1856c8b2', 'esp_lcd_panel_io_3wire_spi.c', 'esp_lcd esp_driver_gpio esp_io_expander', (1, 0, 1)),
    'esp_io_expander': ('1.2.1_7b7f9127', 'esp_io_expander.c', 'esp_driver_gpio', None),
    'esp_io_expander_tca9554': ('2.0.3_ee94a439', 'esp_io_expander_tca9554.c', 'esp_driver_i2c esp_io_expander', None),
    'esp_lcd_ek79007': ('2.0.2_7a0896e1', 'esp_lcd_ek79007.c', 'esp_lcd esp_driver_gpio', (2, 0, 2)),
}
p = argparse.ArgumentParser()
p.add_argument('--cache', type=Path, default=Path.home()/'.cache/Espressif/ComponentManager/service_d92d8f1e')
p.add_argument('--output', type=Path, help='External output directory (recommended for unified runner)')
a = p.parse_args()
root = (a.output or Path(__file__).resolve().parent/'deps').resolve()
root.mkdir(parents=True, exist_ok=True)
manifest = {}
for name, (version, source, requires, ver) in PACKAGES.items():
    origin = a.cache/f'espressif__{name}_{version}'
    if not origin.is_dir():
        raise SystemExit(f'Missing pinned package: {origin}')
    dest = root/name
    shutil.copytree(origin, dest, dirs_exist_ok=True)
    (dest/'idf_component.yml').unlink(missing_ok=True)
    cmake = f'idf_component_register(SRCS "{source}" INCLUDE_DIRS "include" REQUIRES {requires})\n'
    if name == 'esp_lcd_co5300':
        cmake = 'idf_component_register(SRCS esp_lcd_co5300.c esp_lcd_co5300_spi.c esp_lcd_co5300_mipi.c INCLUDE_DIRS include PRIV_INCLUDE_DIRS priv_include REQUIRES esp_lcd esp_driver_gpio)\n'
    if ver:
        cmake += 'target_compile_definitions(${COMPONENT_LIB} PRIVATE ' + ' '.join(f'{name.upper()}_VER_{k}={v}' for k,v in zip(('MAJOR','MINOR','PATCH'),ver)) + ')\n'
    (dest/'CMakeLists.txt').write_text(cmake)
    manifest[name] = {'cache_package':origin.name, 'source_sha256':hashlib.sha256((origin/source).read_bytes()).hexdigest()}
(root/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
print(root)
