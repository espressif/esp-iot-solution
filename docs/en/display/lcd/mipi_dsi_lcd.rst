MIPI DSI LCD Detailed Guide
============================

:link_to_translation:`zh_CN:[中文]`

.. contents:: Table of Contents
    :local:
    :depth: 2

Glossary
--------

Please refer to :ref:`LCD Terms Table <LCD_Terms_Table>`.

MIPI-DSI Related Terms
~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 25 60

   * - Abbreviation
     - Full Name
     - Description
   * - MIPI-DSI
     - MIPI Display Serial Interface
     - High-speed serial interface standard developed by MIPI Alliance, specifically designed for connecting processors and display devices
   * - DCS
     - Display Command Set
     - Standardized display command set defined by MIPI-DSI for controlling display modules
   * - DBI
     - Display Bus Interface
     - Encapsulates DSI's Command Mode into esp-lcd's control IO layer
   * - DPI
     - Display Pixel Interface
     - Encapsulates DSI's Video Mode into esp-lcd's data panel layer
   * - HS
     - High-Speed Mode
     - High-speed mode, mainly used for transmitting display frame data, typically 80 Mbps ~ 1.5 Gbps/Lane
   * - LP
     - Low-Power Mode
     - Low-power mode, typically used for sending DCS commands or controlling link states, maximum rate approximately 10 MHz
   * - HS-TX/HS-RX
     - High-Speed Transmitter/Receiver
     - High-speed transmitter/receiver
   * - LP-TX/LP-RX
     - Low-Power Transmitter/Receiver
     - Low-power transmitter/receiver
   * - LP-CD
     - Low-Power Contention Detector
     - Low-power contention detector
   * - BTA
     - Bus Turnaround
     - Bus turnaround for bidirectional communication
   * - ULPS
     - Ultra Low Power State
     - Ultra low power state for maximum energy savings
   * - LPDT
     - Low Power Data Transmission
     - Low power data transmission mode
   * - ESC
     - Escape Mode
     - Escape mode, low-power operation mode of MIPI-DSI
   * - SoT
     - Start of Transmission
     - Start of transmission signal
   * - EoT
     - End of Transmission
     - End of transmission signal
   * - DI
     - Data Identifier
     - Data identifier for identifying packet types
   * - ECC
     - Error Correction Code
     - Error detection code for data integrity verification
   * - DT
     - Data Type
     - Data type for identifying pixel data format
   * - HSYNC
     - Horizontal Synchronization
     - Horizontal synchronization signal
   * - VSYNC
     - Vertical Synchronization
     - Vertical synchronization signal
   * - DMA
     - Direct Memory Access
     - Direct memory access for efficient data transfer
   * - PSRAM
     - Pseudo Static RAM
     - Pseudo static random access memory
   * - RGB565
     - Red Green Blue 565
     - 16-bit color format, 5 bits red, 6 bits green, 5 bits blue
   * - RGB666
     - Red Green Blue 666
     - 18-bit color format, 6 bits each for red, green, and blue
   * - RGB888
     - Red Green Blue 888
     - 24-bit color format, 8 bits each for red, green, and blue

MIPI-DSI Interface and Protocol Introduction
--------------------------------------------

MIPI-DSI Interface Overview
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

MIPI-DSI (MIPI Display Serial Interface) is a high-speed serial interface standard developed by the MIPI Alliance, specifically designed for connecting processors and display devices. It features:

* High-speed transmission: Each data channel can support transmission rates up to several Gbps
* Low power consumption: Uses low-voltage differential signaling and enters low-power mode during idle or low-speed transmission
* Pin reduction: Significantly reduces the number of signal lines compared to parallel RGB interfaces
* Scalability: Supports 1 to 4 data channels
* High compatibility: Supports various resolutions and display devices

Currently, ESP32-P4 chips feature MIPI-DSI interfaces, supporting 2-lane × 1.5Gbps and using Video mode for video stream output.

MIPI-DSI Screen Refresh Principle
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

MIPI-DSI is a commonly used display interface standard for connecting processors and displays. It supports two main data transmission modes:

1. Command Mode
2. Video Mode

