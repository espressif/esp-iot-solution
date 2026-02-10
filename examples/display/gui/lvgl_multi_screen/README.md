| Supported Targets | ESP32-P4 | ESP32-S3 | ESP32-C3 |
| ----------------- | -------- | -------- | -------- |

| Supported LCD Controller | GC9A01 |
| ------------------------ | ------ |

# LVGL Multi-Screen Example

This example demonstrates how to drive **two SPI LCD panels simultaneously** using LVGL with the `esp_lvgl_adapter` component. It shows how to share a single SPI bus between multiple displays and render independent content on each screen.

## Overview

The example showcases:
- **Dual SPI display support**: Two LCD panels on the same SPI bus
- **Independent rendering**: Each display has its own LVGL display object
- **Different content per screen**: First screen runs benchmark, second shows custom UI
- **Scalable design**: Can be extended to more displays (limited by CS pins)

## Use Cases

This multi-screen approach is ideal for:
- **Dashboard systems**: Multiple data displays (gauges, charts, status)
- **Gaming devices**: Main screen + secondary info display
- **Industrial HMI**: Primary control + auxiliary information panels
- **Wearables**: Multiple watch faces or data screens
- **Smart home controllers**: Different rooms or device controls
- **Medical devices**: Patient data + settings/alerts on separate screens

## How It Works

**Key points:**
- **Shared signals**: MOSI, SCLK, DC, RST, Backlight
- **Individual CS pins**: Each display has unique chip select
- **Bus arbitration**: SPI driver handles concurrent access automatically
- **Independent control**: Each display can be updated separately

## Hardware Requirements

### Required Components

* An ESP32 or ESP32-S3 development board (other ESP32 series also supported)
* **Two GC9A01 LCD panels** (240x240 round displays)
  - Operating voltage: 3.3V
  - Interface: SPI (4-wire)
  - Common on affordable development kits
* Jumper wires for connections
* USB cable for power and programming

**Reference Hardware (Single Display with Various LCD Interfaces):**

For single display examples with various LCD interfaces, please refer to:

