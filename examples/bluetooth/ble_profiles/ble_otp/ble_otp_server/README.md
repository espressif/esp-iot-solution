## BLE Object Transfer Profile (Server Example)

This example demonstrates a BLE Object Transfer Profile (OTP) **server demo**. It exposes the Object Transfer Service (OTS) and accepts L2CAP CoC connections for object data transfer.

### Virtual Object Store (Demo Only)

The server uses an **in-memory virtual object store** for demo and testing only. It does **not** use real files or persistent storage:

- **Object pool**: A small fixed number of objects (e.g. 3) with fixed names (`otp_object_0`, `otp_object_1`, `otp_object_2`).
- **Storage**: Object content is held in RAM buffers; it is lost on reboot.
- **Delete**: Implemented as a logical flag; the list skips deleted entries when navigating (OLCP First/Next/Last). Object Properties expose `delete_prop` so the client can delete.
- **OACP**: Read, Write, Append, Execute, Abort, and Delete are handled; the server responds with OACP Invalid Object (object not found) if the current object was already deleted.

In a real product you would back objects with files (e.g. flash/SD), a database, or other persistent storage. See the example source comments (e.g. "Virtual object store for demo/testing only") for the exact structures and how they map to OTS.

### Configuration

- `CONFIG_EXAMPLE_BLE_DEVICE_NAME`: Local BLE device name.
- `CONFIG_EXAMPLE_BLE_SUB_ADV`: Broadcast / advertising data.
- `CONFIG_EXAMPLE_OTP_PSM`: L2CAP CoC PSM for the Object Transfer Channel (must match the client).

### Usage

1. Build and flash this server example to one device (start it first so it advertises OTS).
2. Build and flash the **ble_otp_client** example to another device.
3. The client will scan for OTS UUID, connect, and run its 9-step demo (read, append, read, delete, etc.) against this server. Watch both logs for transfer progress.
