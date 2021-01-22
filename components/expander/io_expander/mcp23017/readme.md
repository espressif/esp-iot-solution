
## Description

The [MCP23017/MCP23S17](https://ww1.microchip.com/downloads/en/devicedoc/20001952c.pdf) (MCP23X17) device family provides 16-bit, general purpose parallel I/O expansion for I2C bus or SPI applications. The two devices differ only in the serial interface:

* MCP23017 – I2C interface
* MCP23S17 – SPI interface

The MCP23X17 consists of multiple 8-bit configuration registers for input, output and polarity selection. The system master can enable the I/Os as either inputs or outputs by writing the I/O configuration bits (IODIRA/B). The data for each input or output is kept in the corresponding input or output register. The polarity of the Input Port register can be inverted with the Polarity Inversion register. All registers can be read by the system master.

The 16-bit I/O port functionally consists of two 8-bit ports (PORTA and PORTB). The MCP23X17 can be configured to operate in the 8-bit or 16-bit modes via IOCON.BANK. 

The Interrupt Capture register captures port values at the time of the interrupt, thereby saving the condition that caused the interrupt. The Power-on Reset (POR) sets the registers to their default values and initializes the device state machine.

## User Tips

### Device Address

The hardware address pins are used to determine the device address. Cascadable for up to 8 devices on one bus

### Interrupt Pin

There are two interrupt pins, INTA and INTB, that can be associated with their respective ports, or can be logically OR’ed together so that both pins will activate if either port causes an interrupt. The interrupt output can be configured to activate under two conditions (mutually exclusive):

1. When any input state differs from its corresponding Input Port register state. This is used to indicate to the system master that an input state has changed.
2. When an input state differs from a preconfigured register value (DEFVAL register).

