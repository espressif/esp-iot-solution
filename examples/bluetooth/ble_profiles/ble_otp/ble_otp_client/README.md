## BLE Object Transfer Profile (Client Example)

This example demonstrates a BLE Object Transfer Profile (OTP) **client demo**. It scans for a peer advertising the Object Transfer Service (OTS) UUID, connects, then runs a **fixed 9-step flow** against the OTP server (e.g. `ble_otp_server`):

1. **Step 1–2**  
   Discover OTS (auto on connect), read OTS Feature, then select the first object (OLCP First).

2. **Step 3**  
   Read object info (name/size), then read the full object content over L2CAP CoC and print it.

3. **Step 4–5**  
   Append a string to the object (OACP Write Append), then full read again and print.

4. **Step 6–7**  
   Append a second string, then full read again and print.

5. **Step 8**  
   Delete the current object (OACP Delete). The server must expose `delete_prop` for the object.

6. **Step 9**  
   Select first again (OLCP First). If the server has no objects left (e.g. all deleted), the demo stops; otherwise it reads object info and performs one more full read.

Flow control is implemented with a **demo flow task** (`otp_demo_flow_task`) and a message queue (`otp_demo_*`). The OTP event handler turns BLE_OTP_EVENTS (OTS discovered, OLCP/OACP responses, transfer complete/error) into queue messages so the task can advance step-by-step.

### Configuration

- `CONFIG_EXAMPLE_BLE_DEVICE_NAME`: Local BLE device name.
- `CONFIG_EXAMPLE_BLE_SUB_ADV`: Broadcast / advertising data.
- `CONFIG_EXAMPLE_OTP_PSM`: L2CAP CoC PSM for the Object Transfer Channel (must match the server).

### Usage

1. Build and flash this client example to one device.
2. Build and flash the **ble_otp_server** example to another device (or run the server first so it advertises OTS).
3. Power on both; the client scans for OTS UUID, connects, then runs the 9-step demo. Watch the client logs for step progress and received object content.
