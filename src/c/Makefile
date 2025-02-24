# Directories
SRC_DIR = $(CURDIR)/src
BUILD_DIR = $(CURDIR)/build
TEST_DIR = $(CURDIR)/tests
LUAJIT_DIR = $(CURDIR)/deps/luajit
CJSON_DIR = $(CURDIR)/deps/cJSON

# Commands and flags
CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -I$(LUAJIT_DIR)/src -I$(SRC_DIR) -I$(CJSON_DIR)
STATIC_LDFLAGS = -L$(BUILD_DIR)/lib -l:libluajit-5.1.a -ldl -lm

# LuaJIT static build with PIC
LUAJIT_MAKE = make -C $(LUAJIT_DIR) \
	BUILDMODE=static \
	PREFIX=$(BUILD_DIR) \
	INSTALL_BIN=$(BUILD_DIR)/bin \
	INSTALL_LIB=$(BUILD_DIR)/lib \
	INSTALL_SHARE=$(BUILD_DIR)/share \
	INSTALL_INC=$(BUILD_DIR)/include \
	CFLAGS="-fPIC" \
	TARGET_XCFLAGS="-DLUAJIT_NO_UNWIND -fPIC"

.PHONY: all clean test luajit cjson

all: luajit cjson $(BUILD_DIR)/lib/liblua_eval.a test

# Conditional LuaJIT build - only build if libluajit-5.1.a doesn't exist
$(BUILD_DIR)/lib/libluajit-5.1.a:
	mkdir -p $(BUILD_DIR)/bin $(BUILD_DIR)/lib $(BUILD_DIR)/include $(BUILD_DIR)/share
	$(LUAJIT_MAKE) clean
	$(LUAJIT_MAKE)
	$(LUAJIT_MAKE) install

# Conditional target for LuaJIT - make this dependent on the library file
luajit: $(BUILD_DIR)/lib/libluajit-5.1.a

# Conditional cJSON build
$(BUILD_DIR)/lib/libcjson.a: $(CJSON_DIR)/cJSON.c $(CJSON_DIR)/cJSON.h
	mkdir -p $(BUILD_DIR)/lib $(BUILD_DIR)/include
	$(CC) -c $(CFLAGS) -fPIC -o $(BUILD_DIR)/lib/cJSON.o $(CJSON_DIR)/cJSON.c
	$(AR) rcs $@ $(BUILD_DIR)/lib/cJSON.o
	cp $(CJSON_DIR)/cJSON.h $(BUILD_DIR)/include/
	rm $(BUILD_DIR)/lib/cJSON.o  # Cleanup

# Conditional target for cJSON
cjson: $(BUILD_DIR)/lib/libcjson.a

# Build the static library (combined with LuaJIT and cJSON)
$(BUILD_DIR)/lib/liblua_eval.a: $(SRC_DIR)/lua_eval.c $(SRC_DIR)/lua_eval.h $(BUILD_DIR)/lib/libluajit-5.1.a $(BUILD_DIR)/lib/libcjson.a
	mkdir -p $(BUILD_DIR)/lib $(BUILD_DIR)/include
	$(CC) -c $(CFLAGS) -o $(BUILD_DIR)/lib/lua_eval.o $(SRC_DIR)/lua_eval.c
	$(AR) x $(BUILD_DIR)/lib/libluajit-5.1.a  # Extract LuaJIT objects
	$(AR) x $(BUILD_DIR)/lib/libcjson.a      # Extract cJSON objects
	$(AR) rcs $@ $(BUILD_DIR)/lib/lua_eval.o *.o  # Combine all objects
	cp $(SRC_DIR)/lua_eval.h $(BUILD_DIR)/include/  # Copy header to include
	rm $(BUILD_DIR)/lib/lua_eval.o *.o  # Cleanup

# Build and run tests using the combined library
$(BUILD_DIR)/test_lua_eval: $(TEST_DIR)/test_lua_eval.c $(BUILD_DIR)/lib/liblua_eval.a
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -static -o $@ $(TEST_DIR)/test_lua_eval.c -L$(BUILD_DIR)/lib -l:liblua_eval.a -ldl -lm

test: $(BUILD_DIR)/test_lua_eval
	$(BUILD_DIR)/test_lua_eval

clean:
	rm -rf $(BUILD_DIR)
	$(LUAJIT_MAKE) clean
