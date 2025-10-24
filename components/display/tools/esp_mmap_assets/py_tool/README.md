# ESP MMAP Assets Standalone Build Tool

## Introduction

This is a standalone Python build tool that can build ESP MMAP asset files directly without depending on CMake.

## Features

- ✅ **No configuration file required**: Pure command-line parameters, simple and direct
- ✅ **Standalone execution**: No CMake environment required
- ✅ **Auto dependency installation**: Automatically installs required Python packages on first run
- ✅ **Complete functionality**: Supports all format conversion features

## Quick Start

### Dependencies

The script will **automatically check and install** required dependencies. If automatic installation fails, you can install manually:

```bash
pip install Pillow numpy qoi-conv packaging
```

### Basic Usage

```bash
python assets_gen.py \
    --assets-path ./my_images \
    --output ./build/assets.bin
```

## Common Parameters

### Required Parameters

| Parameter | Description |
|-----------|-------------|
| `--assets-path` | Asset file directory path |
| `--output` | Output binary file path |

### Format Options

| Parameter | Description |
|-----------|-------------|
| `--support-spng` | Enable sliced PNG |
| `--support-sjpg` | Enable sliced JPEG |
| `--support-qoi` | Enable QOI format |
| `--support-sqoi` | Enable sliced QOI |
| `--support-pjpg` | Enable progressive JPEG |
| `--split-height N` | Slice height (pixels) |

### Other Common Options

| Parameter | Description | Default |
|-----------|-------------|---------|
| `--partition-size` | Partition size (hexadecimal) | 0x1000000 (16MB) |
| `--name-length` | Maximum filename length | 32 |
| `--support-format` | Supported file formats | .png,.jpg |
| `--partition-name` | Partition name | assets |

## Usage Examples

### Example 1: Basic Image Packaging

```bash
python assets_gen.py \
    --assets-path ./images \
    --output ./build/assets.bin
```

### Example 2: Using PNG Slicing for Memory Optimization

```bash
python assets_gen.py \
    --assets-path ./images \
    --output ./build/assets.bin \
    --support-spng \
    --split-height 16
```

### Example 3: Supporting Both PNG and JPEG Slicing

```bash
python assets_gen.py \
    --assets-path ./images \
    --output ./build/assets.bin \
    --support-spng \
    --support-sjpg \
    --split-height 16 \
    --support-format ".png,.jpg"
```

### Example 4: Using QOI Format

```bash
python assets_gen.py \
    --assets-path ./images \
    --output ./build/assets.bin \
    --support-qoi
```

## Output Files

After successful build, the following files will be generated in the output directory:

1. **Binary file**: `<output>` - Packaged assets
2. **Header file**: `<output_dir>/mmap_generate_<name>.h` - C header file (in the same directory as binary file)
3. **Temporary config**: `<output_dir>/.build_config_tmp.json` - Internal temporary configuration

**Note**: Header files are generated directly in the output directory without creating subdirectories.

## How It Works

The script will:

1. Create a temporary configuration file from command-line parameters
2. Call `spiffs_assets_gen.py` for actual building
3. Save configuration file as `.build_config_tmp.json` in output directory

## Alternative to CMake Usage

**Original CMake usage:**
```cmake
COMMAND ${python} ${MVMODEL_EXE} --config "${CONFIG_FILE_PATH}" --build
```

**Current usage:**
```bash
python assets_gen.py \
    --assets-path /path/to/assets \
    --output /path/to/output.bin
```

## FAQ

### Q: How to view all parameters?

```bash
python assets_gen.py --help
```

### Q: What if dependency installation fails?

The script will automatically install dependencies. If automatic installation fails, please install manually:

```bash
pip install Pillow numpy qoi-conv packaging
```

Or use domestic mirror sources:

```bash
pip install -i https://pypi.tuna.tsinghua.edu.cn/simple Pillow numpy qoi-conv packaging
```

### Q: How to choose slice height?

- For memory-constrained devices, recommend 8-16 pixels
- For devices with sufficient memory, can use 32-64 pixels
- Smaller height means less memory usage but slightly larger files

### Q: How to specify custom partition size?

```bash
--partition-size 0x200000  # 2MB
--partition-size 0x400000  # 4MB
--partition-size 0x1000000 # 16MB (default)
```

### Q: How to package multiple formats simultaneously?

```bash
--support-format ".png,.jpg,.bmp"
```

## Tips

1. **Use relative paths**: Recommend using relative paths for more flexibility
2. **Create Shell scripts**: For common builds, create `.sh` scripts
3. **Check output**: Size information and suggestions will be displayed after build
4. **Header file location**: In the same directory as binary file, no need to specify separately

## Get Help

Run the following command to view complete help:

```bash
python assets_gen.py --help
```