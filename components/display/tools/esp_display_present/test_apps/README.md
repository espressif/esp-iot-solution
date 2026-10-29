# esp_display_present Test App

Unity tests for presenter lifecycle and exact rendered pixels on a software
`esp_lcd` panel. RGB-capable targets exercise PARTITION, DIRECT, and FULL with
all four rotations, padded partition strides, multi-band submissions, and
partial-frame repair. Assertions inspect the in-memory framebuffer directly;
the suite writes no image artifacts and requires no LCD hardware.

```sh
idf.py set-target esp32c3 build flash monitor
```
