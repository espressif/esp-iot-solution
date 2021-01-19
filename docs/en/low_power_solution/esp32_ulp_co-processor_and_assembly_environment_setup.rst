:orphan:

Introduction to the ESP32 ULP Co-processor and Assembly Environment Setup
=========================================================================

:link_to_translation:`zh_CN:[中文]`

This document introduces the ESP32 ULP co-processor and provides a
walkthrough for setting up the assesmbly environment.

1. Introduction
---------------

The ULP co-processor is an ultra-low-power co-processor that remains
powered on whether the main CPU is in operating mode or in deep sleep.
Hence, the ULP co-processor makes ESP32 SoC well suited for low-power
application scenarios.

The ESP32 ULP co-processor has the following key features:

-  Contains up to 8 KB of SRAM for instructions and data
-  Uses RTC\_FAST\_CLK, which is 8 MHz
-  Has built-in ADC and I2C interface
-  Works both in normal and deep sleep mode
-  Is able to wake up the digital core or send an interrupt to the CPU
-  Can access peripheral devices, internal sensors and RTC registers

For more information, you can refer to `ESP32 Technical Reference
Manual <http://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf>`__.

2. Power Consumption of the ULP Co-processor
--------------------------------------------

The ULP co-processor can perform ADC sampling, read/write I2C sensor,
drive RTC GPIO, etc, with minimum power consumption. In some cases, it
can be a replacement of the main CPU.

The table below lists the power consumption while the ULP co-processor
works in different scenarios.

+---------------------+---------------------+
| Scenarios           | Power consumption   |
+=====================+=====================+
| Deep-sleep          | 6 μA                |
+---------------------+---------------------+
| Delay Nop           | 1.4 mA              |
+---------------------+---------------------+
| GPIO Toggle         | 1.4 mA              |
+---------------------+---------------------+
| SAR\_ADC Sampling   | 2.3 mA              |
+---------------------+---------------------+

**Note:** Power consumption listed above is for the ULP co-processor
powered by VDD3P3\_RTC and running at 8 MHz, without going into deep
sleep. These figures may be considerably reduced when the ULP is put
into deep sleep and woken up periodically to work efficiently for a
short period of time.

3. Set up ULP Co-processor Assembly Environment
-----------------------------------------------

The ULP co-processor can only be developed in assembly and compiled
using the toolchain. We provide instructions for the major three
platforms: Linux, Windows and MacOS.

3.1 Linux
^^^^^^^^^

1. `Download the toolchain for
   Linux(x64) <https://github.com/espressif/binutils-esp32ulp/wiki>`__
   and extract it into a directory.
2. Add the path to the ``bin`` directory of the toolchain to the
   ``PATH`` environment variable. For example, if the directory is
   ``/opt/esp32ulp-elf-binutils``, then add
   ``export PATH=/opt/esp32ulp-elf-binutils/bin:$PATH`` to ``.bashrc``.
3. Run ``source .bashrc`` to enable the environment variable.

The ULP co-processor assembly enviroment is set up now.

3.2 Windows
^^^^^^^^^^^

1. Install download the Windows all-in-one toolchain & MSYS2 zip file.
   Please refer to `standard setup of toolchain for
   windows <https://docs.espressif.com/projects/esp-idf/en/stable/get-started/windows-setup.html>`__.
2. Download `ULP
   toolchain <https://github.com/espressif/binutils-esp32ulp/wiki#downloads>`__.
3. Unzlip the ULP toolchain ``esp32ulp-elf-binutils-win32-...`` into
   MSYS2's ``opt`` directory. ``C:\msys32\opt`` directory is
   recommended, since it also contains the ESP32 toolchain
   ``xtensa-esp32-elf``.
4. Open ``esp32_toolchain.sh`` file in ``C:\msys32\etc\profile.d``
   directory for editing and add the path to the ``bin`` directory of
   the toolchain to the ``PATH`` environment variable, as shown below:

   ::

       # This file was created by ESP-IDF windows_install_prerequisites.sh
       # and will be overwritten if that script is run again.
       export PATH="$PATH:/opt/xtensa-esp32-elf/bin:/opt/esp32ulp-elf-binutils/bin"

5. Save the file and restart MSYS2 console.