| Chip | LCD Interface | Development Board |
|------|---------------|-------------------|
| ESP32-P4 | MIPI DSI | [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html) |
| ESP32-S3 | RGB | [ESP32-S3-LCD-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/index.html) |
| ESP32-S3 | QSPI | [EchoEar](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/echoear/index.html) |
| ESP32-S3 | SPI | [ESP32-S3-BOX-3](https://github.com/espressif/esp-box/blob/master/docs/hardware_overview/esp32_s3_box_3/hardware_overview_for_box_3.md) |
| ESP32-C3 | SPI | [ESP32-C3-LCDkit](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c3/esp32-c3-lcdkit/index.html) |

## Hardware Connection

### Default GPIO Pin Mapping

| Target | MOSI | SCLK | DC | RST | CS_0 | CS_1 | BL |
|--------|------|------|----|-----|------|------|----|
| ESP32-S3 / ESP32-P4 | GPIO 38 | GPIO 45 | GPIO 47 | GPIO 21 | GPIO 41 | GPIO 48 | GPIO 39 |
| ESP32-C3 | GPIO 0 | GPIO 1 | GPIO 7 | GPIO 2 | GPIO 5 | GPIO 8 | GPIO 3 |

### Wiring Diagram

```
ESP32-S3 Dev Board          Display 0 (GC9A01)      Display 1 (GC9A01)
┌──────────────┐            ┌─────────────┐         ┌─────────────┐
│              │            │             │         │             │
│     3.3V ────┼────────────┤ VCC         │    ┌────┤ VCC         │
│              │            │             │    │    │             │
│     GND ─────┼────────────┤ GND         │────┼────┤ GND         │
│              │            │             │    │    │             │
│  GPIO45 (SCK)┼────────────┤ SCL         │────┼────┤ SCL         │
│              │            │             │    │    │             │
│ GPIO38 (MOSI)┼────────────┤ SDA         │────┼────┤ SDA         │
│              │            │             │    │    │             │
│  GPIO47 (DC) ┼────────────┤ DC          │────┼────┤ DC          │
│              │            │             │    │    │             │
│  GPIO21 (RST)┼────────────┤ RST         │────┼────┤ RST         │
│              │            │             │    │    │             │
│  GPIO39 (BL) ┼────────────┤ BL          │────┴────┤ BL          │
│              │            │             │         │             │
│  GPIO41 ─────┼────────────┤ CS          │         │             │
│              │            │             │         │             │
│  GPIO48 ─────┼────────────┼─────────────┼─────────┤ CS          │
│              │            │             │         │             │
└──────────────┘            └─────────────┘         └─────────────┘
```

**Notes:**
- All signals except CS are shared
- Each display must have a unique CS pin
- Both displays share the same backlight control
- Common ground is essential for reliable operation

### Changing GPIO Pins

Edit `hw_multi.h` to customize GPIO assignments:

```c
#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4
#define BSP_LCD_DATA0   (GPIO_NUM_38)
#define BSP_LCD_PCLK    (GPIO_NUM_45)
#define BSP_LCD_DC      (GPIO_NUM_47)
#define BSP_LCD_RST     (GPIO_NUM_21)
#define BSP_LCD_CS_0    (GPIO_NUM_41)
#define BSP_LCD_CS_1    (GPIO_NUM_48)
#define BSP_LCD_BL      (GPIO_NUM_39)
#elif CONFIG_IDF_TARGET_ESP32C3
#define BSP_LCD_DATA0   (GPIO_NUM_0)
#define BSP_LCD_PCLK    (GPIO_NUM_1)
#define BSP_LCD_DC      (GPIO_NUM_7)
#define BSP_LCD_RST     (GPIO_NUM_2)
#define BSP_LCD_CS_0    (GPIO_NUM_5)
#define BSP_LCD_CS_1    (GPIO_NUM_8)
#define BSP_LCD_BL      (GPIO_NUM_3)
#endif
```

## Software Architecture

### Display Initialization Flow

```
1. Initialize shared SPI bus (SPI2_HOST)
   ↓
2. Configure backlight GPIO
   ↓
3. For each display (i = 0, 1):
   ├── Create panel IO with unique CS pin
   ├── Install GC9A01 panel driver
   ├── Reset panel
   ├── Initialize panel
   ├── Set rotation
   └── Turn on display
   ↓
4. Initialize LVGL adapter
   ↓
5. Register both displays to LVGL
   ↓
6. Create different UI for each display
```

### Multi-Display Management

**Key data structure:**
```c
typedef struct {
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t panel_io;
} hw_multi_panel_t;

hw_multi_panel_t panels[2];
lv_display_t *displays[2];
```

**Display selection in LVGL:**
```c
// Set default display before creating UI
lv_display_set_default(displays[0]);
lv_demo_benchmark();  // Runs on display 0

lv_display_set_default(displays[1]);
lv_label_create(...);  // Creates on display 1
```

## Build and Flash

### Configure the Project

Run `idf.py menuconfig` and navigate to `Hardware Configuration`:

**Display Rotation:**
- Choose rotation for both displays (0°/90°/180°/270°)
- Both displays use the same rotation setting
- Useful for different physical mounting orientations

### Build and Flash

1. Set the target chip:
```bash
idf.py set-target esp32s3
# or esp32, esp32c3, etc.
```

2. Build and flash:
```bash
idf.py -p PORT build flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

### Expected Output

**Display 0 (Primary):**
- Runs LVGL benchmark demo
- Cycles through various rendering tests
- Shows animations, shapes, text, gradients

**Display 1 (Secondary):**
- Shows simple text: "Second display"
- Centered on the screen
- Can be customized to show any LVGL content

**Serial Console:**
```
I (xxx) hw_multi: SPI bus initialized
I (xxx) hw_multi: Display 0 initialized (240x240, rotation 0)
I (xxx) hw_multi: Display 1 initialized (240x240, rotation 0)
I (xxx) main: Display 0 registered
I (xxx) main: Display 1 registered
```

**If FPS monitoring enabled:**
```
I (xxx) main: Current FPS: 30
```

### Synchronized Updates

If you need both displays to update simultaneously:

```c
if (esp_lv_adapter_lock(-1) == ESP_OK) {
    // Update display 0
    lv_display_set_default(displays[0]);
    lv_label_set_text(label0, "Updated");
    
    // Update display 1
    lv_display_set_default(displays[1]);
    lv_label_set_text(label1, "Updated");
    
    esp_lv_adapter_unlock();
}
```

## Technical Details

### SPI Bus Configuration

```c
spi_bus_config_t buscfg = {
    .sclk_io_num = BSP_LCD_PCLK,
    .mosi_io_num = BSP_LCD_DATA0,
    .miso_io_num = GPIO_NUM_NC,        // Not used for LCD
    .max_transfer_sz = 240 * 80 * 2,   // 80 lines buffer
};
spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
```

### Display Registration

Each display needs:
1. Panel IO handle (with unique CS)
2. Panel handle (driver instance)
3. LVGL display object

```c
esp_lv_adapter_display_config_t display_config = 
    ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(
        panel, panel_io, 240, 240, rotation);

lv_display_t *disp = esp_lv_adapter_register_display(&display_config);
```

### Thread Safety

LVGL is **not thread-safe**. Always use the mutex:

```c
if (esp_lv_adapter_lock(-1) == ESP_OK) {
    // All LVGL API calls here
    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "Safe");
    
    esp_lv_adapter_unlock();
}
```

## Troubleshooting

**Only one display works:**
- Verify both CS pins are different
- Check CS pin connections
- Ensure both panels have power and ground
- Check serial logs for initialization errors

**Second display blank:**
- Check that `panel_count` is 2 in logs
- Verify display initialization succeeded
- Check backlight is on (`gpio_set_level(BSP_LCD_BL, 1)`)
- Try swapping CS pins to isolate hardware issue

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.
