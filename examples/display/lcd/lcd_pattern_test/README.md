| Supported Targets | ESP32-P4 |
| ----------------- | -------- |

| Supported LCD Controller | EK79007 |
| ------------------------ | ------- |

# LCD Pattern Test

This example provides a reusable pattern test suite to quickly verify that the panel is lit and the display data path is correct.  
The test logic is interface-agnostic as long as you have an `esp_lcd_panel_handle_t` and `esp_lcd_panel_draw_bitmap()` works.

## Scope

- Works with SPI/I80/QSPI/RGB/MIPI-DSI LCDs.

## Troubleshooting Guide (by test case)

Use the sequence below to isolate issues. Each case narrows the fault domain.

### Case 1: Solid Colors (black/white/red/green/blue/gray)

What to look for:
- **No image or very dim image**: backlight, reset, power rails, or init sequence.
- **All colors look identical**: bus stuck, pixel format mismatch, or IO miswiring.
- **Colors swapped** (red shows as blue, etc.): RGB/BGR order mismatch.
- **Gray looks tinted**: bit depth mismatch or wrong color order.

Likely root causes:
- Wrong pixel format (RGB565/666/888).
- RGB/BGR order mismatch.
- Panel init commands not matching the vendor variant.

### Case 2: Color Bars (8 vertical bars)

What to look for:
- **Missing colors or duplicated bars**: data bit order/width issues.
- **Bars shifted or uneven width**: timing or resolution mismatch.

Likely root causes:
- Incorrect data bus width or pin mapping.
- Incorrect panel resolution or porch/timing settings.

### Case 3: Grayscale Gradient (left to right)

What to look for:
- **Severe banding**: wrong bpp or limited color mode set by init commands.
- **Noise or flicker in gradient**: unstable clock or data integrity problems.

Likely root causes:
- bpp mismatch between panel config and init commands.
- Clock/pixel timing instability.

### Case 4: RGB Gradient Bars (vertical bars, brightness increases downward)

What to look for:
- **Only one bar changes**: channel missing or swapped.
- **Gradient direction incorrect**: coordinate mapping or panel timing mismatch.

Likely root causes:
- Channel wiring or RGB/BGR mismatch.
- Incorrect width/height or timing settings.

### Case 5: Checkerboard

What to look for:
- **Broken or skewed grid**: bit-order mistakes, data line shorts/opens.
- **Random noise on squares**: signal integrity or power issues.

Likely root causes:
- Data line issues, wrong IO matrix, or unstable supply.

## Pattern Sequence

- Solid: black / white / red / green / blue / gray  
- Color bars: 8 vertical bars  
- Grayscale gradient: left to right  
- RGB gradient: vertical bars, brightness increases downward  
- Checkerboard  


## How to Reuse for Other Panels

1. Replace the panel initialization in `main/lcd_init.c`.
2. Ensure you obtain a valid `esp_lcd_panel_handle_t`.
3. Call:
   - `lcd_pattern_test_run(panel, &config)`

## Build and Flash

```
idf.py set-target esp32p4
idf.py -p PORT build flash monitor
```