ESP32-P4 currently only supports Video mode for video stream output.

In Video Mode, the host transmits data in continuous video frames, strictly synchronized with the display's refresh cycle (such as VSync). The display does not require frame buffer memory, simplifying hardware design. This mode is widely used for high-resolution, high-refresh-rate display scenarios.

.. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_video.png
    :align: center
    :alt: MIPI-DSI Screen Refresh Process

    MIPI-DSI Screen Refresh Process

The figure above shows the MIPI-DSI screen refresh process, where:

* Command data: MIPI-DCS commands are transmitted to the LCD controller through DBI for initializing registers and controlling display functions.
* Frame color data: In MIPI-DSI Video mode, pixel data is continuously output from the host-side frame buffer (internal/external RAM) to the LCD panel for display through DMA, while the LCD driver IC does not require GRAM.
* Touch data: Touch signals are detected by the touchscreen and transmitted back to the host system through I²C or SPI interfaces for processing.

MIPI-DSI Protocol
~~~~~~~~~~~~~~~~~~

MIPI-DSI can be roughly divided into Physical Layer, Protocol Layer, and Display Command Layer (DCS Layer), each with specific functions that work together to complete command and pixel data transmission from the host to the display.

The collaborative workflow of each layer can be summarized as shown in the following figure:

.. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_layer.png
    :align: center
    :alt: MIPI-DSI Protocol Layers

    MIPI-DSI Protocol Layers

Physical Layer
^^^^^^^^^^^^^^

* Function: Responsible for actual data transmission, serving as the bottom layer of the protocol stack. Uses MIPI D-PHY to provide high-speed differential signal transmission, supporting multi-channel data transmission.

* Transmission modes:
  
  * High-Speed Mode: Mainly used for transmitting display frame data, can also transmit commands, typically 80 Mbps ~ 1.5 Gbps/Lane.
  * Low-Power Mode: Typically used for sending DCS commands (in command mode) or controlling link states, maximum rate approximately 10 MHz (specific depends on chip support).

* Configuration:
  
  * Clock Lane: Provides synchronization signals.
  * Data Lane: Transmits actual display data.

* Detailed explanation of Lane Module, i.e., D-PHY:
  
  * Types
    
    .. list-table::
        :widths: 20 35 35
        :header-rows: 1

        * - Lane Type/Transmission Role
          - Master
          - Slave
        * - Unidirectional Clock Lane
          - HS-TX, LP-TX
          - HS-RX, LP-RX
        * - Unidirectional Data Lane
          - HS-TX, LP-TX
          - HS-RX, LP-RX
        * - Bidirectional Data Lane
          - HS-TX, LP-TX, HS-RX, LP-RX, LP-CD
          - HS-TX, LP-TX, HS-RX, LP-RX, LP-CD
    
.. note::
   Module function descriptions:
   
   - **LP-TX/LP-RX**: Low-power transmitter/receiver
   - **HS-TX/HS-RX**: High-speed transmitter/receiver  
   - **LP-CD**: Low-power contention detector
   
   Clock lanes are typically unidirectional, provided by Master and received by Slave. Bidirectional lanes and LP-CD are mainly used for low-power communication in command mode.
  
* States

    * LP States (LP-00, LP-01, LP-10, LP-11): Use single-ended signals, mainly for low-speed command transmission, control signals, initialization, or standby states. Different LP states are used for encoding and link control.
    * HS States (HS-0, HS-1): Use differential signals, mainly for high-speed transmission of video data or frame content. HS-0 and HS-1 correspond to logical levels on differential pairs, only valid on HS-TX/HS-RX.
  
* Typical voltages

    * LP: Typical voltage approximately 0 – 1.2 V
    * HP: Typical differential voltage approximately 100 – 300 mV
  
