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

ETK_DIR		?= $(abspath $(lastword $(MAKEFILE_LIST))/../../../)
TOOLCHAIN_DIR	= $(ETK_DIR)/arm-toolchain
export PATH	:= $(TOOLCHAIN_DIR)/bin/:$(PATH)
CROSS_COMPILE	?= arm-none-linux-gnueabihf-

CAP	= cap.upgx
CAP_DIR	= cap
CAP_DSC	= cap_description.in
SYS_DIR	= $(CAP_DIR)/sys
CFG_SRC	= reader.conf
CFG_DST	= $(SYS_DIR)/$(CFG_SRC)
START	= start
CXX	:= $(CROSS_COMPILE)g++
CFLAGS	+= -g -Wall -Wextra
LDFLAGS	+= -static \
	   -l:libltkcpp.a \
	   -l:libltkcppimpinj.a \
	   -l:libssl.a \
	   -l:libcrypto.a \
	   -l:libpthread.a \
	   -l:libdl.a
OBJS	= main.o
TARGET	= example
INCDIR	= -I$(TOOLCHAIN_DIR)/arm-buildroot-linux-gnueabihf/sysroot/usr/include/

all: $(CAP)

$(CAP): $(TARGET) $(CFG_DST) $(CAP_DSC) $(START) | $(CAP_DIR)
	$P '  GEN		CAP'
	$E cp $(TARGET) $(CAP_DIR)/
	$E cp $(START) $(CAP_DIR)/
	$E $(ETK_DIR)/cap_gen.sh -d $(CAP_DSC) -o $(CAP)

$(CFG_DST): $(CFG_SRC) | $(SYS_DIR)
	$P '  CP		CONF'
	$E cp $(CFG_SRC) $@

$(CAP_DIR) $(SYS_DIR):
	$E mkdir -p $@

$(TARGET): $(OBJS)
	$P '  LD		TARGET'
	$E $(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$P '  CXX 		$@'
	$E $(CXX) -c -o $@ $^ $(INCDIR) $(CFLAGS)

.PHONY: clean
clean:
	$P '  RM		TARGET'
	$E rm -rf $(TARGET)
	$P '  RM		CAP_DIR'
	$E rm -rf $(CAP_DIR)
	$P '  RM		CAP'
	$E rm -rf $(CAP)
	$P '  RM		OBJS'
	$E rm -rf $(OBJS)
