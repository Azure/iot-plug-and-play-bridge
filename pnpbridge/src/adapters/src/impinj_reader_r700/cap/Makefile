#
# Example 'Hello, World' CAP
#

V ?= 0

ifeq ($V, 0)
    E = @
    P = @echo 
else
    E = 
    P = @true
endif

ETK_DIR ?= $(shell cat ./../support_files/ETKpath.txt)
TOOLCHAIN_DIR	= $(ETK_DIR)/arm-toolchain
export PATH	:= $(TOOLCHAIN_DIR)/bin/:$(PATH)
CROSS_COMPILE	?= arm-none-linux-gnueabihf-

CAP_NAME = azure_pnpbridge_impinj_r700.upgx
BUILD_DIR = build
PNPBRIDGE_FOLDER = ./../../../../../
PNP_BIN = $(PNPBRIDGE_FOLDER)/cmake/pnpbridge_linux/src/pnpbridge/samples/console/pnpbridge_bin
CONFIG_JSON = ../support_files/config.json
CAP_UPGX = $(BUILD_DIR)/$(CAP_NAME)
CAP_DIR	= $(BUILD_DIR)/cap
CAP_DSC	= cap_description.in
SYS_DIR	= $(CAP_DIR)/sys
CFG_SRC	= reader.conf
CFG_DST	= $(SYS_DIR)/$(CFG_SRC)
START	= start
# CXX	:= $(CROSS_COMPILE)g++
# CFLAGS	+= -g -Wall -Wextra
# LDFLAGS	+= -static \
# 	   -l:libltkcpp.a \
# 	   -l:libltkcppimpinj.a \
# 	   -l:libssl.a \
# 	   -l:libcrypto.a \
# 	   -l:libpthread.a \
# 	   -l:libdl.a
# OBJS	= $(BUILD_DIR)/main.o
# INCDIR	= -I$(TOOLCHAIN_DIR)/arm-buildroot-linux-gnueabihf/sysroot/usr/include/

all: $(CAP_UPGX)

$(CAP_UPGX): $(PNP_BIN) $(CONFIG_JSON) $(CFG_DST) $(CAP_DSC) $(START) | $(CAP_DIR)
	$P '  GEN		CAP'
	$E cp $(PNP_BIN) $(CAP_DIR)/
	$E cp $(CONFIG_JSON) $(CAP_DIR)/
	$E cp $(START) $(CAP_DIR)/
	$E $(ETK_DIR)/cap_gen.sh -d $(CAP_DSC) -o $(CAP_UPGX)

$(CFG_DST): $(CFG_SRC) | $(SYS_DIR)
	$P '  CP		CONF'
	$E cp $(CFG_SRC) $@

$(CAP_DIR) $(SYS_DIR):
	$E mkdir -p $@

# $(TARGET): $(OBJS)
# 	$P '  LD		TARGET'
# 	$E $(CXX) -o $@ $^ $(LDFLAGS)

# $(BUILD_DIR)/%.o: %.cpp $(CAP_DIR)
# 	$P '  CXX 		$@'
# 	$E $(CXX) -c -o $@ $^ $(INCDIR) $(CFLAGS)

.PHONY: clean
clean:
	$P '  RM		TARGET'
	$E rm -rf $(TARGET)
	$P '  RM		CAP_DIR'
	$E rm -rf $(BUILD_DIR)
	$P '  RM		CAP'
	$E rm -rf $(CAP)
	$P '  RM		OBJS'
	$E rm -rf $(OBJS)
