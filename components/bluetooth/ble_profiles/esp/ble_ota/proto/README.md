 # Protobuf files for defining ota packet structures

`ble_ota` uses Google Protobuf for language, transport and architecture agnostic protocol communication. These proto files define the protocomm packet structure, separated across multiple fies:
* ota_send_file.proto - Defines OTA start, subscribe and send file commands and result structures.
Note : These proto files are not automatically compiled during the build process.

 # Compilation
 
Compilation requires protoc (Protobuf Compiler) and protoc-c (Protobuf C Compiler) installed. Since the generated files are to remain the same, as long as the proto files are not modified, therefore the generated files are already available under `components/ble_ota/proto-c` and `components/ble_ota/python` directories, and thus running cmake / make (and installing the Protobuf compilers) is optional.
 
 If using `cmake` follow the below steps. If using `make`, jump to Step 2 directly.
 
 ## Step 1 (Only for cmake)
 
 When using cmake, first create a build directory and call cmake from inside:
 
 ```
 mkdir build
 cd build
 cmake ..
 ```
 
 ## Step 2
 
 Simply run `make` to generate the respective C and Python files. The newly created files will overwrite those under `components/ble_ota/proto-c` and `components/ble_ota/python`