* Operation modes

    * Escape Mode: Only available in Low-Power Mode, MIPI-DSI's low-power operation mode, used for special data transmission or triggering specific functions. Supports the following functions:
        
        .. list-table::
           :header-rows: 1
           :widths: 25 40 15

           * - Function Name
             - Description
             - Command
           * - LPDT (Low Power Data Transmission)
             - Transmit data in low-power mode
             - 0x87
           * - ULPS (Ultra Low Power State)
             - Enter ultra low power state for maximum energy savings
             - 0x84
           * - Trigger Command
             - Trigger specific events, such as state switching or mode adjustment
             - 0x85
        
        * Entry process (signal state machine transition): LP-11 → LP-10 → LP-00 → LP-01 → LP-00
        * Exit process: LP-10 → LP-11

    * High-Speed (Burst) Mode: Always in High-Speed Mode, used for high-speed serial data transmission, suitable for high bandwidth requirements.
        
        * Entry process: LP-11 → LP-01 → LP-00 → SoT (Start of Transmission) → HSD (80Mbps ~ 1.5Gbps)
        * Exit process: EoT (End of Transmission) → LP-11

    * Control Mode: Default in Low-Power Mode, used for command transmission in stop state and bidirectional communication (BTA, Bus Turnaround).
        
        * BTA transmission process: LP-11 → LP-10 → LP-00 → LP-10 → LP-00 (bus turnaround state transition)
        * BTA receive process: LP-00 → LP-10 → LP-11

**Important Signal Descriptions**

During MIPI-DSI communication, two critical Stop signals ensure proper bus state management:

.. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_stop.png
    :align: center
    :alt: MIPI-DSI Stop Signal Timing Diagram

    MIPI-DSI Stop Signal Timing Diagram

#. **BTA-Stop Signal**
   
   Sent by the slave after completing Bus Turnaround response, indicating:
   
   - Bus turnaround operation completed successfully
   - Slave has released bus control
   - Host can regain bus control

#. **ESC-0X84-Stop Signal**
   
   Used to indicate Lane stop signal transmission, specific functions include:
   
   - Enter system into ultra low power state (ULPS)
   - Corresponds to 0x84 command in Escape Mode
   - Achieve maximum energy management

.. tip::
   These Stop signals are key mechanisms in the MIPI-DSI protocol for ensuring communication reliability and power management.

Protocol Layer
^^^^^^^^^^^^^^^

* Function: Implements data packaging and parsing, defines transmission formats (such as DCS and video data packets), and provides error detection mechanisms to ensure data correctness and reliability.

* Data frame structure
  
  * Short Packet:
    
    * Length: Fixed length of 4 bytes
    * Composition: Data Identifier (DI), 1 byte; Frame data, 2 bytes; Error detection (ECC), 1 byte.
    
    .. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_short_packet.png
        :align: center
        :alt: MIPI-DSI Short Packet Composition

        MIPI-DSI Short Packet Composition

    * Example:
    
    .. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_short_packet_example.png
        :align: center
        :alt: MIPI-DSI Short Packet Timing

        MIPI-DSI Short Packet Timing

    The key fields in order are:

    - ESC: Enter Escape mode
    - 0x87: LPDT (Low Power Data Transmission mode)
    - 0x37: Data identifier, corresponding to DCS command "Set Maximum Return Packet Size"; Function: Set the maximum size of return data packets when the host receives data from the slave
    - 0x03, 0x00: Frame data, indicating setting the maximum return packet size to 3 bytes
    - 0x01: ECC (Error Correction Code)
  
  * Long Packet:
    
    * Length: Variable length, 6 to 65541 bytes.
    * Composition: Frame header (Data Identifier (DI) 1 byte; Data Count (WC) 2 bytes; Error detection (ECC) 1 byte); Data payload (0 to 65535 bytes); Frame tail (Checksum 2 bytes).
    
    .. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_long_paket.png
        :align: center
        :alt: MIPI-DSI Long Packet Composition

        MIPI-DSI Long Packet Composition

    * Example:
    
    .. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_long_packet_example.png
        :align: center
        :alt: MIPI-DSI Long Packet Timing

        MIPI-DSI Long Packet Timing

    The key fields in order are:

    - ESC: Enter Escape mode
    - 0x87: LPDT (Low Power Data Transmission mode)
    - 0x39: Data identifier, corresponding to DCS command "DCS Write Long"
    - 0x03, 0x00: Data count, indicating sending 3 bytes of data commands
    - 0x09: ECC (Error Correction Code)
    - 0xB6, 0xB2, 0xB2: Actual data commands being sent
    - 0xEF, 0xFA: Checksum field