Now you should be able to compile ULP programs, e.g.
:example:`system/ulp_adc`

3.3 MacOS
^^^^^^^^^

It's pretty much the same way as on Linux. Download ULP toolchain for
MacOS, extract it, and add it to your PATH. 1. `Download the toolchain
for MacOS <https://github.com/espressif/binutils-esp32ulp/wiki>`__. 2.
Extract the toolchain into a directory, and add the path to the ``bin/``
directory of the toolchain to the ``PATH`` environment variable.

4. Compiling ULP code
---------------------

Follow the steps below to compile ULP code as part of a component:

1.ULP code, written in assembly, must be added to one or more files with  .S extension . These files must be placed into a separate directory inside component directory, for instance ulp/. 

2. Modify the component makefile, adding the following:

::

    ULP_APP_NAME ?= ulp_$(COMPONENT_NAME)
    ULP_S_SOURCES = $(COMPONENT_PATH)/ulp/ulp_source_file.S
    ULP_EXP_DEP_OBJECTS := main.o
    include $(IDF_PATH)/components/ulp/component_ulp_common.mk

Each line is explained in `Compiling ULP
Code <https://docs.espressif.com/projects/esp-idf/en/stable/api-guides/ulp.html#compiling-ulp-code>`__.

3. Build the application as usual (e.g. make app).

5. Assembly Instructions
------------------------

There are 24 assembly instructions for the ULP co-processor that are
documented in details in `ULP coprocessor instruction
set <https://docs.espressif.com/projects/esp-idf/en/stable/api-guides/ulp_instruction_set.html>`__.

Algorithm and logical instructions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  Algorithm instructions: ADD (add to register), SUB (subtract from
   register)
-  Logical instructions: AND, OR
-  Logical shift instructions: LSH (logical shift left), RSH (logical
   shift right)
-  Register instructions: MOVE (move to register)
-  Count register instructions: STAGE\_RST (reset stage count register),
   STAGE\_INC (increment stage count register), STAGE\_DEC (decrement
   stage count register)

Data loading and storing instructions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  Load data from memory: LD
-  Store data to memory: ST
-  Read from peripheral register: REG\_RD
-  Write to peripheral register: REG\_WR

Jumping instructions
^^^^^^^^^^^^^^^^^^^^

-  Jump to an absolute address: JUMP
-  Jump to a relative offset (condition based on R0): JUMPR
-  Jump to a relative address (condition based on stage count): JUMPS

Measurement instructions
^^^^^^^^^^^^^^^^^^^^^^^^

-  Measurement with ADC: ADC
-  Measurement with temperature sensor: TSENS

I2C communication instructions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  Read single byte from I2C slave: I2C\_RD
-  Write single byte to I2C slave: I2C\_WR

Program execution management instructions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  Wait some number of cycles: WAIT
-  End the program: HALT

ULP sleep period setting instructions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  Set ULP wakeup timer period: SLEEP

CPU waking instructions
^^^^^^^^^^^^^^^^^^^^^^^

-  Wake up the CPU: WAKE

6. Examples
-----------

You can find some ULP co-processor examples within
`esp-iot-solution <https:404/tree/master/examples/ulp_examples>`__.
More examples will be added later.

+-------+-------------------------+---------------------------------------------------------------------------+
| No.   | Examples                | Note                                                                      |
+=======+=========================+===========================================================================+
| 1     | ulp\_hall\_sensor       | An example of how to read hall sensor in ULP mode                         |
+-------+-------------------------+---------------------------------------------------------------------------+
| 2     | ulp\_rtc\_gpio          | An example of how to operate RTC GPIO pins toggle in ULP mode             |
+-------+-------------------------+---------------------------------------------------------------------------+
| 3     | ulp\_tsens              | An example of how to read on-chip temperature sensor in ULP mode          |
+-------+-------------------------+---------------------------------------------------------------------------+
| 4     | ulp\_watering\_device   | An application demo of watering machine based on ESP32 ULP co-processor   |
+-------+-------------------------+---------------------------------------------------------------------------+

7. Related Documents
--------------------

-  `Low Power
   Solution <https:404/tree/master/documents/low_power_solution>`__
-  `ULP Co-processor
   Programming <https://docs.espressif.com/projects/esp-idf/en/stable/api-guides/ulp.html>`__

