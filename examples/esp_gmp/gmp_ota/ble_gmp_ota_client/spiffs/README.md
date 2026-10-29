Place the firmware to upload here as `ota_image.bin`, then rebuild and flash the client project.

Example:

```bash
cp ../ble_gmp_ota_server/build/ble_gmp_ota_server.bin ota_image.bin
cd .. && idf.py build flash monitor
```

After adding or updating `ota_image.bin`, run `idf.py build flash` again so `storage.bin` is regenerated and written to the `storage` SPIFFS partition (`0x110000`).

Verify flash includes SPIFFS: `build/flash_args` should list `0x110000 storage.bin`.

At runtime the client reads `/spiffs/ota_image.bin`.