* Common Data Identifiers (DI)

.. list-table::
   :header-rows: 1
   :widths: 15 30 55

   * - Data ID
     - Name
     - Description
   * - 0x05
     - DCS Short Write, 0 Param
     - Send a DCS command with no parameters (e.g., Sleep Out `0x11`)
   * - 0x15
     - DCS Short Write, 1 Param
     - Send a DCS command with 1 parameter
   * - 0x39
     - DCS Long Write
     - Send a DCS command with multiple bytes of data (commonly used for memory writes)
   * - 0x06
     - DCS Short Read, 0 Param
     - Read a DCS status or register with no parameters
   * - 0x16
     - DCS Short Read, 1 Param
     - Read a DCS status or register with 1 parameter
   * - 0x37
     - DCS Read Response
     - DCS read data returned by the display
   * - 0x03
     - Generic Short Write, 0 Param
     - Send generic command with no parameters
   * - 0x13
     - Generic Short Write, 1 Param
     - Send generic command with 1 parameter
   * - 0x23
     - Generic Short Write, 2 Param
     - Send generic command with 2 parameters
   * - 0x29
     - Generic Long Write
     - Send generic command with multiple bytes of data
   * - 0x04
     - Generic Read, 0 Param
     - Generic read command with no parameters
   * - 0x14
     - Generic Read, 1 Param
     - Generic read command with 1 parameter
   * - 0x24
     - Generic Read, 2 Param
     - Generic read command with 2 parameters

Display Command Layer (DCS Layer)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* Function: DCS (Display Command Set) is a standardized display command set defined by MIPI-DSI. The host interacts with the display through these commands to implement basic display module control (such as power on/off, sleep, backlight, display on/off), display parameter configuration (such as pixel format, display mode, address setting), and frame buffer access (such as writing image data, reading status information). DCS commands are transmitted on the DSI bus in the form of short write, long write, and read commands, serving as the main control interface between the application layer and display hardware.

* Common commands:

.. list-table::
   :header-rows: 1
   :widths: 10 25 40

   * - Command Code
     - Name
     - Function Description
   * - 0x01
     - Software Reset
     - Software reset
   * - 0x11
     - Sleep Out
     - Exit sleep mode
   * - 0x28
     - Display Off
     - Turn off display
   * - 0x29
     - Display On
     - Turn on display
   * - 0x2A
     - Column Address Set
     - Set column address range
   * - 0x2B
     - Page Address Set
     - Set page address range
   * - 0x2C
     - Memory Write
     - Write frame buffer data
   * - 0x2E
     - Memory Read
     - Read frame buffer data
   * - 0x36
     - Address Mode
     - Set memory scan direction and flip mode
   * - 0x3A
     - Pixel Format Set
     - Set pixel format (e.g., RGB565/888)

Color Format
------------

.. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_color_format.png
    :align: center
    :alt: MIPI-DSI Color Format

    MIPI-DSI Color Format

Most MIPI-DSI LCDs support multiple input color formats, such as **RGB565, RGB666, RGB888**.  
The host can configure the display color depth through **DCS command 0x3A (Pixel Format Set)**, for example:

- When setting to **RGB565 (16-bit)**, send ``Command: 0x3A``, ``Parameter: 0x55`` (specific parameter values need to refer to the LCD controller datasheet).

Pixel data is typically transmitted through **Long Packets**, which contain pixel data and corresponding Data Types.  
For example, RGB565 format pixel streams generally use **DT = 0x0E (Packed Pixel Stream, 16-bit)**.

Description
~~~~~~~~~~~

- **16-bit and 18-bit** formats are commonly used to reduce bandwidth and storage usage  
- **24-bit** format is suitable for high-quality display scenarios

Common Color Formats and Data Types
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 15 20 25

   * - Color Format
     - Bit Depth
     - DCS Parameter (Example)
     - Data Type (DT)
   * - RGB565
     - 16-bit
     - 0x55
     - 0x0E
   * - RGB666
     - 18-bit
     - 0x66
     - 0x1E / 0x2E
   * - RGB888
     - 24-bit
     - 0x77
     - 0x3E

