| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- |

# ESP-IDF BLE Remote Control Example

This example shows how to create a BLE Joystick with an ESP32 series SoC implemented in BLUEDROID. 

Please, check this [tutorial](tutorial/controller_walkthrough.md) for more information about this example.

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using:

```bash
idf.py set-target <chip_name>
```

### Hardware Required

* A development board with ESP32/ESP32-C3/ESP32-H2/ESP32-C2/ESP32-S3 SoC (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.)
* A USB cable for Power supply and programming

See [Development Boards](https://www.espressif.com/en/products/devkits) for more information about it.

### Configure the Project

```bash
idf.py menuconfig
```

* Pick the setup (simple setup or using external hardware) under the `Example Configuration` option

### Build and Flash

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://idf.espressif.com/) for full steps to configure and use ESP-IDF to build projects.

## Example Output
> Assuming default configuration of boot button and console joystick input
```
I (586) HID_RC_GAP_GATTS: GATTS to be handled by profile, event 22
I (596) HID_RC: Create attribute table successfully, table size = 12
I (596) HID_RC: Service Instance ID: 0, UUID: 0x1812
I (606) HID_RC: Attribute Table Length: 12
I (616) HID_RC: Create attribute table successfully, the number handle = 12

I (616) HID_RC_GAP_GATTS: ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT
I (626) HID_RC_GAP_GATTS: GATTS to be handled by profile, event 12
I (636) HID_RC: Start Service Successful, service_handle 40
I (636) HID_RC_GAP_GATTS: GATTS to be handled by profile, event 12
I (646) HID_RC: Start Service Successful, service_handle 43
I (656) HID_RC_GAP_GATTS: GATTS to be handled by profile, event 12
I (656) HID_RC: Start Service Successful, service_handle 48
I (666) HID_RC_GAP_GATTS: GAP Start Advertising Success

I (666) CONTROLLER: Console input usage:
I (676) CONTROLLER: 	 Top Left(q) 		 Top Center(w) 			 Top Right(e)
I (686) CONTROLLER: 	 Middle Left(a) 	 Middle Center(s) 	 Middle Right(d)
I (686) CONTROLLER: 	 Bottom Left(z) 	 Bottom Center(x) 	 Bottom Right(c)
I (696) CONTROLLER: Press Enter to send input, make sure caps lock is off

I (60716) HID_REMOTE_CONTROL: Not connected, do not send user input yet
I (63076) HID_RC: Profile Receiving a new connection, conn_id 0
I (63476) HID_RC_GAP_GATTS: Update connection params status = 0, min_int = 0, max_int = 0,conn_int = 6,latency = 0, timeout = 500
I (63976) HID_RC_GAP_GATTS: Update connection params status = 0, min_int = 0, max_int = 0,conn_int = 40,latency = 0, timeout = 500
I (68626) HID_REMOTE_CONTROL: Console input received
I (68626) HID_REMOTE_CONTROL:  
I (68626) HID_REMOTE_CONTROL: ----- Sending user input -----
I (68626) HID_REMOTE_CONTROL: X: 0x7f (127), Y: 0x0 (0), SW: 0, B: 0, Thr: 0
I (68636) HID_REMOTE_CONTROL:  ----- End of user input ----- 

I (68636) HID_RC: Send notif, params: gatt_if: 0x3, conn_id: 0x0, attr_handle: 0x39, len: 4, values 7f:0:0:0
I (68656) HID_RC_GAP_GATTS: GATTS to be handled by profile, event 5
I (68656) HID_RC: Receive Confirm Event, status = 0, attr_handle 57

```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub. We will get back to you soon.
