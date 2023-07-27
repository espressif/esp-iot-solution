# Boards Component

As a common component of examples, this component can provide unified pin macro definitions and hardware-independent initialization operations to applications. Applications developed based on this component are compatible with different development boards at the same time.

* The Boards component contains the following:

    * ``board_common.h``, contains the function declaration of the common API;
    * ``board_common.c``, contains the function implementation of the common API (weak function);
    * ``Kconfig.projbuild``, contains common configuration items;

* The subfolders named after the development board name includes the following:

    * ``iot_board.h`` provides the gpio definition of the development board, and the board's unique custom API function declaration
    * ``board.c`` provides user implementation of common API (Covering default weak function), custom API function implementation
    * ``kconfig.in`` provides custom configuration items unique to the development board.

For details about how to add a custom development board, please refer [Boards Component](https://docs.espressif.com/projects/esp-iot-solution/en/latest/basic/boards.html)