MIPI-DSI Driver Flow
--------------------

The MIPI-DSI LCD driver flow can be roughly divided into four parts: initializing the DSI bus, configuring the DBI interface, porting the driver component, and initializing the LCD device.

DSI, DBI, and DPI
~~~~~~~~~~~~~~~~~~

* DSI: Responsible for low-level initialization of PHY, Host, and Bridge, serving as the common "bus" for DBI and DPI.  
* DBI: Encapsulates DSI's Command Mode into esp-lcd's control IO layer
* DPI: Encapsulates DSI's Video Mode into esp-lcd's data panel layer, responsible for continuous refresh from frame buffer to screen

Initialize DSI Bus
~~~~~~~~~~~~~~~~~~

.. code-block:: c

    esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,                                   // DSI controller number
        .num_data_lanes = 2,                           // Number of data lanes (1-2)
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,   // PHY clock source
        .lane_bit_rate_mbps = 1300,                    // Bit rate per lane (Mbps)
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &dsi_bus));

The number of data lanes (num_data_lanes) depends on the number of lanes used and supported by the LCD IC. Some LCD ICs can configure the number of lanes used through registers, while some LCD ICs have a fixed number of lanes. It's worth noting that ESP32-P4 supports a maximum of 2 lanes, supporting both 1-lane and 2-lane LCD channel configurations.

Calculation method for bit rate per lane (lane_bit_rate_mbps):

+ Example calculation:
  
  * Resolution: 800×1280 (hspw=4, hbp=20, hfp=20; vspw=4, vbp=20, vfp=20)
  * Refresh rate: 60Hz
  * Color depth: 24bpp
  * Number of data lanes: 2

* Calculate raw data rate
  
  * pixel_clock = horizontal_total_pixels × vertical_total_pixels × refresh_rate
  * raw_data_rate = pixel_clock × bits_per_pixel
    
    Example calculation:
    
    * pixel_clock = (800+20+20+4) × (1280+20+20+4) × 60 = 62.4MHz
    * raw_data_rate = 62.4M × 24 = 1497.6Mbps

* Consider DSI protocol overhead
  
  * 8b/10b encoding: actual data rate needs to be multiplied by 1.25
  * Protocol header and ECC: additional overhead of approximately 10-15%
  * protocol_data_rate = raw_data_rate × 1.25 × 1.15
    
    Example calculation:
    
    * protocol_data_rate = 1497.6 × 1.25 × 1.15 = 2153.55Mbps

* Distribute to each data lane
  
  * lane_bit_rate = protocol_data_rate ÷ num_data_lanes
    
    Example calculation:
    
    * lane_bit_rate = 2153.55 ÷ 2 = 1076.78Mbps

* Reserve bandwidth margin (recommended 20%)
  
  * final_bit_rate = lane_bit_rate × 1.2
    
    Example calculation:
    
    * final_bit_rate = 1076.78 × 1.2 ≈ 1292Mbps

Therefore, it is recommended to set lane_bit_rate_mbps to final_bit_rate, which is around `1300` in the example calculation scenario. Note that on the ESP32P4 platform, lane_bit_rate_mbps cannot exceed `1500`, and the recommended minimum should not be below `480`.

Configure DBI Interface
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_dbi_io_config_t io_config = {
        .virtual_channel = 0,           // Virtual channel number (0-3), most LCDs only support channel `0`, usually set to `0`
        .lcd_cmd_bits = 8,              // Command bit width
        .lcd_param_bits = 8,            // Parameter bit width
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsi_bus, &io_config, &io_handle));

The command and parameter bit widths must match the panel specifications, and virtual channels are used to distinguish multiple devices.

Port Driver Component
~~~~~~~~~~~~~~~~~~~~~~

