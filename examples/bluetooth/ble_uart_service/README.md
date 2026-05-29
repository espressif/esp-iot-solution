| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# Emote BLE UART OpenCode Device

This example is the **ESP32-S3 device-side adaptation** for the OpenCode BLE
UART bridge. It adapts the host-side OpenCode plugin protocol to the Emote
device UI: the device receives JSONL messages from the BLE UART daemon, mirrors
OpenCode session state on the display, and lets the user approve or reject
OpenCode permission requests from the touch key.

## Compatibility

- Adapted for **ESP-IDF release/v5.5**.
- Uses the BLE UART component from
  `$IDF_PATH/examples/bluetooth/common/ble_uart`, so the build follows the
  currently exported ESP-IDF environment.
- Covered by GitLab CI compile testing on `esp32s3` with the
  `espressif/idf:release-v5.5` image.

The matching OpenCode host-side demo is:

```bash
$IDF_PATH/tools/ble/ble_uart_bridge/demos/opencode
```

That directory is not firmware. It is an OpenCode plugin demo. The complete
data path is:

```text
OpenCode
  -> OpenCode BLE plugin
  -> tools/ble/ble_uart_bridge daemon
  -> BLE UART JSONL
  -> this ESP32-S3 device firmware
```

## Features

- BLE UART device using `$IDF_PATH/examples/bluetooth/common/ble_uart`.
- Receives OpenCode `session.status` events and displays busy / idle / retry
  state.
- Receives OpenCode `permission.request` events and displays the permission
  type, title, and compact metadata.
- Touch-key permission decisions:
  - single click: return `once`
  - long press: return `reject`
  - no input for 30 seconds: return `reject`
- Handles `permission.cancel` to clear stale permission prompts.
- Keeps legacy maintenance commands: `status`, `name`, and `unpair`.

See [json_format.md](json_format.md) for the device JSONL protocol.

## Code Layout

```text
main/
├── app_main.c        # NVS, BLE UART, protocol layer, and demo startup
├── ble_protocol.c    # BLE JSONL protocol, permission requests, session status, legacy commands
├── ble_protocol.h
├── board.c           # Display, panel touch, touch key, and emote player lifecycle
├── board.h
├── emote_demo.c      # Animations, tip text, and key-to-permission decision mapping
├── emote_demo.h
└── idf_component.yml
```

The top-level [CMakeLists.txt](CMakeLists.txt) uses the currently exported
`IDF_PATH`:

```cmake
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/examples/bluetooth/common/ble_uart")
```

## Build and Flash

```bash
cd esp-iot-solution/examples/bluetooth/ble_uart_service
. $IDF_PATH/export.sh
idf.py set-target esp32s3
idf.py build flash monitor
```

Use an ESP-IDF `release/v5.5` environment for the validated release build.

## CI Compile Test

This example is included in the GitLab CI build matrix. The CI job builds
`examples/bluetooth/ble_uart_service` with:

- target: `esp32s3`
- image: `espressif/idf:release-v5.5`
- build app manifest: [examples/.build-rules.yml](../../.build-rules.yml)

The default BLE device name is:

```text
emote-XXXX
```

`XXXX` is derived from the last two bytes of the BT MAC address. The legacy
`name` command can store a custom BLE name in NVS; reboot the device to apply it.

## Run With OpenCode

### 1. Install BLE UART Bridge Dependencies

```bash
cd $IDF_PATH
. ./export.sh
cd tools/ble/ble_uart_bridge
python -m pip install -r requirements.txt
```

### 2. Find and Connect the Device

Flash this example first and make sure the device is advertising.

```bash
cd $IDF_PATH/tools/ble/ble_uart_bridge
python main.py list-devices
python main.py connection-check <DEVICE_ID>
python main.py console <DEVICE_ID>
python main.py daemon <DEVICE_ID> --host 127.0.0.1 --port 8888
```

In another terminal, check daemon status:

```bash
cd $IDF_PATH/tools/ble/ble_uart_bridge
python main.py daemon-status
```

The OpenCode plugin connects to this endpoint by default:

```text
http://127.0.0.1:8888
```

To use another endpoint, set this before starting OpenCode:

```bash
export OPENCODE_BLE_DAEMON_URL="http://127.0.0.1:9999"
```

### 3. Install the OpenCode Plugin Demo

The OpenCode plugin demo is located at:

```bash
$IDF_PATH/tools/ble/ble_uart_bridge/demos/opencode
```

