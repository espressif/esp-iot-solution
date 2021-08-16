# ESP MODEM

> Porting From UART To USB

This component is used to communicate with modems in command and data modes. 
It abstracts the USB I/O processing into a DTE (data terminal equipment) entity which is configured and setup separately.
On top of the DTE, the command and data processing is performed in a DCE (data communication equipment) unit.
```
   +-----+   
   | DTE |--+
   +-----+  |   +-------+
            +-->|       |
   +-----+      |       |
   | DTE |----->| modem |---> modem events
   +-----+      |       |
             +->|       |
   +------+  |  +-------+ 
   | PPP  |--+   |   |
   | netif|-----------------> network events
   +------+      |   |
                 |   |
    start-ppp----+   |
    stop-ppp---------+
```

## Start-up sequence

To initialize the modem we typically:
* create DTE with desired USB parameters
* create DCE with desired command palette
* create PPP netif with desired network parameters
* attach the DTE, DCE and PPP together to start the esp-modem
* configure event handlers for the modem and the netif

Then we can start and stop PPP mode using esp-modem API, receive events from the `ESP_MODEM` base, as well as from netif (IP events).

### DTE

Responsibilities of the DTE unit are
* sending/receiving commands to/from USB 
* sending/receiving data to/from USB 
* changing data/command mode on physical layer

### DCE

Responsibilities of the DCE unit are
* definition of available commands
* cooperate with DTE on changing data/command mode

#### DCE configuration

DCE unit configuration structure contains:
* device -- sets up predefined commands for this specific modem device
* pdp context -- used to setup cellular network
* populate command list -- uses runtime configurable linked list of commands. New commands could be added and/or 
existing commands could be modified. This might be useful for some applications, which have to (re)define
and execute multiple AT commands. For other applications, which typically just connect to network, this option
might be preferably turned off (as the consumes certain amount of data memory)

### PPP-netif

The modem-netif attaches the network interface (which was created outside of esp-modem) to the DTE and
serves as a glue layer between esp-netif and esp-modem.

### Additional units

ESP-MODEM provides also provides a helper module to define a custom retry/reset strategy using:
* a very simple GPIO pulse generator, which could be typically used to reset or power up/down the module
* a command executor abstraction, that helps with retrying sending commands in case of a failure or a timeout
and run user defined recovery actions.

## Modification of existing DCE

In order to support an arbitrary modem, device or introduce a new command we typically have to either modify the DCE,
adding a new or altering an existing command or creating a new "subclass" of the existing DCE variants.

