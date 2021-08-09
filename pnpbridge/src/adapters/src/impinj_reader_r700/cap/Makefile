#
# Makefile for PnP Bridge application CAP for Impinj readers
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
PNPBRIDGE_FOLDER_SRC = ./../../../../../
PNP_BIN = $(PNPBRIDGE_FOLDER_SRC)/cmake/pnpbridge_linux/src/pnpbridge/samples/console/pnpbridge_bin
CONFIG_JSON = ../support_files/config.json
UPGRADE_SCRIPT = cust_app_upgrade
CAP_UPGX = $(BUILD_DIR)/$(CAP_NAME)
CAP_DIR	= $(BUILD_DIR)/cap
CAP_PNPBRIDGE_DIR = $(CAP_DIR)/azure_pnpbridge
CAP_SUPPORT_DIR = $(CAP_PNPBRIDGE_DIR)/support_files
CAP_DSC	= cap_description.in
SYS_DIR	= $(CAP_DIR)/sys
CFG_SRC	= reader.conf
CFG_DST	= $(SYS_DIR)/$(CFG_SRC)
START	= start
CONFIG_UPDATE = source/config_update
CONFIG_UPDATE_ARM = $(CONFIG_UPDATE)/build/config_update-arm

all: $(CAP_UPGX)

$(CAP_UPGX): $(PNP_BIN) $(CONFIG_JSON) $(CFG_DST) $(CAP_DSC) $(START) $(UPGRADE_SCRIPT) $(CONFIG_UPDATE_ARM) $(CAP_SUPPORT_DIR) | $(CAP_DIR) 
	$P '  Generating CAP file...'
	$E cp $(PNP_BIN) $(CAP_PNPBRIDGE_DIR)/
	$E cp $(CONFIG_JSON) $(CAP_PNPBRIDGE_DIR)/
	$E cp $(START) $(CAP_DIR)/
	$E cp $(UPGRADE_SCRIPT) $(CAP_DIR)/
	$E cp $(CONFIG_UPDATE_ARM) $(CAP_SUPPORT_DIR)/
	$E $(ETK_DIR)/cap_gen.sh -d $(CAP_DSC) -o $(CAP_UPGX)

$(CFG_DST): $(CFG_SRC) | $(SYS_DIR)
	$P '  CP		CONF'
	$E cp $(CFG_SRC) $@

$(CAP_SUPPORT_DIR) $(SYS_DIR):
	$E mkdir -p $@

$(CONFIG_UPDATE_ARM):
	$E cd $(CONFIG_UPDATE); make all

.PHONY: clean
clean:
	$P '  RM		CONFIG_UPDATE'
	$E cd $(CONFIG_UPDATE); make clean
	$P '  RM		TARGET'
	$E rm -rf $(TARGET)
	$P '  RM		BUILD_DIR'
	$E rm -rf $(BUILD_DIR)
	$P '  RM		CAP'
	$E rm -rf $(CAP)
	$P '  RM		OBJS'
	$E rm -rf $(OBJS)
