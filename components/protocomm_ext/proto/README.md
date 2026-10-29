# Protobuf files for implementing protocol communication packets

Protocomm_ext uses Google Protobuf for language, transport and architecture agnostic protocol communication. These proto files define the protocomm_ext packet structure, it should match the definition with the Protocomm in ESP-IDF, separated across multiple files:
* constants.proto - Defines the "Status" structure for conveying the success or failure of a single protocomm_ext transaction
* sec0.proto - Defines the Security0 Command and Response packet structures
* sec1.proto - Defines the Security1 Command and Response packet structures
* session.proto - Defines the protocomm_ext transaction packets for session establishment which internally has a Security packet as payload

Note : These proto files are not automatically compiled during the build process.

# Compilation

Compilation requires protoc (Protobuf Compiler) and protoc-c (Protobuf C Compiler) installed. Since the generated files are to remain the same, as long as the proto files are not modified, therefore the generated files are already available under `components/protocomm_ext/proto-c` and `components/protocomm_ext/python` directories, and thus running cmake / make (and installing the Protobuf compilers) is optional.

If using `cmake` follow the below steps. If using `make`, jump to Step 2 directly.

## Step 1 (Only for cmake)

When using cmake, first create a build directory and call cmake from inside:

```
mkdir build
cd build
cmake ..
```

## Step 2

Simply run `make` to generate the respective C and Python files. The newly created files will overwrite those under `components/protocomm_ext/proto-c` and `components/protocomm_ext/python`