The basic principles for porting MIPI-DSI LCD driver components include the following three points:

  1. Send commands and parameters in specified formats based on interface device handles of type ``esp_lcd_dbi_io_config_t``.
  2. Implement and create an LCD device, then implement the functions in the `esp_lcd_panel_t <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/interface/esp_lcd_panel_interface.h>`_ structure by registering callback functions.
  3. Implement a function to provide LCD device handles of type ``esp_lcd_panel_handle_t``, enabling applications to use `LCD General APIs <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/include/esp_lcd_panel_ops.h>`_ to operate the LCD device

Below is the implementation description of each function in ``esp_lcd_panel_handle_t`` and its correspondence with `LCD General APIs <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/include/esp_lcd_panel_ops.h>`_:

For most MIPI-DSI LCDs, their driver IC commands and parameters are compatible with the implementation descriptions above, so porting can be completed through the following steps:

1. Select a MIPI-DSI LCD driver component with a similar model from the `LCD Driver Components <https://github.com/espressif/esp-iot-solution/blob/master/docs/zh_CN/display/lcd/lcd_development_guide.rst#%E9%A9%B1%E5%8A%A8%E5%8F%8A%E7%A4%BA%E4%BE%8B>`_.
2. Confirm whether the commands and parameters used in each function of the selected component are consistent with the target LCD driver IC by consulting the target LCD driver IC datasheet. If not, modify the relevant code.
3. Even for LCD driver ICs of the same model, screens from different manufacturers usually require their own initialization command configurations. Therefore, it is necessary to modify the commands and parameters sent in the initialization function ``init()``. These initialization commands are usually stored in a static array in a specific format. Additionally, be careful not to include special commands in the initialization commands, such as ``LCD_CMD_COLMOD(3Ah)`` and ``LCD_CMD_MADCTL(36h)``, which are managed and used by the driver component.
4. Use the editor's character search and replace function to replace the LCD driver IC name in the component with the target name, such as replacing ``ek79007`` with ``ili9881``

Initialize LCD Device
~~~~~~~~~~~~~~~~~~~~~

The following code example uses `EK79007 <https://components.espressif.com/components/espressif/esp_lcd_ek79007>`_ as an example:

