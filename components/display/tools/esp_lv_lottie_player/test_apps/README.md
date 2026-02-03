# ESP LVGL Lottie Player Test Applications

This directory contains test applications for the ESP LVGL Lottie Player component, organized by LVGL version.

## Directory Structure

```
test_apps/
├── lvgl8/          # Test application for LVGL v8.x
│   ├── main/
│   │   ├── test_lottie_player.c
│   │   ├── CMakeLists.txt
│   │   └── idf_component.yml  # Dependencies: lvgl ^8
│   ├── CMakeLists.txt
│   ├── partitions.csv
│   └── sdkconfig.defaults
└── lvgl9/          # Test application for LVGL v9.x
    ├── main/
    │   ├── test_lottie_player.c
    │   ├── CMakeLists.txt
    │   └── idf_component.yml  # Dependencies: lvgl ^9
    ├── CMakeLists.txt
    ├── partitions.csv
    └── sdkconfig.defaults
```

## Building and Running

### For LVGL v8
```bash
cd lvgl8
idf.py build flash monitor
```

### For LVGL v9
```bash
cd lvgl9
idf.py build flash monitor
```

## Test Features

- Embedded Lottie JSON using EMBED_TXTFILES
- LVGL v8 and v9 compatibility verification
- Memory leak detection
- Basic render validation
