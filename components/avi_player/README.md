[![Component Registry](https://components.espressif.com/components/espressif/avi_player/badge.svg)](https://components.espressif.com/components/espressif/avi_player)

# Component: AVI Player
[Online documentation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/input_device/avi_player.html)

The `avi_player` component can parse avi format data and pass the audio/video frames through callback.

Currently supported features:
* Parse avi from file system
* Parse avi from memory
* mjpeg video stream
* pcm audio stream

## Add component to your project

Please use the component manager command `add-dependency` to add the `avi_player` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/avi_player=*"
```
