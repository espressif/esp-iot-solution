# IoT USB Modem

This component using ESP32-S2, ESP32-S3 series SoC as a USB host to dial-up 4G Cat.1 through PPP to access the Internet, with the help of ESP32-Sx Wi-Fi softAP function, share the Internet with IoT devices or mobile devices. Realize low-cost "medium-high-speed" Internet access.

**Features Supported:**

* Compatible with mainstream 4G module AT commands
* USB PPP dial-up
* USB Dual Interface
* Network self-recovery
* Wi-Fi hotspot

**Supported 4G Cat.1 Module:** 

|      Name       |   PPP   | Secondary AT Port |
| :-------------: | :-----: | :---------------: |
| ML302-DNLM/CNLM | Support |    No-Support     |
|  Air724UG-NFM   | Support |      unknown      |
| EC600N-CNLA-N05 | Support |      unknown      |
| EC600N-CNLC-N06 | Support |      unknown      |
| SIMCom A7600C1  | Support |      unknown      |
|     BG95_M3     | Support |      unknown      |
|     BG96_MA     | Support |      unknown      |
|    MC610_EU     | Support |      Support      |

> Secondary AT Port can be used for AT command when main modem port is in ppp network mode

> There are different sub-models of the above modules. The communication stands may be different. For example, LTE-FDD 5(UL)/10(DL), LTE-TDD 1(UL)/8(DL). And endpoint address may different also, if you encounter problems, try modifying parameters using a custom device mode.

**Other 4G Cat.1 module adaptation methods**

1. Confirm whether the 4G module **supports USB Fullspeed mode**;
2. Confirm whether the 4G module **supports USB PPP dial-up interface**;
3. Confirm that the **4G SIM card is activated** and the Internet access is turned on;
4. Confirm that the **necessary signal wires have been connected** in accordance with the hardware wiring;
5. Confirm the module's PPP interface input endpoint (IN) and output endpoint (OUT) addresses, and modify the following options in `menuconfig`: 

   * Choose a `User Defined` board:
   ```
   Component config → ESP-MODEM → Choose Modem Board → User Defined
                                
   ```
   * Configure the endpoint address of 4G Modem:
   ```
   Component config → ESP-MODEM → USB CDC endpoint address config
                                        → Modem USB CDC IN endpoint address
                                        → Modem USB CDC OUT endpoint address
   ```

6. Check outputs log to confirm that the `AT` command can be executed;

> The basic AT commands supported by different Cat.1 chip platforms are roughly the same, but there may be some special commands that need to be supported by users.

## Examples

* [USB CDC 4G Module](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/host/usb_cdc_4g_module)

