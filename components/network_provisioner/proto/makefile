all: c_proto python_proto

c_proto: *.proto
	@protoc-c --c_out=../proto-c/ -I . -I ${IDF_PATH}/components/protocomm/proto/ *.proto

python_proto: *.proto
	@protoc --python_out=../python/ -I . -I ${IDF_PATH}/components/protocomm/proto/ *.proto
