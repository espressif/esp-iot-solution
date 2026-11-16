CC ?= cc
PYTHON ?= python3
ROOT := ../..
BUILD := $(ROOT)/build
CORE := $(ROOT)
SOURCES := $(wildcard $(CORE)/src/core/*.c $(CORE)/src/container/eaf/*.c $(CORE)/src/container/eaf4/*.c $(CORE)/src/codec/*.c $(CORE)/src/codec/common/*.c $(CORE)/src/codec/eaf/*.c $(CORE)/src/codec/eaf4/*.c $(CORE)/src/port/host/*.c)
HEADERS := $(shell find $(CORE)/include $(CORE)/src -name '*.h')
CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror -pthread -I$(CORE)/include -I$(CORE)/src/core -I$(CORE)/src/codec -I$(CORE)/src/port
LDLIBS := -lz -ljpeg -pthread
.PHONY: test sanitize clean
$(BUILD):
	mkdir -p $@
$(BUILD)/libeaf_host.so: $(SOURCES) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $(SOURCES) $(LDLIBS)
$(BUILD)/test_host_core: test_core.c $(SOURCES) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_core.c $(SOURCES) $(LDLIBS)
$(BUILD)/eaf4_verify: eaf4_verify.c $(SOURCES) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ eaf4_verify.c $(SOURCES) $(LDLIBS)
$(BUILD)/test_host_sanitize: test_core.c $(SOURCES) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -o $@ test_core.c $(SOURCES) $(LDLIBS)
test: $(BUILD)/libeaf_host.so $(BUILD)/test_host_core $(BUILD)/eaf4_verify
	$(PYTHON) test_naming.py
	$(PYTHON) test_host_core.py
	$(PYTHON) test_regressions.py
	$(BUILD)/test_host_core $(BUILD)/host_fixtures/*.bin
sanitize: test $(BUILD)/test_host_sanitize
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(BUILD)/test_host_sanitize $(BUILD)/host_fixtures/*.bin
clean:
	rm -rf $(BUILD)