.. code-block:: c

    #include "esp_lcd_panel_vendor.h"   // Required header file
    #include "esp_lcd_panel_ops.h"
    #include "esp_lcd_ek79007.h"        // Target driver component header file

    // static const ek79007_lcd_init_cmd_t lcd_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    // {0xE0, (uint8_t []){0x00}, 1, 0},
    // {0xE1, (uint8_t []){0x93}, 1, 0},
    // {0xE2, (uint8_t []){0x65}, 1, 0},
    // {0xE3, (uint8_t []){0xF8}, 1, 0},
    //     ...
    // };

        ESP_LOGI(TAG, "MIPI DSI PHY Powered on");
        esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
        esp_ldo_channel_config_t ldo_mipi_phy_config = {
            .chan_id = 3,
            .voltage_mv = 2500,
        };
        ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy));

        ESP_LOGI(TAG, "Initialize MIPI DSI bus");
        esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
        esp_lcd_dsi_bus_config_t bus_config = EK79007_PANEL_BUS_DSI_2CH_CONFIG();
        ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

        ESP_LOGI(TAG, "Install panel IO");
        esp_lcd_panel_io_handle_t mipi_dbi_io = NULL;
        esp_lcd_dpi_panel_config_t dpi_config = { 
            .virtual_channel = 0,
            .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
            .dpi_clock_freq_mhz = 60,                 // Pixel clock frequency
            .in_color_format = LCD_COLOR_FMT_RGB888,  // Color format
            .video_timing = {
                .h_size = 800,                        // Horizontal resolution
                .v_size = 1280,                       // Vertical resolution
                .hsync_pulse_width = 4,               // HSYNC pulse width
                .hsync_back_porch = 20,               // Horizontal back porch
                .hsync_front_porch = 20,              // Horizontal front porch
                .vsync_pulse_width = 4,               // VSYNC pulse width
                .vsync_back_porch = 20,               // Vertical back porch
                .vsync_front_porch = 20,              // Vertical front porch
            },
            .flags.use_dma2d = true,                  // Use 2D DMA acceleration
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &mipi_dbi_io));

        ESP_LOGI(TAG, "Install EK79007S panel driver");
        esp_lcd_panel_handle_t panel_handle = NULL;
        ek79007_vendor_config_t vendor_config = {
            .flags = {
                .use_mipi_interface = 1,
            },
            .mipi_config = {
                .dsi_bus = mipi_dsi_bus,
                .dpi_config = &dpi_config,
            },
        };
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = EXAMPLE_LCD_IO_RST,           // Set to -1 if not use
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,     // Implemented by LCD command `36h`
            .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,    // Implemented by LCD command `3Ah` (16/18/24)
            .vendor_config = &vendor_config,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_ek79007(mipi_dbi_io, &panel_config, &panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

**Timing Parameter Description:**

The timing parameter definitions for MIPI-DSI are consistent with the `SYNC Mode` of the RGB interface in terms of timing diagrams. Both use HSYNC and VSYNC synchronization signals and require configuration of blanking porch parameters. For detailed explanations of timing parameters, the correspondence between parameters and timing diagram symbols, and configuration methods, please refer to the "Timing Parameter Correspondence" section in :doc:`RGB LCD Introduction <rgb_lcd>`.

Note that ESP32P4 requires a stable 2.5V power supply for the MIPI DSI PHY. The clock frequency calculation refers to the pixel_clock calculation method mentioned above. Color format, resolution, pulse width, and front/back porch parameters must strictly follow the panel datasheet requirements.

Then create an LCD device through the ported driver component and obtain a handle of type ``esp_lcd_panel_handle_t``, then use `LCD General APIs <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/include/esp_lcd_panel_ops.h>`_ to initialize the LCD device.

For more detailed explanations of ``MIPI-DSI`` interface configuration parameters, please refer to the `ESP-IDF Programming Guide <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/api-reference/peripherals/lcd/index.html>`_. Below are some explanations about using the function ``esp_lcd_panel_draw_bitmap()`` to refresh LCD images:

  - This function refreshes image data in the frame buffer through memory copy, meaning that after the function call completes, the image data in the frame buffer has also been updated. The ``MIPI-DSI`` interface itself uses DMA to fetch image data from the frame buffer to refresh the LCD, and these two processes are asynchronous.
  - This function will check whether the incoming parameter ``color_data`` value is the internal frame buffer address of the ``MIPI-DSI`` interface. If so, it will not perform the above memory copy operation, but directly set the DMA transfer address of the ``MIPI-DSI`` interface to that buffer address, thus achieving switching functionality in cases with multiple frame buffers.

In addition to `LCD General APIs <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/include/esp_lcd_panel_ops.h>`_, the `MIPI-DSI Interface Driver <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/src/esp_lcd_panel_mipi_dsi.c>`_ also provides some special function functions. Below are usage instructions for some commonly used functions:

  * esp_lcd_dpi_panel_get_frame_buffer(): Get frame buffer addresses, available quantity determined by configuration parameter ``num_fbs``, used for multi-buffer anti-tearing.
  * esp_lcd_dpi_panel_set_pattern(): Set predefined patterns to the screen for testing or debugging purposes.
  * esp_lcd_dpi_panel_set_color_conversion(): Set color conversion configuration for DPI panel.
  * esp_lcd_dpi_panel_register_event_callbacks(): Register callback functions for various events, example code and explanation as follows:

.. code-block:: c

    static bool example_on_color_trans_event(esp_lcd_panel_handle_t panel, const esp_lcd_dpi_panel_event_callbacks_t *edata, void *user_ctx)
    {
        /* Operations can be performed here */

        return false;
    }

    static bool example_on_refresh_event(esp_lcd_panel_handle_t panel, const esp_lcd_dpi_panel_event_callbacks_t *edata, void *user_ctx)
    {
        /* Operations can be performed here */

        return false;
    }

    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = example_on_color_trans_event,   // Triggered when user's color buffer is copied to internal frame buffer
        .on_refresh_done = example_on_refresh_event,  		   // Triggered when internal frame buffer content completes refresh and displays on screen
    };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(panel_handle, &cbs, &example_user_ctx));

Common Issues
-------------