Project-level install example:

```bash
mkdir -p <project>/.opencode/plugins/opencode-ble-uart-bridge
cp $IDF_PATH/tools/ble/ble_uart_bridge/demos/opencode/src/*.ts \
   <project>/.opencode/plugins/opencode-ble-uart-bridge/
```

Merge this into `<project>/opencode.json`:

```json
{
  "plugin": [
    ".opencode/plugins/opencode-ble-uart-bridge/opencode-ble-uart-bridge.ts"
  ],
  "permission": {
    "edit": "ask"
  }
}
```

You can also use the provided example:

```bash
$IDF_PATH/tools/ble/ble_uart_bridge/demos/opencode/opencode.json.example
```

### 4. Trigger a Permission Request

Start OpenCode, then ask it to perform an operation that needs `edit`
permission. The plugin forwards `permission.asked` to the BLE device through the
daemon:

```json
{"v":1,"id":"<bridge-request-id>","op":"permission.request","data":{"v":1,"kind":"permission.request","payload":{"type":"edit","title":"Permission request","metadata":{"path":"..."}}}}
```

After the device shows the prompt:

- Single-click the touch key. The device replies:

  ```json
  {"v":1,"id":"<bridge-request-id>","ok":true,"data":{"decision":"once","message":"Approved from BLE device"}}
  ```

- Long-press the touch key. The device replies:

  ```json
  {"v":1,"id":"<bridge-request-id>","ok":true,"data":{"decision":"reject","message":"Rejected from BLE device"}}
  ```

The plugin receives the result and calls the OpenCode permission reply API.

## Message Mapping

| OpenCode event | Plugin to daemon | BLE `op` | Device behavior |
| -------------- | ---------------- | -------- | --------------- |
| `session.status` | `POST /notify` | `session.status` | Update busy / idle / retry UI, no reply |
| `permission.asked` | `POST /request` | `permission.request` | Show prompt, wait for key input, reply `once` or `reject` |
| Session becomes idle while a BLE prompt is stale | `POST /notify` | `permission.cancel` | Clear pending permission UI, no reply |

The device allows only one pending permission request at a time. Overlapping
requests return a `busy` error. The host-side plugin queues permission requests
before sending them to the device.

## Manual Testing

You can test the device through the daemon HTTP API without starting OpenCode.

Send a session status update:

```bash
curl -X POST http://127.0.0.1:8888/notify \
  -H 'Content-Type: application/json' \
  -d '{"op":"session.status","data":{"v":1,"kind":"session.status","event_id":"manual","session_id":"manual","requires_reply":false,"payload":{"type":"busy"}}}'
```

Send a permission request:

```bash
curl -X POST http://127.0.0.1:8888/request \
  -H 'Content-Type: application/json' \
  -d '{"op":"permission.request","timeout":60,"data":{"v":1,"kind":"permission.request","event_id":"manual","session_id":"manual","permission_id":"manual-perm","requires_reply":true,"payload":{"id":"manual-perm","sessionID":"manual","type":"bash","title":"Run command","metadata":{"command":"git status"}}}}'
```

The device should show a permission prompt. Single-click or long-press the touch
key, and the `curl` command should receive the daemon JSON response.

## Legacy Commands

These commands are for local maintenance and are not part of the OpenCode flow:

| Command | Description |
| ------- | ----------- |
| `{"cmd":"status"}` | Query heap, uptime, and BLE connection state |
| `{"cmd":"name","name":"..."}` | Save the BLE name to NVS; reboot to apply |
| `{"cmd":"unpair"}` | Clear bonded peers |

## Assets and Partitions

- On first CMake configure, [main/CMakeLists.txt](main/CMakeLists.txt)
  downloads `https://dl.espressif.com/AE/emote_assets.bin` into
  `build/prebuilt/emote_assets.bin`.
- `partitions.csv` defines the `emote_gen` SPIFFS partition with size `5500K`.
- `sdkconfig.defaults` targets `esp32s3` and enables NimBLE, PSRAM, and 16 MB
  flash.

## Related Documentation

- [json_format.md](json_format.md): device JSONL protocol.
- `$IDF_PATH/tools/ble/ble_uart_bridge/README.md`: BLE UART Bridge overview.
- `$IDF_PATH/tools/ble/ble_uart_bridge/demos/opencode/README.md`: OpenCode
  plugin-side guide.
