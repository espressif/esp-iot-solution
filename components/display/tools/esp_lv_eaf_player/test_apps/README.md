# ESP EAF Player Test Applications

This directory contains test applications for the ESP EAF Player component, organized by LVGL version.

## Directory Structure

```
test_apps/
├── lvgl8/          # Test application for LVGL v8.x
│   ├── main/
│   │   ├── test_eaf_player.c
│   │   ├── test.eaf           # 196KB embedded EAF animation (55 frames)
│   │   ├── CMakeLists.txt
│   │   └── idf_component.yml  # Dependencies: lvgl ^8
│   ├── CMakeLists.txt
│   ├── partitions.csv
│   └── sdkconfig.defaults
└── lvgl9/          # Test application for LVGL v9.x
    ├── main/
    │   ├── test_eaf_player.c
    │   ├── test.eaf           # 196KB embedded EAF animation (55 frames)
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

- ✅ Embedded EAF file (196KB, 55 frames) using EMBED_TXTFILES
- ✅ LVGL v8 and v9 compatibility verification
- ✅ Memory leak detection
- ✅ Animation playback validation
- ✅ 240x240 pixel display simulation

## Test Output

Successful test output should show:
```
I (3170) eaf player: Embedded EAF size: 200253 bytes
I (3190) eaf player: Total frames: 55
I (3190) eaf player: flush_cb [0,0,239,239]
MALLOC_CAP_8BIT usage: Free memory delta: 0
Test ran in 30ms
PASS
```
