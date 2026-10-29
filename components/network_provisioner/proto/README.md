# Protobuf files for defining Network provisioning packet structures

`network_provisioning` uses Google Protobuf for language, transport and architecture agnostic protocol communication. These proto files define the protocomm packet structure, separated across multiple files:
* network_contants.proto - Defines the various enums for indicating state of network (connected / disconnect / connecting), disconnect reasons, auth modes, etc.
* network_config.proto - Defines network configuration structures and commands for setting Wi-Fi credentials (SSID, passphrase, BSSID) or Thread dataset, applying credentials and getting connection state.
* network_scan.proto - Defines network scan commands and result structures
* network_ctrl.proto - Defines network control commands(reset and re-provision) and result structures

Note : These proto files are not automatically compiled during the build process.

# Compilation

Compilation requires protoc (Protobuf Compiler) and protoc-c (Protobuf C Compiler) installed. Since the generated files are to remain the same, as long as the proto files are not modified, therefore the generated files are already available under `components/network_provisioning/proto-c` and `components/network_provisioning/python` directories, and thus running cmake / make (and installing the Protobuf compilers) is optional.

If using `cmake` follow the below steps. If using `make`, jump to Step 2 directly.

## Step 1 (Only for cmake)

When using cmake, first create a build directory and call cmake from inside:

```
mkdir build
cd build
cmake ..
```

## Step 2

Simply run `make` to generate the respective C and Python files. The newly created files will overwrite those under `components/network_provisioning/proto-c` and `components/network_provisioning/python`
