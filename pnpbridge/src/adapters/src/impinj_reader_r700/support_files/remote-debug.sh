#!/bin/bash -eux

# NOTE : This script is meant to be used from within the docker container

fail() {
    echo "$1"
    exit 1
}

[ "$#" -eq 1 ] || fail "Usage: ./remote-debug.sh <hostname>:<port>"

OUT_DIR="/home/kwenner/iothub/r700/iot-plug-and-play-bridge/pnpbridge/cmake/pnpbridge_linux"
[ -d "${OUT_DIR}" ] || fail "Unable to find build directory"

TOOLCHAIN_DIR="/home/kwenner/arm-toolchains/r700/7.3.0_Octane_Embedded_Development_Tools/arm-toolchain"
[ -d "${TOOLCHAIN_DIR}" ] || fail "Unable to find toolchain directory"

# GDB="/${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-gdb"
# [ -e "$GDB" ] || fail "Unable to find Impinj ETK gdb binary"

SYSROOT_DIR="${TOOLCHAIN_DIR}/arm-buildroot-linux-gnueabihf/sysroot"
APP_BIN="${OUT_DIR}/src/adapters/samples/environmental_sensor/pnpbridge_environmentalsensor"
INIT_SYSROOT="set sysroot ${SYSROOT_DIR}"
INIT_SO="set solib-search-path ${SYSROOT_DIR}/usr/lib/:/home/kwenner/arm-toolchains/r700/7.3.0_Octane_Embedded_Development_Tools/arm-toolchain/lib"
INIT_CONNECT="target extended-remote $1"
CD_OUT="cd ${OUT_DIR}"
args=( -ex "${INIT_SYSROOT}" -ex "${INIT_SO}" -ex "${INIT_CONNECT}" -ex "${CD_OUT}" ${APP_BIN})
gdb-multiarch "${args[@]}"