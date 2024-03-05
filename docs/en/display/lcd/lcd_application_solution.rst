LCD Application Solution
========================================

:link_to_translation:`zh_CN:[中文]`

Introduction to LCD Solutions
----------------------------------------

Espressif's HMI Smart Screen (LCD) solution features outstanding performance and scalability, and can be paired with different ESP main control chips. This solution performs well in various application scenarios such as smart home control, household appliance screens, medical equipment, industrial control, and children's education. Advantages include high-performance graphic visualization, low memory usage, and a comprehensive screen adaptation solution that supports high-performance JPEG decoding and frame rate optimization. Below is a detailed introduction to the LCD solution

- **Outstanding Graphic Visualization:** Leveraging the high-performance graphic processing capabilities of ESP chips and deep collaboration with the LVGL official, it excels in LVGL compatibility. This solution offers exquisite visual effects, low memory usage, and can be easily integrated into product designs.

- **Simple UI Design:** The UI editor Squareline Studio supports quick and easy design and development of UI for embedded devices. It is easy to port and allows UI implementation without code, minimizing development time.

- **Rich Software and Hardware References:** Provides comprehensive LCD software and hardware development materials, including detailed guides and examples. In addition, HMI development boards designed for various HMI application scenarios can help developers get started quickly.

- **Comprehensive Screen Adaptation:** Supports various operation methods, including touch, knobs, etc. It also supports various peripheral interfaces such as RGB, SPI, etc. Multiple LCD driver ICs and Touch driver ICs have been adapted and organized into components to meet the needs of different user groups.

Furthermore, Espressif also supports the following features to further ensure a smoother interactive experience in LCD application scenarios:

- **Frame Rate Optimization and Tear Prevention Technology:** Ensures smooth and consistent image display through carefully optimized frame rate control and tear prevention technology.

- **High-Performance JPEG Decoding:** Supports efficient image processing to ensure a smooth multimedia experience.

- **Voice Wake-up and Recognition:** Integration of advanced voice recognition technology provides users with a more convenient way of interaction.

The following is an overview of a flowchart scheme:


.. image:: https://dl.espressif.com/AE/esp-iot-solution/lcd_program_architecture_en.png
  :alt: Scheme Architecture Diagram
  :align: center

Common Applications of LCDs:
-----------------------------

Espressif's LCD solutions are widely used in various fields, including but not limited to:

- **Rotary Knob Screen Solution:** Targeted at smart home appliance products, it is the preferred upgrade for traditional segment code screens and black-and-white screens. It supports Wi-Fi, Bluetooth, and expansion interfaces for functions such as serial communication. A typical application scenario is the rotary knob screen in small home appliance applications.

.. image:: https://dl.espressif.com/AE/esp-iot-solution/knob_screen.png
  :alt: Rotary Knob Screen Example
  :align: center

- **Central Control Screen Solution:** Integrates Wi-Fi, BLE, offline voice, RGB LCD display, supports offline command words, and continuous voice recognition. Suitable for the upgrade and iteration of traditional 86 panels, it constructs a smart home control center that integrates device control, switch panels, temperature control panels, smart remote controls, etc.

.. image:: https://dl.espressif.com/AE/esp-iot-solution/central_control.png
  :alt: Central Control Screen Example
  :align: center

**Visual Voice Solution:** Utilizes native USB to connect to a universal USB camera, achieving camera data stream reading, JPEG decoding, and real-time display on an RGB interface screen on a single SoC, without the need for additional USB chips. Suitable for scenarios such as cat's eyes, smart doorbell locks, electronic endoscopes, etc.

.. image:: https://dl.espressif.com/AE/esp-iot-solution/visual_voice.png
  :alt: Visual Speech Solution
  :align: center

**High-Performance Multimedia Solution:** This solution uses the ESP32-P4 chip, supporting MIPI-CSI and MIPI-DSI interfaces, suitable for various scenarios requiring high-resolution cameras and displays. The chip also integrates hardware accelerators for various media encoding and compression protocols, hardware pixel processing accelerator (PPA), and 2D-DMA, making it suitable for a wide range of multimedia applications.