Issue: No image display on screen with no other error logs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Check if SPIRAM SPEED is configured to 200M. Driving MIPI-DSI interface screens requires higher PSRAM bandwidth.

Issue: Screen initialization commands cannot be sent normally
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The chip's DSI data pins (DSI_DATAN0 and DSI_DATAP0) and clock pins (DSI_CLKN and DSI_CLKP) are not correctly connected to the screen's data pins and clock pins.

Issue: Screen image distortion and misalignment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This situation is generally caused by mismatch between lane_bit_rate_mbps and pixel_clock, or mismatch between lane_bit_rate_mbps and screen required rate. You can adjust the lane_bit_rate_mbps value and test, specifically refer to the pixel_clock calculation method mentioned above.

Issue: Screen flashes blue screen with log: "can't fetch data from external memory fast enough, underrun happens"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This issue occurs due to insufficient PSRAM bandwidth. You can consider reducing lane_bit_rate_mbps and pixel_clock values. In RGB888 color format scenarios, you can consider changing to RGB565 format. Also consider configuring the following options to improve PSRAM bandwidth:

    - ``CONFIG_SPIRAM_XIP_FROM_PSRAM=y``
    - ``CONFIG_CACHE_L2_CACHE_256KB=y``
    - ``CONFIG_CACHE_L2_CACHE_LINE_128B=y``
    - ``COMPILER_OPTIMIZATION_PERF=y``

Issue: How to resolve screen tearing
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Refer to `LCD Screen Tearing Detailed Guide <https://github.com/espressif/esp-iot-solution/blob/master/docs/zh_CN/display/lcd/lcd_screen_tearing.rst>`, related examples can be found in `mipi_dsi_avoid_tearing <https://github.com/espressif/esp-iot-solution/tree/master/examples/display/lcd/mipi_dsi_avoid_tearing>`_.

Issue: Frame rate optimization configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Consider configuring the following options to optimize frame rate:

    - ``CONFIG_SPIRAM_XIP_FROM_PSRAM=y``
    - ``CONFIG_CACHE_L2_CACHE_256KB=y``
    - ``CONFIG_CACHE_L2_CACHE_LINE_128B=y``
    - ``COMPILER_OPTIMIZATION_PERF=y``
    - LVGL related configurations:
        - ``CONFIG_LV_MEMCPY_MEMSET_STD=y``
        - ``CONFIG_LV_MEM_CUSTOM=y``

Issue: How to drive default 4-lane screens
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Most LCD ICs support adjusting the number of lanes through registers, but the register addresses and operations differ between ICs. You need to confirm whether the lane count can be adjusted based on the specific IC's datasheet or directly consult the screen manufacturer. For example, in EK79007, you can configure the 0xB2 register to choose between 2-lane/4-lane.

    .. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_lane_num.png
        :align: center
        :alt: Lane Number Configuration

        Lane Number Configuration

Issue: How to confirm the lane bit rate range supported by the screen
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In high-speed serial communication, UI usually refers to the time unit of signal bits (bit) on the physical channel, which is one bit time (bit period). For example, if the frame rate or link rate is 1 Gbps, then one UI ≈ 1 nanosecond (ns). If the rate is 500 Mbps, then UI ≈ 2 ns, and so on. Therefore, you can calculate the lane bit rate range based on the AC ELECTRICAL CHARACTERISTIC - High speed transmission in the LCD IC datasheet. As shown in the figure, the lane bit rate (transmission rate of a single Data Lane) ranges from 20Mbps to 500Mbps.

    .. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_speed.png
        :align: center
        :alt: Speed Range

        Speed Range

Related Documentation and Examples
----------------------------------

- `MIPI DSI Specification <https://www.mipi.org/specifications/dsi>`_
- `ESP-IDF MIPI DSI LCD Programming Guide <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32p4/api-reference/peripherals/lcd/dsi_lcd.html>`_
- `ESP LCD Driver Library <https://github.com/espressif/esp-idf/tree/master/components/esp_lcd>`_
- `ESP LCD Example Code <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/lcd>`_
- `ESP LCD FAQ <https://docs.espressif.com/projects/esp-faq/zh_CN/latest/software-framework/peripherals/lcd.html>`_
