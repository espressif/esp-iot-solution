| Supported Targets | ESP32-C3 | ESP32-C2 | ESP32-S3 | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- | -------- | -------- | -------- |

# BLE Periodic Sync Example

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example performs passive scan for non-connectable non-scannable extended advertisement, it then establishes the periodic sync with the advertiser and then listens to the periodic advertisements.

It uses Bluetooth controller based on BLE connection management.

This example aims at understanding BLE periodic sync establishment and periodic advertisement reports.

To test this demo, use any periodic advertiser with uses extended adv data as "ESP_PERIODIC_ADV".

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using:

```bash
idf.py set-target <chip_name>
```

### Hardware Required

* A development board with ESP32-C3/ESP32-C2/ESP32-S3 SoC
* A USB cable for Power supply and programming

See [Development Boards](https://www.espressif.com/en/products/devkits) for more information about it.

### Configure the Project

Open the project configuration menu: 

```bash
idf.py menuconfig
```

In the `Example Configuration` menu:

* Select advertisement name of device from `Example Configuration --> Advertisement name`, default is `ESP_PERIODIC_ADV`.

### Build and Flash

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://idf.espressif.com/) for full steps to configure and use ESP-IDF to build projects.

## Example Output

This is the console output on successful periodic sync:

```
I (318) BTDM_INIT: BT controller compile version [80abacd]
I (318) phy_init: phy_version 950,11a46e9,Oct 21 2022,08:56:12
I (368) system_api: Base MAC address is not set
I (368) system_api: read default base MAC address from EFUSE
I (368) BTDM_INIT: Bluetooth MAC: 84:f7:03:09:09:ca

I (378) blecm_nimble: BLE Host Task Started
I (578) blecm_nimble: BLE_GAP_EVENT_PERIODIC_SYNC
I (578) app_main: ESP_BLE_CONN_EVENT_PERIODIC_SYNC
I (578) app_main: status : 0, periodic_sync_handle : 1, sid : 2
I (578) app_main: adv addr : da:c0:3f:c6:52:0f
I (588) app_main: adv_phy : 2m
I (588) app_main: per_adv_ival : 240
I (598) app_main: adv_clk_accuracy : 0
I (598) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (608) app_main: sync_handle : 1
I (608) app_main: tx_power : 0
I (608) app_main: rssi : -31
I (618) app_main: data_status : 1
I (618) app_main: data_length : 241
I (628) app_main: data : 
I (628) app_main: de 04 e0 04 e2 04 e4 04 e6 04 e8 04 ea 04 ec 04 
I (638) app_main: ee 04 f0 04 f2 04 f4 04 f6 04 f8 04 fa 04 fc 04 
I (638) app_main: fe 05 01 05 03 05 05 05 07 05 09 05 0b 05 0d 05 
I (648) app_main: 0f 05 11 05 13 05 15 05 17 05 19 05 1b 05 1d 05 
I (658) app_main: 1f 05 21 05 23 05 25 05 27 05 29 05 2b 05 2d 05 
I (658) app_main: 2f 05 31 05 33 05 35 05 37 05 39 05 3b 05 3d 05 
I (668) app_main: 3f 05 41 05 43 05 45 05 47 05 49 05 4b 05 4d 05 
I (678) app_main: 4f 05 51 05 53 05 55 05 57 05 59 05 5b 05 5d 05 
I (678) app_main: 5f 05 61 05 63 05 65 05 67 05 69 05 6b 05 6d 05 
I (688) app_main: 6f 05 71 05 73 05 75 05 77 05 79 05 7b 05 7d 05 
I (698) app_main: 7f 05 81 05 83 05 85 05 87 05 89 05 8b 05 8d 05 
I (698) app_main: 8f 05 91 05 93 05 95 05 97 05 99 05 9b 05 9d 05 
I (708) app_main: 9f 05 a1 05 a3 05 a5 05 a7 05 a9 05 ab 05 ad 05 
I (718) app_main: af 05 b1 05 b3 05 b5 05 b7 05 b9 05 bb 05 bd 05 
I (718) app_main: bf 05 c1 05 c3 05 c5 05 c7 05 c9 05 cb 05 cd 05 
I (728) app_main: cf 
I (728) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (738) app_main: sync_handle : 1
I (738) app_main: tx_power : 0
I (748) app_main: rssi : -31
I (748) app_main: data_status : 1
I (758) app_main: data_length : 247
I (758) app_main: data : 
I (758) app_main: de 04 e0 04 e2 04 e4 04 e6 04 e8 04 ea 04 ec 04 
I (768) app_main: ee 04 f0 04 f2 04 f4 04 f6 04 f8 04 fa 04 fc 04 
I (778) app_main: fe 05 01 05 03 05 05 05 07 05 09 05 0b 05 0d 05 
I (778) app_main: 0f 05 11 05 13 05 15 05 17 05 19 05 1b 05 1d 05 
I (788) app_main: 1f 05 21 05 23 05 25 05 27 05 29 05 2b 05 2d 05 
I (798) app_main: 2f 05 31 05 33 05 35 05 37 05 39 05 3b 05 3d 05 
I (798) app_main: 3f 05 41 05 43 05 45 05 47 05 49 05 4b 05 4d 05 
I (808) app_main: 4f 05 51 05 53 05 55 05 57 05 59 05 5b 05 5d 05 
I (818) app_main: 5f 05 61 05 63 05 65 05 67 05 69 05 6b 05 6d 05 
I (818) app_main: 6f 05 71 05 73 05 75 05 77 05 79 05 7b 05 7d 05 
I (828) app_main: 7f 05 81 05 83 05 85 05 87 05 89 05 8b 05 8d 05 
I (838) app_main: 8f 05 91 05 93 05 95 05 97 05 99 05 9b 05 9d 05 
I (848) app_main: 9f 05 a1 05 a3 05 a5 05 a7 05 a9 05 ab 05 ad 05 
I (848) app_main: af 05 b1 05 b3 05 b5 05 b7 05 b9 05 bb 05 bd 05 
I (858) app_main: bf 05 c1 05 c3 05 c5 05 c7 05 c9 05 cb 05 cd 05 
I (868) app_main: cf 05 d1 05 d3 05 d5 
I (868) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (878) app_main: sync_handle : 1
I (878) app_main: tx_power : 0
I (878) app_main: rssi : -31
I (888) app_main: data_status : 1
I (888) app_main: data_length : 3
I (898) app_main: data : 
I (898) app_main: d9 05 db 
I (898) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (908) app_main: sync_handle : 1
I (908) app_main: tx_power : 0
I (918) app_main: rssi : -31
I (918) app_main: data_status : 1
I (918) app_main: data_length : 247
I (928) app_main: data : 
I (928) app_main: d9 05 db 05 dd 05 df 05 e1 05 e3 05 e5 05 e7 05 
I (938) app_main: e9 05 eb 05 ed 05 ef 05 f1 05 f3 05 f5 05 f7 05 
I (938) app_main: f9 05 fb 05 fd 06 00 06 02 06 04 06 06 06 08 06 
I (948) app_main: 0a 06 0c 06 0e 06 10 06 12 06 14 06 16 06 18 06 
I (958) app_main: 1a 06 1c 06 1e 06 20 06 22 06 24 06 26 06 28 06 
I (958) app_main: 2a 06 2c 06 2e 06 30 06 32 06 34 06 36 06 38 06 
I (968) app_main: 3a 06 3c 06 3e 06 40 06 42 06 44 06 46 06 48 06 
I (978) app_main: 4a 06 4c 06 4e 06 50 06 52 06 54 06 56 06 58 06 
I (988) app_main: 5a 06 5c 06 5e 06 60 06 62 06 64 06 66 06 68 06 
I (988) app_main: 6a 06 6c 06 6e 06 70 06 72 06 74 06 76 06 78 04 
I (998) app_main: 84 04 86 04 88 04 8a 04 8c 04 8e 04 90 04 92 04 
I (1008) app_main: 94 04 96 04 98 04 9a 04 9c 04 9e 04 a0 04 a2 04 
I (1008) app_main: a4 04 a6 04 a8 04 aa 04 ac 04 ae 04 b0 04 b2 04 
I (1018) app_main: b4 04 b6 04 b8 04 ba 04 bc 04 be 04 c0 04 c2 04 
I (1028) app_main: c4 04 c6 04 c8 04 ca 04 cc 04 ce 04 d0 04 d2 04 
I (1028) app_main: d4 04 d6 04 d8 04 da 
I (1038) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (1038) app_main: sync_handle : 1
I (1048) app_main: tx_power : 0
I (1048) app_main: rssi : -31
I (1058) app_main: data_status : 1
I (1058) app_main: data_length : 3
I (1058) app_main: data : 
I (1068) app_main: de 04 e0 
I (1068) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (1078) app_main: sync_handle : 1
I (1078) app_main: tx_power : 0
I (1078) app_main: rssi : -31
I (1088) app_main: data_status : 1
I (1088) app_main: data_length : 247
I (1098) app_main: data : 
I (1098) app_main: de 04 e0 04 e2 04 e4 04 e6 04 e8 04 ea 04 ec 04 
I (1108) app_main: ee 04 f0 04 f2 04 f4 04 f6 04 f8 04 fa 04 fc 04 
I (1108) app_main: fe 05 01 05 03 05 05 05 07 05 09 05 0b 05 0d 05 
I (1118) app_main: 0f 05 11 05 13 05 15 05 17 05 19 05 1b 05 1d 05 
I (1128) app_main: 1f 05 21 05 23 05 25 05 27 05 29 05 2b 05 2d 05 
I (1128) app_main: 2f 05 31 05 33 05 35 05 37 05 39 05 3b 05 3d 05 
I (1138) app_main: 3f 05 41 05 43 05 45 05 47 05 49 05 4b 05 4d 05 
I (1148) app_main: 4f 05 51 05 53 05 55 05 57 05 59 05 5b 05 5d 05 
I (1158) app_main: 5f 05 61 05 63 05 65 05 67 05 69 05 6b 05 6d 05 
I (1158) app_main: 6f 05 71 05 73 05 75 05 77 05 79 05 7b 05 7d 05 
I (1168) app_main: 7f 05 81 05 83 05 85 05 87 05 89 05 8b 05 8d 05 
I (1178) app_main: 8f 05 91 05 93 05 95 05 97 05 99 05 9b 05 9d 05 
I (1178) app_main: 9f 05 a1 05 a3 05 a5 05 a7 05 a9 05 ab 05 ad 05 
I (1188) app_main: af 05 b1 05 b3 05 b5 05 b7 05 b9 05 bb 05 bd 05 
I (1198) app_main: bf 05 c1 05 c3 05 c5 05 c7 05 c9 05 cb 05 cd 05 
I (1198) app_main: cf 05 d1 05 d3 05 d5 
I (1208) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (1208) app_main: sync_handle : 1
I (1218) app_main: tx_power : 0
I (1218) app_main: rssi : -31
I (1228) app_main: data_status : 1
I (1228) app_main: data_length : 3
I (1228) app_main: data : 
I (1238) app_main: d9 05 db 
I (1238) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (1248) app_main: sync_handle : 1
I (1248) app_main: tx_power : 0
I (1248) app_main: rssi : -31
I (1258) app_main: data_status : 1
I (1258) app_main: data_length : 247
I (1268) app_main: data : 
I (1268) app_main: d9 05 db 05 dd 05 df 05 e1 05 e3 05 e5 05 e7 05 
I (1278) app_main: e9 05 eb 05 ed 05 ef 05 f1 05 f3 05 f5 05 f7 05 
I (1278) app_main: f9 05 fb 05 fd 06 00 06 02 06 04 06 06 06 08 06 
I (1288) app_main: 0a 06 0c 06 0e 06 10 06 12 06 14 06 16 06 18 06 
I (1298) app_main: 1a 06 1c 06 1e 06 20 06 22 06 24 06 26 06 28 06 
I (1298) app_main: 2a 06 2c 06 2e 06 30 06 32 06 34 06 36 06 38 06 
I (1308) app_main: 3a 06 3c 06 3e 06 40 06 42 06 44 06 46 06 48 06 
I (1318) app_main: 4a 06 4c 06 4e 06 50 06 52 06 54 06 56 06 58 06 
I (1328) app_main: 5a 06 5c 06 5e 06 60 06 62 06 64 06 66 06 68 06 
I (1328) app_main: 6a 06 6c 06 6e 06 70 06 72 06 74 06 76 06 78 04 
I (1338) app_main: 84 04 86 04 88 04 8a 04 8c 04 8e 04 90 04 92 04 
I (1348) app_main: 94 04 96 04 98 04 9a 04 9c 04 9e 04 a0 04 a2 04 
I (1348) app_main: a4 04 a6 04 a8 04 aa 04 ac 04 ae 04 b0 04 b2 04 
I (1358) app_main: b4 04 b6 04 b8 04 ba 04 bc 04 be 04 c0 04 c2 04 
I (1368) app_main: c4 04 c6 04 c8 04 ca 04 cc 04 ce 04 d0 04 d2 04 
I (1368) app_main: d4 04 d6 04 d8 04 da 
I (1378) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (1378) app_main: sync_handle : 1
I (1388) app_main: tx_power : 0
I (1388) app_main: rssi : -31
I (1398) app_main: data_status : 1
I (1398) app_main: data_length : 3
I (1398) app_main: data : 
I (1408) app_main: de 04 e0 
I (1408) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (1418) app_main: sync_handle : 1
I (1418) app_main: tx_power : 0
I (1418) app_main: rssi : -31
I (1428) app_main: data_status : 1
I (1428) app_main: data_length : 247
I (1438) app_main: data : 
I (1438) app_main: de 04 e0 04 e2 04 e4 04 e6 04 e8 04 ea 04 ec 04 
I (1448) app_main: ee 04 f0 04 f2 04 f4 04 f6 04 f8 04 fa 04 fc 04 
I (1448) app_main: fe 05 01 05 03 05 05 05 07 05 09 05 0b 05 0d 05 
I (1458) app_main: 0f 05 11 05 13 05 15 05 17 05 19 05 1b 05 1d 05 
I (1468) app_main: 1f 05 21 05 23 05 25 05 27 05 29 05 2b 05 2d 05 
I (1468) app_main: 2f 05 31 05 33 05 35 05 37 05 39 05 3b 05 3d 05 
I (1478) app_main: 3f 05 41 05 43 05 45 05 47 05 49 05 4b 05 4d 05 
I (1488) app_main: 4f 05 51 05 53 05 55 05 57 05 59 05 5b 05 5d 05 
I (1498) app_main: 5f 05 61 05 63 05 65 05 67 05 69 05 6b 05 6d 05 
I (1498) app_main: 6f 05 71 05 73 05 75 05 77 05 79 05 7b 05 7d 05 
I (1508) app_main: 7f 05 81 05 83 05 85 05 87 05 89 05 8b 05 8d 05 
I (1518) app_main: 8f 05 91 05 93 05 95 05 97 05 99 05 9b 05 9d 05 
I (1518) app_main: 9f 05 a1 05 a3 05 a5 05 a7 05 a9 05 ab 05 ad 05 
I (1528) app_main: af 05 b1 05 b3 05 b5 05 b7 05 b9 05 bb 05 bd 05 
I (1538) app_main: bf 05 c1 05 c3 05 c5 05 c7 05 c9 05 cb 05 cd 05 
I (1538) app_main: cf 05 d1 05 d3 05 d5 
I (1548) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (1548) app_main: sync_handle : 1
I (1558) app_main: tx_power : 0
I (1558) app_main: rssi : -31
I (1568) app_main: data_status : 1
I (1568) app_main: data_length : 3
I (1568) app_main: data : 
I (1578) app_main: d9 05 db 
I (1578) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (1588) app_main: sync_handle : 1
I (1588) app_main: tx_power : 0
I (1588) app_main: rssi : -31
I (1598) app_main: data_status : 0
I (1598) app_main: data_length : 159
I (1608) app_main: data : 
I (1608) app_main: d9 05 db 05 dd 05 df 05 e1 05 e3 05 e5 05 e7 05 
I (1618) app_main: e9 05 eb 05 ed 05 ef 05 f1 05 f3 05 f5 05 f7 05 
I (1618) app_main: f9 05 fb 05 fd 06 00 06 02 06 04 06 06 06 08 06 
I (1628) app_main: 0a 06 0c 06 0e 06 10 06 12 06 14 06 16 06 18 06 
I (1638) app_main: 1a 06 1c 06 1e 06 20 06 22 06 24 06 26 06 28 06 
I (1638) app_main: 2a 06 2c 06 2e 06 30 06 32 06 34 06 36 06 38 06 
I (1648) app_main: 3a 06 3c 06 3e 06 40 06 42 06 44 06 46 06 48 06 
I (1658) app_main: 4a 06 4c 06 4e 06 50 06 52 06 54 06 56 06 58 06 
I (1668) app_main: 5a 06 5c 06 5e 06 60 06 62 06 64 06 66 06 68 06 
I (1668) app_main: 6a 06 6c 06 6e 06 70 06 72 06 74 06 76 06 78 
I (3878) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (3878) app_main: sync_handle : 1
I (3878) app_main: tx_power : 0
I (3878) app_main: rssi : -31
I (3878) app_main: data_status : 1
I (3888) app_main: data_length : 241
I (3888) app_main: data : 
I (3888) app_main: de 04 e0 04 e2 04 e4 04 e6 04 e8 04 ea 04 ec 04 
I (3898) app_main: ee 04 f0 04 f2 04 f4 04 f6 04 f8 04 fa 04 fc 04 
I (3908) app_main: fe 05 01 05 03 05 05 05 07 05 09 05 0b 05 0d 05 
I (3918) app_main: 0f 05 11 05 13 05 15 05 17 05 19 05 1b 05 1d 05 
I (3918) app_main: 1f 05 21 05 23 05 25 05 27 05 29 05 2b 05 2d 05 
I (3928) app_main: 2f 05 31 05 33 05 35 05 37 05 39 05 3b 05 3d 05 
I (3938) app_main: 3f 05 41 05 43 05 45 05 47 05 49 05 4b 05 4d 05 
I (3938) app_main: 4f 05 51 05 53 05 55 05 57 05 59 05 5b 05 5d 05 
I (3948) app_main: 5f 05 61 05 63 05 65 05 67 05 69 05 6b 05 6d 05 
I (3958) app_main: 6f 05 71 05 73 05 75 05 77 05 79 05 7b 05 7d 05 
I (3958) app_main: 7f 05 81 05 83 05 85 05 87 05 89 05 8b 05 8d 05 
I (3968) app_main: 8f 05 91 05 93 05 95 05 97 05 99 05 9b 05 9d 05 
I (3978) app_main: 9f 05 a1 05 a3 05 a5 05 a7 05 a9 05 ab 05 ad 05 
I (3978) app_main: af 05 b1 05 b3 05 b5 05 b7 05 b9 05 bb 05 bd 05 
I (3988) app_main: bf 05 c1 05 c3 05 c5 05 c7 05 c9 05 cb 05 cd 05 
I (3998) app_main: cf 
I (3998) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (4008) app_main: sync_handle : 1
I (4008) app_main: tx_power : 0
I (4008) app_main: rssi : -32
I (4018) app_main: data_status : 1
I (4018) app_main: data_length : 247
I (4028) app_main: data : 
I (4028) app_main: de 04 e0 04 e2 04 e4 04 e6 04 e8 04 ea 04 ec 04 
I (4038) app_main: ee 04 f0 04 f2 04 f4 04 f6 04 f8 04 fa 04 fc 04 
I (4038) app_main: fe 05 01 05 03 05 05 05 07 05 09 05 0b 05 0d 05 
I (4048) app_main: 0f 05 11 05 13 05 15 05 17 05 19 05 1b 05 1d 05 
I (4058) app_main: 1f 05 21 05 23 05 25 05 27 05 29 05 2b 05 2d 05 
I (4068) app_main: 2f 05 31 05 33 05 35 05 37 05 39 05 3b 05 3d 05 
I (4068) app_main: 3f 05 41 05 43 05 45 05 47 05 49 05 4b 05 4d 05 
I (4078) app_main: 4f 05 51 05 53 05 55 05 57 05 59 05 5b 05 5d 05 
I (4088) app_main: 5f 05 61 05 63 05 65 05 67 05 69 05 6b 05 6d 05 
I (4088) app_main: 6f 05 71 05 73 05 75 05 77 05 79 05 7b 05 7d 05 
I (4098) app_main: 7f 05 81 05 83 05 85 05 87 05 89 05 8b 05 8d 05 
I (4108) app_main: 8f 05 91 05 93 05 95 05 97 05 99 05 9b 05 9d 05 
I (4108) app_main: 9f 05 a1 05 a3 05 a5 05 a7 05 a9 05 ab 05 ad 05 
I (4118) app_main: af 05 b1 05 b3 05 b5 05 b7 05 b9 05 bb 05 bd 05 
I (4128) app_main: bf 05 c1 05 c3 05 c5 05 c7 05 c9 05 cb 05 cd 05 
I (4128) app_main: cf 05 d1 05 d3 05 d5 
I (4138) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (4138) app_main: sync_handle : 1
I (4148) app_main: tx_power : 0
I (4148) app_main: rssi : -32
I (4158) app_main: data_status : 1
I (4158) app_main: data_length : 3
I (4158) app_main: data : 
I (4168) app_main: d9 05 db 
I (4168) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (4178) app_main: sync_handle : 1
I (4178) app_main: tx_power : 0
I (4178) app_main: rssi : -32
I (4188) app_main: data_status : 1
I (4188) app_main: data_length : 247
I (4198) app_main: data : 
I (4198) app_main: d9 05 db 05 dd 05 df 05 e1 05 e3 05 e5 05 e7 05 
I (4208) app_main: e9 05 eb 05 ed 05 ef 05 f1 05 f3 05 f5 05 f7 05 
I (4208) app_main: f9 05 fb 05 fd 06 00 06 02 06 04 06 06 06 08 06 
I (4218) app_main: 0a 06 0c 06 0e 06 10 06 12 06 14 06 16 06 18 06 
I (4228) app_main: 1a 06 1c 06 1e 06 20 06 22 06 24 06 26 06 28 06 
I (4238) app_main: 2a 06 2c 06 2e 06 30 06 32 06 34 06 36 06 38 06 
I (4238) app_main: 3a 06 3c 06 3e 06 40 06 42 06 44 06 46 06 48 06 
I (4248) app_main: 4a 06 4c 06 4e 06 50 06 52 06 54 06 56 06 58 06 
I (4258) app_main: 5a 06 5c 06 5e 06 60 06 62 06 64 06 66 06 68 06 
I (4258) app_main: 6a 06 6c 06 6e 06 70 06 72 06 74 06 76 06 78 04 
I (4268) app_main: 84 04 86 04 88 04 8a 04 8c 04 8e 04 90 04 92 04 
I (4278) app_main: 94 04 96 04 98 04 9a 04 9c 04 9e 04 a0 04 a2 04 
I (4278) app_main: a4 04 a6 04 a8 04 aa 04 ac 04 ae 04 b0 04 b2 04 
I (4288) app_main: b4 04 b6 04 b8 04 ba 04 bc 04 be 04 c0 04 c2 04 
I (4298) app_main: c4 04 c6 04 c8 04 ca 04 cc 04 ce 04 d0 04 d2 04 
I (4298) app_main: d4 04 d6 04 d8 04 da 
I (4308) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (4308) app_main: sync_handle : 1
I (4318) app_main: tx_power : 0
I (4318) app_main: rssi : -32
I (4328) app_main: data_status : 1
I (4328) app_main: data_length : 3
I (4328) app_main: data : 
I (4338) app_main: de 04 e0 
I (4338) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (4348) app_main: sync_handle : 1
I (4348) app_main: tx_power : 0
I (4358) app_main: rssi : -32
I (4358) app_main: data_status : 1
I (4358) app_main: data_length : 247
I (4368) app_main: data : 
I (4368) app_main: de 04 e0 04 e2 04 e4 04 e6 04 e8 04 ea 04 ec 04 
I (4378) app_main: ee 04 f0 04 f2 04 f4 04 f6 04 f8 04 fa 04 fc 04 
I (4378) app_main: fe 05 01 05 03 05 05 05 07 05 09 05 0b 05 0d 05 
I (4388) app_main: 0f 05 11 05 13 05 15 05 17 05 19 05 1b 05 1d 05 
I (4398) app_main: 1f 05 21 05 23 05 25 05 27 05 29 05 2b 05 2d 05 
I (4408) app_main: 2f 05 31 05 33 05 35 05 37 05 39 05 3b 05 3d 05 
I (4408) app_main: 3f 05 41 05 43 05 45 05 47 05 49 05 4b 05 4d 05 
I (4418) app_main: 4f 05 51 05 53 05 55 05 57 05 59 05 5b 05 5d 05 
I (4428) app_main: 5f 05 61 05 63 05 65 05 67 05 69 05 6b 05 6d 05 
I (4428) app_main: 6f 05 71 05 73 05 75 05 77 05 79 05 7b 05 7d 05 
I (4438) app_main: 7f 05 81 05 83 05 85 05 87 05 89 05 8b 05 8d 05 
I (4448) app_main: 8f 05 91 05 93 05 95 05 97 05 99 05 9b 05 9d 05 
I (4448) app_main: 9f 05 a1 05 a3 05 a5 05 a7 05 a9 05 ab 05 ad 05 
I (4458) app_main: af 05 b1 05 b3 05 b5 05 b7 05 b9 05 bb 05 bd 05 
I (4468) app_main: bf 05 c1 05 c3 05 c5 05 c7 05 c9 05 cb 05 cd 05 
I (4468) app_main: cf 05 d1 05 d3 05 d5 
I (4478) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (4488) app_main: sync_handle : 1
I (4488) app_main: tx_power : 0
I (4488) app_main: rssi : -32
I (4498) app_main: data_status : 1
I (4498) app_main: data_length : 3
I (4498) app_main: data : 
I (4508) app_main: d9 05 db 
I (4508) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (4518) app_main: sync_handle : 1
I (4518) app_main: tx_power : 0
I (4528) app_main: rssi : -32
I (4528) app_main: data_status : 1
I (4528) app_main: data_length : 247
I (4538) app_main: data : 
I (4538) app_main: d9 05 db 05 dd 05 df 05 e1 05 e3 05 e5 05 e7 05 
I (4548) app_main: e9 05 eb 05 ed 05 ef 05 f1 05 f3 05 f5 05 f7 05 
I (4548) app_main: f9 05 fb 05 fd 06 00 06 02 06 04 06 06 06 08 06 
I (4558) app_main: 0a 06 0c 06 0e 06 10 06 12 06 14 06 16 06 18 06 
I (4568) app_main: 1a 06 1c 06 1e 06 20 06 22 06 24 06 26 06 28 06 
I (4578) app_main: 2a 06 2c 06 2e 06 30 06 32 06 34 06 36 06 38 06 
I (4578) app_main: 3a 06 3c 06 3e 06 40 06 42 06 44 06 46 06 48 06 
I (4588) app_main: 4a 06 4c 06 4e 06 50 06 52 06 54 06 56 06 58 06 
I (4598) app_main: 5a 06 5c 06 5e 06 60 06 62 06 64 06 66 06 68 06 
I (4598) app_main: 6a 06 6c 06 6e 06 70 06 72 06 74 06 76 06 78 04 
I (4608) app_main: 84 04 86 04 88 04 8a 04 8c 04 8e 04 90 04 92 04 
I (4618) app_main: 94 04 96 04 98 04 9a 04 9c 04 9e 04 a0 04 a2 04 
I (4618) app_main: a4 04 a6 04 a8 04 aa 04 ac 04 ae 04 b0 04 b2 04 
I (4628) app_main: b4 04 b6 04 b8 04 ba 04 bc 04 be 04 c0 04 c2 04 
I (4638) app_main: c4 04 c6 04 c8 04 ca 04 cc 04 ce 04 d0 04 d2 04 
I (4638) app_main: d4 04 d6 04 d8 04 da 
I (4648) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (4658) app_main: sync_handle : 1
I (4658) app_main: tx_power : 0
I (4658) app_main: rssi : -32
I (4668) app_main: data_status : 1
I (4668) app_main: data_length : 3
I (4668) app_main: data : 
I (4678) app_main: de 04 e0 
I (4678) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (4688) app_main: sync_handle : 1
I (4688) app_main: tx_power : 0
I (4698) app_main: rssi : -32
I (4698) app_main: data_status : 1
I (4698) app_main: data_length : 247
I (4708) app_main: data : 
I (4708) app_main: de 04 e0 04 e2 04 e4 04 e6 04 e8 04 ea 04 ec 04 
I (4718) app_main: ee 04 f0 04 f2 04 f4 04 f6 04 f8 04 fa 04 fc 04 
I (4718) app_main: fe 05 01 05 03 05 05 05 07 05 09 05 0b 05 0d 05 
I (4728) app_main: 0f 05 11 05 13 05 15 05 17 05 19 05 1b 05 1d 05 
I (4738) app_main: 1f 05 21 05 23 05 25 05 27 05 29 05 2b 05 2d 05 
I (4748) app_main: 2f 05 31 05 33 05 35 05 37 05 39 05 3b 05 3d 05 
I (4748) app_main: 3f 05 41 05 43 05 45 05 47 05 49 05 4b 05 4d 05 
I (4758) app_main: 4f 05 51 05 53 05 55 05 57 05 59 05 5b 05 5d 05 
I (4768) app_main: 5f 05 61 05 63 05 65 05 67 05 69 05 6b 05 6d 05 
I (4768) app_main: 6f 05 71 05 73 05 75 05 77 05 79 05 7b 05 7d 05 
I (4778) app_main: 7f 05 81 05 83 05 85 05 87 05 89 05 8b 05 8d 05 
I (4788) app_main: 8f 05 91 05 93 05 95 05 97 05 99 05 9b 05 9d 05 
I (4788) app_main: 9f 05 a1 05 a3 05 a5 05 a7 05 a9 05 ab 05 ad 05 
I (4798) app_main: af 05 b1 05 b3 05 b5 05 b7 05 b9 05 bb 05 bd 05 
I (4808) app_main: bf 05 c1 05 c3 05 c5 05 c7 05 c9 05 cb 05 cd 05 
I (4808) app_main: cf 05 d1 05 d3 05 d5 
I (4818) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (4828) app_main: sync_handle : 1
I (4828) app_main: tx_power : 0
I (4828) app_main: rssi : -32
I (4838) app_main: data_status : 1
I (4838) app_main: data_length : 3
I (4838) app_main: data : 
I (4848) app_main: d9 05 db 
I (4848) app_main: ESP_BLE_CONN_EVENT_PERIODIC_REPORT
I (4858) app_main: sync_handle : 1
I (4858) app_main: tx_power : 0
I (4868) app_main: rssi : -32
I (4868) app_main: data_status : 0
I (4868) app_main: data_length : 159
I (4878) app_main: data : 
I (4878) app_main: d9 05 db 05 dd 05 df 05 e1 05 e3 05 e5 05 e7 05 
I (4888) app_main: e9 05 eb 05 ed 05 ef 05 f1 05 f3 05 f5 05 f7 05 
I (4888) app_main: f9 05 fb 05 fd 06 00 06 02 06 04 06 06 06 08 06 
I (4898) app_main: 0a 06 0c 06 0e 06 10 06 12 06 14 06 16 06 18 06 
I (4908) app_main: 1a 06 1c 06 1e 06 20 06 22 06 24 06 26 06 28 06 
I (4918) app_main: 2a 06 2c 06 2e 06 30 06 32 06 34 06 36 06 38 06 
I (4918) app_main: 3a 06 3c 06 3e 06 40 06 42 06 44 06 46 06 48 06 
I (4928) app_main: 4a 06 4c 06 4e 06 50 06 52 06 54 06 56 06 58 06 
I (4938) app_main: 5a 06 5c 06 5e 06 60 06 62 06 64 06 66 06 68 06 
I (4938) app_main: 6a 06 6c 06 6e 06 70 06 72 06 74 06 76 06 78
```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.
