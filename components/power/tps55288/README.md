# TPS55288 Buck-Boost Converter

I2C driver for the TI TPS55288 buck-boost converter.

Internal feedback defaults to a ratio of 0.0564, so `VOUT = 0.8 V + REF * 20 mV` (800–21260 mV, 20 mV step). Set `fb_ratio` in `tps55288_config_t` to choose another internal ratio. Zero keeps this default. The other ratios are 0.0752 (15 mV step), 0.1128 (10 mV step), and 0.2256 (5 mV step).

`sense_resistor_mohm` is the resistor soldered between ISP and ISN. It has no driver default, because the value is fixed by the board. The output current limit is `I(mA) = code * 500 / R(mΩ)`.

See `examples/power/tps55288`. The 7-bit address is `0x74` or `0x75`, selected by the MODE pin. EN/UVLO must be high before the device answers on I2C.

[TPS55288 datasheet](https://www.ti.com/lit/ds/symlink/tps55288.pdf)
