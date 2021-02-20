
## Description

Atmel [AT24C01A/02/04/08A/16A](http://ww1.microchip.com/downloads/en/devicedoc/doc0180.pdf) provides 1024/2048/4096/8192/16384 bits of serial electrically erasable and programmable read-only memory (EEPROM) organized as 128/256/512/1024/2048 words of 8 bits each. The device is optimized for use in many industrial and commercial applications where low-power and low-voltage operation are essential.

* Low-voltage and Standard-voltage Operation
  * 2.7 (VCC = 2.7V to 5.5V)
  * 1.8 (VCC = 1.8V to 5.5V)
* I2C bus 100 kHz (1.8V) and 400 kHz (2.7V, 5V) Compatibility.
* 8-byte Page (1K, 2K), 16-byte Page (4K, 8K, 16K) Write Modes
* Partial Page Writes Allowed
* High-reliability
  * Endurance: 1 Million Write Cycles
  * Data Retention: 100 Years

## User Tips

### Device Address:

* The A2, A1 and A0 pins are device address inputs that are hard wired for the AT24C01A and the **AT24C02**. As many as eight 1K/2K devices may be addressed on a single bus system.
* But AT24C16A does not use the device address pins, which limits the number of
devices on a single bus to one. 

### Memory Address

* AT24C01A, 1K SERIAL EEPROM: Internally organized with 16 pages of 8 bytes each,the 1K requires a 7-bit data word address for random word addressing
* **AT24C02, 2K SERIAL EEPROM**: Internally organized with 32 pages of 8 bytes each, the 2K requires an **8-bit data word address for random word** addressing. 
* AT24C16A, 16K SERIAL EEPROM: Internally organized with 128 pages of 16 bytes each, the 16K requires an 11-bit data word address for random word addressing.

### Write Operations

* **Byte Write**: AT24C02 write operation requires an 8-bit data word address following the device address word and acknowledgment. 
* **PAGE WRITE**: 1K/2K EEPROM is capable of an 8-byte page write，after the EEPROM acknowledges receipt of the first data word, the microcontroller can transmit up to seven (1K/2K) more data words. But if more than eight (1K/2K) or sixteen (4K, 8K, 16K) data words are transmitted to the EEPROM, **the data word address will “roll over” and previous data will be overwritten.**

### Read Operations

* **CURRENT ADDRESS READ:** The internal data word address counter maintains the last address accessed during the last read or write operation, incremented by one. This address stays valid between operations as long as the chip power is maintained. **The address “roll over” during read is from the last byte of the last memory page to the first byte of the first page**.
* **RANDOM READ:** A random read requires a “dummy” byte write sequence to load in the data word address. Once the device address word and data word address are clocked in and acknowledged by the EEPROM, the microcontroller must generate another start condition.
* **SEQUENTIAL READ**: Sequential reads are initiated by either a current address read or a random address read. As long as the EEPROM receives an acknowledge, it will continue to increment the data word address and serially clock out sequential data words. When the memory address limit is reached, the data word address will “roll over” and the sequential read will continue.

>The definition of “roll over” during read is different from write

### Write Protect(WP)

* The **AT24C01A/02/04/08A/16A** has a Write Protect pin that provides hardware data protection. 
* The Write Protect pin allows normal Read/Write operations when connected to ground (GND). When the Write Protect pin is connected to VCC, the write protection feature is enabled