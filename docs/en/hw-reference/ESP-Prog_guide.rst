:orphan:

Introduction to the ESP-Prog Board
==================================

:link_to_translation:`zh_CN:[中文]`


1. Overview
-----------

ESP-Prog is one of Espressif’s development and debugging tools, with
functions including automatic firmware downloading, serial
communication, and JTAG online debugging. ESP-Prog's automatic firmware
downloading and serial communication functions are supported on both the
ESP8266 and ESP32 platforms, while the JTAG online debugging is
supported only on the ESP32 platform.

ESP-Prog can easily connect to a PC with the use of only one USB cable.
Then, the PC can identify the board's downloading and JTAG interfaces
(functions) by their port numbers.

Given that the power supply voltage may vary on different user boards,
either of the ESP-Prog interfaces can provide the 5V or the 3.3V power
supply through pin headers, in order to ensure power compatibility.

2. System Diagram
-----------------

ESP-Prog's overall functional block diagram is displayed below:

.. figure:: ../../_static/hw-reference/esp-prog/block.png
   :align: center

3. Hardware Introduction
------------------------

The figure below describes the areas of each function on the ESP-Prog
board.

.. figure:: ../../_static/hw-reference/esp-prog/modules.png
   :align: center

3.1. PCB Layout and Dimensions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

ESP-Prog's PCB layout is displayed below, showing the board's dimensions
and its interface markings. Please refer to `Espressif
website <http://espressif.com/en/support/download/documents?keys=Reference+Design>`__
for the hardware resources including schematics, PCB reference design,
BOM and other files.

-  Top side

.. figure:: ../../_static/hw-reference/esp-prog/top.jpg
   :align: center

-  Bottom side

.. figure:: ../../_static/hw-reference/esp-prog/bottom.jpg
   :align: center

3.2. Introduction to Functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

3.2.1. The Working Mode of USB Bridge
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

ESP-Prog uses FT2232HL, which is provided by FTDI, as its USB Bridge
Controller chip. The board can be configured to convert the USB 2.0
interface to serial and parallel interfaces that support a wide range of
industry standards. ESP-Prog uses FT2232HL's default dual-asynchronous
serial interface mode, allowing users to use the board easily by
installing the `FT2232HLdriver <http://www.ftdichip.com/Drivers/VCP.htm>`__ on their PCs.

.. Note:: The PC is able to identify the ESP-Prog's two ports by their port numbers. The bigger port number represents the Program interface, while the other one represents the JTAG interface.

3.2.2. Communication Interface
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

ESP-Prog can connect to ESP32 user boards, using both the Program
interface and the JTAG interface. Users should connect the user board
interfaces in a way that corresponds to the ESP-Prog board.

-  **Program Interface**
   The Program interface has six pins, including the UART interface
   (TXD, RXD), boot mode selection pin (ESPIO0) and reset pin (ESPEN).
   The design for the Program interface on the user board should follow
   the reference provided in the figure below.

.. figure:: ../../_static/hw-reference/esp-prog/program_pin.png
   :align: center

-  **JTAG Interface**
   The design for the JTAG interface on the user board should follow the
   reference provided in the figure below.

.. figure:: ../../_static/hw-reference/esp-prog/JTAG_pin.png
   :align: center

-  **Fool-proof Design**
   The ESP-Prog board uses header connectors (DC3-6P / DC3-10P) which
   support reverse-current circuitry protection. In such cases, it is
   recommended that users also use header connectors on their user
   boards, such as ``FTSH-105-01-S-DV-*`` or ``DC3-*P``.

.. Note:: The flat cables used here are directional. Please use the cables provided by Espressif.

3.2.3. Automatic Downloading Function
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

ESP-Prog supports automatic downloading. After connecting the Program
interface of ESP-Prog to the user board, the downloading program can
download data or run programs automatically by controlling the states of
the start-mode selection pin (ESPIO0) and reset pin (ESPEN), which
spares the users from manually restarting the device and selecting the
downloading modes. The two buttons on the ESP-Prog board enable users to
reset and control the boot mode of the device manually.

The schematics of the automatic downloading circuit is displayed below.

.. figure:: ../../_static/hw-reference/esp-prog/Auto_program.png
   :align: center

3.2.4. Delay Circuit
^^^^^^^^^^^^^^^^^^^^

The delay circuit of ESP-Prog includes the bus buffer, inverter, MOS
tube, first-order RC circuit, and other components. This delay circuit
ensures that the ESP32 chip can power up or reset itself, before
connecting with the JTAG signal, thus protecting the chip from the
influence of JTAG on power-up or reset.

.. figure:: ../../_static/hw-reference/esp-prog/delay_cricuit.png
   :align: center

3.2.5. LED Status Indication
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  The red LED lights up when the system is connected to the 3.3V power;
-  The green LED lights up when ESP-Prog is downloading data to ESP32;
-  The blue LED lights up when ESP-Prog is receiving data from ESP32.

.. figure:: ../../_static/hw-reference/esp-prog/prog_led.jpg
   :align: center

3.2.6. Pin Headers
^^^^^^^^^^^^^^^^^^

Users can choose either the 3.3V or 5V power supply for the Program and
JTAG interfaces, using the pin headers shown in the figure below.

-  **Pin header to select power supply**
   The pin header in the middle is the power input pin for each
   interface. When this pin is connected to 5V, the power output of the
   interface is 5V. When this pin is connected to 3.3V, the power output
   of the interface is 3.3V.

-  **IO0 On/Off Pin**
   Pin IO0 can be set to select ESP8266's and ESP32's boot modes. This
   pin can be used as a common GPIO, after the chip is powered on. Users
   can then disconnect Pin IO0 manually to protect the operation of the
   user board from the influence of ESP-Prog's automatic downloading
   circuit.

.. figure:: ../../_static/hw-reference/esp-prog/prog_power_sel.jpg
   :align: center

4. Step by Step Instruction
---------------------------

1. Connect the ESP-Prog board and the PC USB port via a USB cable.
2. Install the `FT2232HL chip
   driver <http://www.ftdichip.com/Drivers/VCP.htm>`__ on your PC. The
   PC then detects the two ports of ESP-Prog, indicating that the driver
   has been installed successfully.
3. Select the output power voltage for the Program / JTAG interfaces,
   using pin headers.
4. Connect the ESP-Prog and ESP user boards with the gray flat cables
   provided by Espressif.
5. Start automatic downloading or JTAG debugging, using the official
   software tools or scripts provided by Espressif.

5. Useful Links
---------------

-  `Espressif's Official Website <https://espressif.com>`__

-  **How to buy**: espressif\_systems (WeChat Account), `Purchase
   consulting <http://www.espressif.com/en/company/contact/pre-sale-questions-crm>`__

-  `ESP-Prog schematics, PCB reference design, BOM <http://espressif.com/en/support/download/documents?keys=Reference+Design>`__

-  `Introduction to the ESP32 JTAG
   Debugging <https://docs.espressif.com/projects/esp-idf/en/stable/api-guides/jtag-debugging/index.html#>`__

-  `Flash Download Tools (ESP8266 &
   ESP32) <http://espressif.com/en/support/download/other-tools>`__

-  `FT2232HL Chip Driver <http://www.ftdichip.com/Drivers/VCP.htm>`__