.. image:: https://dl.espressif.com/AE/esp-iot-solution/high-performance_multimedia.png
  :alt: High-Performance Multimedia Example
  :align: center

In summary:

.. list-table:: Overview of LCD Solutions
   :widths: 10 20 15 20 40 40
   :header-rows: 1

   * - Solution Category
     - Solution Name
     - Main Control
     - Screen
     - Features
     - Application Scenarios
   * - Rotary Screen
     - `2.1-inch Rotary Screen Solution (Qiming) <https://www.bilibili.com/video/BV1TL411w7Vi>`__
     - ESP32-S3
     - 2.1-inch, 480 x 480 RGB interface screen
     - Supports Wi-Fi, Bluetooth, and expandable interfaces for serial communication, buttons, USB cameras, and other functions
     - Ideal for smart home appliances, preferred upgrade for traditional segment displays and black-and-white screens
   * - Rotary Screen
     - `1.28-inch Rotary Screen Solution <https://www.bilibili.com/video/BV1nG4y1d7Ja>`__
     - ESP32-C3
     - 1.28-inch, 240 x 240 SPI interface screen
     - Rotary encoder with push-button switch, compact design
     - Application scenarios include rotary screens and small-sized displays in small home appliances
   * - Central Control Screen
     - `Smart Voice Touch Panel (86 Box) Solution <https://www.bilibili.com/video/BV12G4y1o7A7>`__
     - ESP32-S3
     - 3.95-inch, 480 x 480 RGB interface screen
     - Integrated with Wi-Fi, BLE, offline voice recognition, RGB LCD display, supports offline command words and continuous voice recognition
     - Upgraded iteration for traditional 86 panels, serving as the central hub for smart home control integrating device control, switch panels, temperature control panels, and smart remote controls
   * - Central Control Screen
     - `Smart Central Control Switch Solution <https://www.bilibili.com/video/BV1g44y1S7zn>`__
     - ESP32-S3
     - 7-inch, 800 x 480 RGB interface screen
     - Multi-touch screen for gesture recognition, supports Wi-Fi CSI human body proximity sensing, used for building home visual panels
     - Quick switch control in smart homes, such as scene mode switching and light control
   * - Visual Voice Solution
     - `Visual Voice Solution <https://www.bilibili.com/video/BV1ZM411k7tZ>`__
     - ESP32-S3
     - 4.3-inch, 800 x 480 RGB interface screen
     - Utilizes native USB for connecting to a generic USB camera, achieving camera data stream reading, JPEG decoding, and real-time display on an RGB interface screen on a single SoC without the need for additional USB chips. Local video decoding and screen refresh resolution can reach 800 x 480 @15 FPS
     - Applications include cat's eye cameras, smart doorbells, smart door locks, electronic endoscopes, and more.
   * - High-Performance Multimedia Solution
     - `High-Performance Multimedia Solution <https://www.bilibili.com/video/BV18m421s7p4/>`__
     - ESP32-P4
     - 8-inch, 800 x 1280 MIPI-DSI interface screen
     - Supporting MIPI-CSI and MIPI-DSI interfaces, it is suitable for various scenarios requiring high-resolution cameras and displays. It incorporates hardware accelerators for multiple media encoding and compression protocols, a hardware pixel processing accelerator (PPA), and 2D-DMA, making it ideal for diverse multimedia applications.
     - Designed for high-performance multimedia applications

LCD Reference Solutions
------------------------

ESP-BOX
^^^^^^^^^^^^^^^^^

| **Description:**

A home appliance control platform developed for voice assistants, touchscreen controllers, sensors, infrared controllers, and smart Wi-Fi gateways.

| **Hardware:**

- Development Board: `ESP32-S3-BOX-3 <https://github.com/espressif/esp-box/blob/master/docs/hardware_overview/esp32_s3_box_3/hardware_overview_for_box_3.md>`__

| **Related Links:**

- Code Repository: `esp-box <https://github.com/espressif/esp-box/tree/master/examples/factory_demo>`__
- Related Video: `ESP32-S3-BOX-3 Surprise Unboxing! <https://www.bilibili.com/video/BV1aC4y177rf/?spm_id_from=333.999.0.0&vd_source=2dce370e48c5640c913869575b606ebe>`__

