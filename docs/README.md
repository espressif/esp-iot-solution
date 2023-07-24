# Documentation Source Folder

This folder contains source files of **ESP-IDF documentation** available in [English](https://docs.espressif.com/projects/esp-iot-solution/en/latest/index.html) and [中文](https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/index.html).

The sources do not render well in GitHub and some information is not visible at all.

Use actual documentation generated within about 20 minutes on each commit:

# Hosted Documentation

* English: https://docs.espressif.com/projects/esp-iot-solution/en/latest/index.html
* 中文: https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/index.html

The above URLs are all for the master branch latest version. Click the drop-down in the bottom left to choose a stable version or to download a PDF.

# Building Documentation

The documentation is built using the python package `esp-docs`, which can be installed by running `pip install esp-docs`. Running `build-docs --help` will give a summary of available options. For more information see the `esp-docs` documentation at https://github.com/espressif/esp-docs/blob/master/README.md

If you need to compile html files in this repository, you can use:

```
build-docs -t esp32 -l zh_CN -bs html
build-docs -t esp32 -l en -bs html
```

# Preview

To preview the compiled html files locally:

* Start a local web page
    
    ```
    python3 -m http.server 8000 --directory _build/zh_CN/esp32/html 
    ```

* Enter the URL: http://localhost:8000/ for online preview