# UWB Linux drivers AOSP script

## Introduction

This project includes required files to build drivers for AOSP.

First, setup AOSP build environment, then launch the build script.

## Setup environment

To build drivers for AOSP kernel, you must first setup the source tree
with `repo init` command by selecting the correct branch and manifest.

In a separate directory, call `repo init`:

```
mkdir -p ~/aosp-kernel
cd ~/aosp-kernel
repo init -u https://gitlab.com/qorvo/uwb-mobile/qm/android/aosp-manifest \
  -b develop -m kernel-qm35-manifest.xml --depth=1 \
  --reference=path/to/mirror/android-kernel
```

If you don't have android-kernel already cloned locally, remove the
`--reference=` argument.

Finally, download all sources and required tools with `repop sync`:

```
repo sync -c --jobs-network=8 --no-clone-bundle -v
```

The used manifest `kernel-qm35-manifest.xml` ensures all dependencies are
correctly downloaded and placed at good places and setups required links.

A fresh copy of `linux-drivers` repository is cloned in
`~/aosp-kernel/vendor/qorvo/linux-drivers` with all required dependencies
cloned in `deps` subdirectory. No need to use `git submodule` command.

## Build the drivers

After the `repo sync` command succeed, launch build with the created link:

For example:
```
export TARGET_KERNEL_USE=qm
export TARGET_USES_BOOT_HDR_V3=true
export QM_KERNEL_OUT=$PWD/out/android13-5.10/dist
SKIP_MRPROPER=1 QM_MODULE_ONLY=true ./build-qm_uwb.sh -j16
```

Remove `QM_MODULE_ONLY=true` to do a full kernel build and resolve module
symbols missing errors if any.

## Customizing

Since `repo` use shallow clone by default, if you want to change files in
any modules, use `git fetch --unshallow` first to be able to manage branch
and compare them easily. You can use `repo` too.