| **Features:**

- Based on LVGL GUI framework
- Dual-microphone far-field voice interaction, offline AI recognition in Chinese and English, supports over 200 voice commands
- Integrated end-to-end AIoT development framework ESP-RainMaker
- Pmod™ compatible connector supports peripheral modules for expanding sensors, infrared controllers, etc.
- PSRAM requirement: 8-bit (8M)

ESP32-C3 Rotary Screen
^^^^^^^^^^^^^^^^^^^^^^^^

| **Description:**

Circular rotary screen solution, integrating common scenarios such as washing machines, dimmers, and thermostats.

| **Hardware:**

- Development Board: `ESP32-C3-LCDkit <https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c3/esp32-c3-lcdkit/user_guide.html>`__

| **Related Links:**

- Code Repository: `esp32-c3-lcdkit <https://github.com/espressif/esp-dev-kits/tree/master/esp32-c3-lcdkit/examples/knob_panel>`__
- Related Videos:
    - `ESP32-C3 Rotary Screen Demo <https://www.bilibili.com/video/BV1nG4y1d7Ja/?share_source=copy_web&vd_source=0f97bc013051255d733b8e1e7bf54008>`__
    - `ESP32-C3-LCDKit Rotary Screen Development Board <https://www.bilibili.com/video/BV1GM4y177A6/?spm_id_from=333.999.0.0&vd_source=2dce370e48c5640c913869575b606ebe>`__

| **Features:**

- Based on LVGL GUI framework
- Circular screen UI display (non-touch), controlled by a rotary encoder

Intelligent Voice Touch Panel (86 Box)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

| **Description:**

Can be used for the upgrade iteration of traditional 86 panels, building an intelligent home control center integrating device control, switch panel, temperature control panel, smart remote control, etc.

| **Hardware:**

- Development Board: `ESP32-S3-LCD-EV-Board <https://docs.espressif.com/projects/espressif-esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/user_guide.html>`__
- Screen: LCD Subboard 2 (480x480)

| **Related Links:**

- Code Repository: `esp32-s3-lcd-ev-board/86-box Smart Panel Example <https://github.com/espressif/esp-dev-kits/tree/master/esp32-s3-lcd-ev-board/examples/86box_smart_panel>`__
- Related Video: `ESP32-S3 Smart Voice Touch Panel <https://www.bilibili.com/video/BV12G4y1o7A7/?share_source=copy_web&vd_source=0f97bc013051255d733b8e1e7bf54008>`__

| **Features:**

- Based on LVGL GUI framework
- Dual microphone far-field voice interaction, Chinese and English AI offline voice recognition, supports over 200 voice commands
- PSRAM requires 8 lines (R8) and enables 120M

Electronic Visual Doorbell
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

| **Description:**

Utilizes native USB interface to connect with a generic USB camera, achieving camera data stream reading, JPEG decoding, and real-time display on an RGB interface screen on a single SoC without the need for additional USB chips. The local video decoding and screen refresh resolution can reach 800x480@15 FPS.

| **Hardware:**

- Development Board: `ESP32-S3-LCD-EV-Board <https://docs.espressif.com/projects/espressif-esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/user_guide.html>`__
- Screen: LCD Subboard 3 (800x480)

| **Related Links:**

- Code Repository: `esp32-s3-lcd-ev-board/USB Camera LCD Example <https://github.com/espressif/esp-dev-kits/tree/master/esp32-s3-lcd-ev-board/examples/usb_camera_lcd>`__
- Related Video: `ESP32-S3 Driving RGB Interface Screen + USB CDC Camera Demo <https://www.bilibili.com/video/BV1ZM411k7tZ/?share_source=copy_web&vd_source=0f97bc013051255d733b8e1e7bf54008>`__

| **Features:**

- USB camera data stream reading, requires support for Bulk mode
- JPEG decoding
- 800x480 RGB LCD display
- PSRAM requires 8 lines (R8) and enables 120M

Smart Central Control Switch
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

| **Description:**

Recognizes gestures such as double-finger tapping and patting through a multi-touch screen, which can be used for quick switch control in smart homes, such as scene mode switching and light control. Combined with Wi-Fi CSI human body proximity sensing function, it can also achieve automatic on-off control of screen proximity wake-up and screen-off.

