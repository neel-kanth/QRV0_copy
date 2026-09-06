#!/usr/bin/env bash

# Sync and build Android T kernel for DragonBoard, like on CI
#
# Prerequisites:
# - The AOSP download environment, see https://source.android.com/setup/build/downloading
#
# Usage from working directory: ./build-qm_uwb.sh

[ ! -f ./build/build.sh ] && echo "This script shall be executed from AOSP kernel root dir. Abort!" && exit 1

set -ex

SCRIPT_DIR="$(dirname $(realpath --relative-to=. $0))"
[ -z "${DRIVERS_DIR}" ] && \
    DRIVERS_DIR="$(git -C ${SCRIPT_DIR} rev-parse --show-toplevel)"

[ -z "${UWB_DRIVER}" ] && UWB_DRIVER="qm35"
if [ "${UWB_DRIVER}" = "qm35" ]; then
    CONFIG_MODULES=(CONFIG_DW3000=n
                    CONFIG_QCOM_SPI_LOWLATENCY=m
                    CONFIG_QM35=m)
else
    echo "This script can build only QM35 driver. Abort!" && exit 2
fi

export KERNEL_DIR="common"
# Link to config automatically added by repo manifest.
export BUILD_CONFIG="common/build.config.db845c-qm35"
export UWB_FRAGMENT_CONFIG="${SCRIPT_DIR}/db845c-uwb.fragment"
export EXT_MODULES="$(realpath --relative-to=. ${DRIVERS_DIR}/kernel)"
export LTO="thin"

./build/build.sh ${CONFIG_MODULES[*]} $@
