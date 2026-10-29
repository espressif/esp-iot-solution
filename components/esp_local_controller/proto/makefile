all: c_proto python_proto

# constants.proto is provided by protocomm_ext; do not regenerate it here.
c_proto: esp_local_ctrl.proto
	@protoc-c --c_out=../proto-c/ -I . -I ../../protocomm_ext/proto/ esp_local_ctrl.proto

python_proto: esp_local_ctrl.proto
	@protoc --python_out=../python/ -I . -I ../../protocomm_ext/proto/ esp_local_ctrl.proto
