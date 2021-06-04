## Patch Apply Guide

> This patch only for test internal

1. Install latest `master` branch [ESP-IDF](https://github.com/espressif/esp-idf)
2. Copy `usb_support_workaround.patch` to ESP-IDF folder
3. CD in to ESP-IDF folder
4. Run command below

```
git checkout 94118efe64c72
git apply --check usb_support_workaround.patch
git am --signoff < usb_support_workaround.patch
git submodule update --init --recursive
```