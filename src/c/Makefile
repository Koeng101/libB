# Directories
SRC_DIR = $(CURDIR)/src
BUILD_DIR = $(CURDIR)/build
TEST_DIR = $(CURDIR)/tests
LUAJIT_DIR = $(CURDIR)/deps/luajit

# Commands and flags
CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -I$(LUAJIT_DIR)/src -I$(SRC_DIR)
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

.PHONY: all clean test luajit

all: luajit $(BUILD_DIR)/server $(BUILD_DIR)/lib/liblua_eval.a test

# Build static LuaJIT
luajit:
	mkdir -p $(BUILD_DIR)/bin $(BUILD_DIR)/lib $(BUILD_DIR)/include $(BUILD_DIR)/share
	$(LUAJIT_MAKE) clean
	$(LUAJIT_MAKE)
	$(LUAJIT_MAKE) install

# Build the static library (combined with LuaJIT)
$(BUILD_DIR)/lib/liblua_eval.a: $(SRC_DIR)/lua_eval.c $(SRC_DIR)/lua_eval.h luajit
	mkdir -p $(BUILD_DIR)/lib $(BUILD_DIR)/include
	$(CC) -c $(CFLAGS) -o $(BUILD_DIR)/lib/lua_eval.o $(SRC_DIR)/lua_eval.c
	$(AR) x $(BUILD_DIR)/lib/libluajit-5.1.a  # Extract LuaJIT objects
	$(AR) rcs $@ $(BUILD_DIR)/lib/lua_eval.o *.o  # Combine all objects
	cp $(SRC_DIR)/lua_eval.h $(BUILD_DIR)/include/  # Copy header to include
	rm $(BUILD_DIR)/lib/lua_eval.o *.o  # Cleanup

# Build the static executable
$(BUILD_DIR)/server: $(SRC_DIR)/main.c $(SRC_DIR)/lua_eval.c $(SRC_DIR)/lua_eval.h luajit
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -static -o $@ $(SRC_DIR)/main.c $(SRC_DIR)/lua_eval.c $(STATIC_LDFLAGS)

# Build and run tests using the combined library
$(BUILD_DIR)/test_lua_eval: $(TEST_DIR)/test_lua_eval.c $(BUILD_DIR)/lib/liblua_eval.a
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -static -o $@ $(TEST_DIR)/test_lua_eval.c -L$(BUILD_DIR)/lib -l:liblua_eval.a -ldl -lm

test: $(BUILD_DIR)/test_lua_eval
	$(BUILD_DIR)/test_lua_eval

clean:
	rm -rf $(BUILD_DIR)
	$(LUAJIT_MAKE) clean
