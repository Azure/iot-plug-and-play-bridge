# Makefile for config_update utility

V ?= 0

ifeq ($V, 0)
    E = @
    P = @echo 
else
    E = 
    P = @true
endif

ETK_DIR ?= $(shell cat ./../../../support_files/ETKpath.txt)
TOOLCHAIN_DIR	= $(ETK_DIR)/arm-toolchain
export PATH	:= $(TOOLCHAIN_DIR)/bin/:$(PATH)
CROSS_COMPILE	?= arm-none-linux-gnueabihf-

BUILD_DIR = ./build
OBJ_DIR = $(BUILD_DIR)/objs
PNPBRIDGE_DIR = ./../../../../../../..
PARSON_DIR = $(PNPBRIDGE_DIR)/deps/azure-iot-sdk-c-pnp/deps/parson
PARSON = $(PARSON_DIR)/parson.c
CONFIG_UPDATE = config_update.cpp
CONFIG_UPDATE_ARM = $(BUILD_DIR)/config_update-arm
CONFIG_UPDATE_X64 = $(BUILD_DIR)/config_update-x64
PARSON_O_ARM = $(OBJ_DIR)/parson_arm.o
PARSON_O_X64 = $(OBJ_DIR)/parson_x64.o

CPP	:= g++
CC	:= gcc
CPP_ARM	:= $(CROSS_COMPILE)$(CPP)
CC_ARM	:= $(CROSS_COMPILE)$(CC)
CPPFLAGS	+= -g 
INCDIR	= -I$(TOOLCHAIN_DIR)/arm-buildroot-linux-gnueabihf/sysroot/usr/include/

all: $(CONFIG_UPDATE_X64) $(CONFIG_UPDATE_ARM)
	$P '  Building config_update...'

$(CONFIG_UPDATE_ARM): $(PARSON_O_ARM)
	$E $(CPP_ARM) -g $(CONFIG_UPDATE) $(PARSON_O_ARM) -o $(CONFIG_UPDATE_ARM)

$(PARSON_O_ARM): $(OBJ_DIR)
	$E $(CC_ARM) $(PARSON) -c -o $(PARSON_O_ARM)

$(CONFIG_UPDATE_X64): $(PARSON_O_X64)
	$E $(CPP) -g $(CONFIG_UPDATE) $(PARSON_O_X64) -o $(CONFIG_UPDATE_X64)

$(PARSON_O_X64): $(OBJ_DIR)
	$E $(CC) $(PARSON) -c -o $(PARSON_O_X64)

$(BUILD_DIR) $(OBJ_DIR):
	$E mkdir -p $@

.PHONY: clean
clean:
	$P '  RM		BUILD_DIR'
	$E rm -rf $(BUILD_DIR)
