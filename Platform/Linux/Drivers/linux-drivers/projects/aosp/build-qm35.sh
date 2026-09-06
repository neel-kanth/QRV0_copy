#!/usr/bin/env bash

[ ! -f ./tools/bazel ] && echo "This script shall be executed from aosp kernel root dir, abort" && exit 1

UWB_DTS="hdk-qm35"

SCRIPT_DIR="$(dirname $(realpath --relative-to=. $0))"
[ -z "${DRIVERS_DIR}" ] && \
    DRIVERS_DIR="$(git -C ${SCRIPT_DIR} rev-parse --show-toplevel)"
[ -z "${UWB_DRIVER}" ] && UWB_DRIVER="qm35"

# Create the full BUILD.bazel by appending the custom db845c-qm35 build entries
# to the default common/BUILD.bazel:
# Backup the original file

BUILD_FILE="$PWD/common/BUILD.bazel"
TARGET_NAME="db845c-qm35"
TMP_BUILD_FILE="$PWD/BUILD.bazel.tmp"
grep -q "$TARGET_NAME" "$BUILD_FILE" ||
    (cat $BUILD_FILE "$SCRIPT_DIR/BUILD.bazel" > "$TMP_BUILD_FILE" && mv "$TMP_BUILD_FILE" "$BUILD_FILE")

# Add missing symbol to db845c ABI

ABI="$PWD/common/android/abi_gki_aarch64_db845c"
for FUNC in ktime_add_safe; do
    grep -qxF "$FUNC" "$ABI" || echo "$FUNC" >> "$ABI"
done

echo "Building ${TARGET_NAME} with ${UWB_DTS} DTB"
TARGET_NAME="//common:${TARGET_NAME}_dist"

./tools/bazel run --lto=thin $TARGET_NAME
