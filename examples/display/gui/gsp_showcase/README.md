| Supported Targets | ESP32-P4 | ESP32-S3 | ESP32-S31 | ESP32-C3 |
| ----------------- | -------- | -------- | --------- | -------- |

# ESP-GSP showcase

Four interactive scenes introduce controls, compiled and runtime images, GIF
and native animation, charts, lists, and full-screen navigation. This example
uses `espressif/esp-gsp` version `1.5.*`. The UI is in `scenes/`, shared device/PC
interactions are in `main/showcase_ui.c`, and board setup is in
`main/app_main.c`.

The 0° layouts correspond to `common/hw_init` interfaces:

| Interface | Resolution | Scene files | Input |
| --------- | ---------- | ----------- | ----- |
| P4 MIPI DSI | 1024×600 | `scenes/*.json` | touch |
| S3/S31 RGB | 800×480 | `scenes/800/*.json` | touch |
| S3 QSPI | 360×360 | `scenes/360/*.json` | touch |
| S3 SPI with PSRAM | 320×240 | `scenes/320/*.json` | touch |
| C3 SPI without PSRAM | 240×240 | `scenes/240/*.json` | knob |

On touch panels, swipe horizontally across the screen to change pages and pull
down from the top for the Quick Panel. On the 240×240 board, turn the knob to
change pages. A short click performs the page action; on Controls, hold to enter
or leave edit mode, turn to focus Add 10, Reset, Level, Power, Alerts or either
radio choice, then click to apply. On the other pages, hold to open/close the
Quick Panel. The Controls header shows the current knob focus.

Use ESP-IDF 6.1 or newer, with `esp-gsp-tools` installed in its Python
environment. From this example directory:

```sh
python -m pip install -U esp-gsp-tools
idf.py set-target esp32p4 build
idf.py -p PORT flash monitor
```

Change the target and port for your board. For separate configurations, use
separate build directories:

```sh
# P4 revision 1.x (pre-v3 panel)
idf.py -B build_p4_rev1 -D SDKCONFIG=build_p4_rev1/sdkconfig \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.p4_rev1' set-target esp32p4 build

# S3 QSPI 360×360
idf.py -B build_s3_qspi -D SDKCONFIG=build_s3_qspi/sdkconfig \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.qspi' set-target esp32s3 build

# S3 RGB 800×480
idf.py -B build_s3_rgb -D SDKCONFIG=build_s3_rgb/sdkconfig \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.rgb' set-target esp32s3 build

# S31 RGB 800×480 (preview target)
idf.py --preview -B build_s31_rgb -D SDKCONFIG=build_s31_rgb/sdkconfig \
  set-target esp32s31 build

# C3 SPI 240×240
idf.py -B build_c3_spi -D SDKCONFIG=build_c3_spi/sdkconfig set-target esp32c3 build
```

When flashing or monitoring a separate configuration, keep the same build
directory and sdkconfig. For example, after building the S3 QSPI configuration:

```sh
idf.py -B build_s3_qspi -D SDKCONFIG=build_s3_qspi/sdkconfig -p PORT flash monitor
```

This repository example uses the local `esp_display_present` component through
`override_path`, so firmware builds and CI validate the presenter in the same
checkout.

The native PC simulator runs the same C event/timer/image logic as the firmware
and uses the 1024×600 scene set. After one `idf.py reconfigure` installs the
component:

```sh
python managed_components/espressif__esp-gsp/tools/sim_bridge/run.py --project pc
```

Match panel pins and timings to your board before flashing. The example uses
`common/hw_init`. The PNG/GIF teaching assets come
from ESP-GSP 1.5.0 Component Registry examples; `assets/NOTICE` records their
source and license.