| **Hardware:**

- Development Board: `ESP32-S3-LCD-EV-Board <https://docs.espressif.com/projects/espressif-esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/user_guide.html>`__
- Screen: 7-inch, RGB interface, 800x480 resolution

| **Related Links:**

- Related Video: `ESP32-S3 Driving Super Large RGB Interface Screen <https://www.bilibili.com/video/BV1g44y1S7zn/?share_source=copy_web&vd_source=0f97bc013051255d733b8e1e7bf54008>`__

| **Features:**

- 7-inch large LCD screen, supports multi-touch
- Wi-Fi CSI human body proximity sensing
- PSRAM requires 8 lines (R8) and enables 120M

High-Performance Multimedia Solution
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

| **Description:**

Supporting MIPI-CSI and MIPI-DSI interfaces, it is suitable for various scenarios requiring high-resolution cameras and displays, with hardware accelerators for multiple media encoding and compression protocols, a hardware pixel processing accelerator (PPA), and 2D-DMA, making it suitable for a wide range of multimedia applications.

| **Hardware:**

- Development Board: ESP32-P4_Function_EV_Board
- Screen: 8-inch 800 x 1280 LCD screen (IC: ILI9881C)

| **Related Links:**

- Related Video: `Challenge: Building a Smartphone with ESP32-P4 <https://www.bilibili.com/video/BV18m421s7p4/>`__

| **Features:**

- Supports MIPI-DSI and MIPI-CSI interfaces
- Hardware accelerators for various media encoding and compression protocols
- Hardware pixel processing accelerator (PPA) and 2D-DMA

LCD Reference Materials
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- LCD Software References

  - `ESP LCD Driver Library <https://github.com/espressif/esp-idf/tree/master/components/esp_lcd>`_
  - `Arduino LCD Driver Library <https://github.com/esp-arduino-libs/ESP32_Display_Panel>`_
  - `ESP LCD Driver Documentation <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/lcd/index.html>`_
  - `ESP LCD Examples <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/lcd>`_
  - `ESP-BOX AIoT Development Framework <https://github.com/espressif/esp-box>`_

- LCD Solutions & Development Guides

  - `ESP-HMI Smart Display Solution <https://www.espressif.com/solutions/hmi/smart-displays>`_
  - `Quick Start GUI (Part 1) <https://www.bilibili.com/read/cv19147389?spm_id_from=333.999.0.0>`_
  - `Quick Start GUI (Part 2) <https://www.bilibili.com/read/cv19354117?spm_id_from=333.999.0.0>`_
  - `ESP LCD Development Guide <https://docs.espressif.com/projects/espressif-esp-iot-solution/en/latest/display/screen.html>`_

- Purchase Links for LCD Development Boards

  - `ESP32-S3-LCD-EV-Board Purchase Link <https://item.taobao.com/item.htm?spm=a1z10.5-c.w4002-8715811636.23.4bc567d8eBiLiI&id=680580609719>`_
  - `ESP32-S3-BOX Purchase Link <https://item.taobao.com/item.htm?spm=a1z10.1-c.w4004-10057817917.13.36b47931Jgygni&id=732842971319&skuId=5456418374248>`_
  - `ESP32-S3-BOX-Lite Purchase Link <https://item.taobao.com/item.htm?spm=a1z10.3-c.w4002-8715811646.10.1b7719b0Af0aPp&id=658634202331>`_
  - `ESP32-C3-LCDkit Purchase Link <https://item.taobao.com/item.htm?spm=a1z10.5-c.w4002-8715811636.21.74eb570eCig1wf&id=722089055506>`_

- Module/Development Board References and Options

  - `ESP32-S3 Datasheet <https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf>`_
  - `ESP32-S3-WROOM-1 Datasheet <https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf>`_
  - `ESP32-C3 Datasheet <https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf>`_
  - `ESP32-C3-MINI-1 Datasheet <https://www.espressif.com/sites/default/files/documentation/esp32-c3-mini-1_datasheet_en.pdf>`_
  - `Espressif Product Selection Tool <https://products.espressif.com/>`_