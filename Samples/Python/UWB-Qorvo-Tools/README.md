# UWB Qorvo Tools

This document serves as the reference guide for UWB Qorvo Tools (UQT).
It describes the structure, installation, and usage of the utilities provided by Qorvo for operating Ultra-Wideband (UWB) devices via the UCI (UWB Communication Interface).

> **Warning**: These tools are provided by Qorvo as UCI examples on an as-is basis and are neither optimized nor intended for production use.

## Root Folder Organization

The main structure of the UQT library is outlined below. Non-essential files and directories are omitted for clarity.

```
uwb-qorvo-tools
    ├── README.md                         <-- UQT Introduction
    ├── pyproject.toml                    <-- UQT Project configuration file.
    ├── requirements.txt                  <-- UQT requirements for pip installation
    ├── LICENSE                           <-- The Qorvo software license
    ├── lib                               <-- Library folder.
    │   |── uqt-utils                     <-- Helpers for main scripts.
    │   └── uwb-uci                       <-- UCI transport layer libraries.
    └── scripts                           <-- Script folder divided into "chapters".
        ├── <functionality_1>             <-- A "chapter" represents functionality 1.
        │   ├── <example_1>               <-- Example 1 for functionality 1.
        │   │   ├── <example_1_main.py>   <-- Entrance point for example 1 for functionality 1.
        │   │   ├── <example_1_helper.py> <-- Helper script for example 1 for functionality 1.
        │   │   ├── ...                   <-- Other scripts and assets needed for for example 1 for functionality 1.
        │   │   └── README.md             <-- Documentation for example 1 for functionality 1.
        │   ├── <example_2>               <-- Example 2 for functionality 1.
        │   ├── ...                       <-- Other examples for functionality 1.
        │   └── README.md                 <-- Generic documentation for functionality 1.
        ├── <functionality_2>             <-- A "chapter" represents functionality 2.
        └── ...                           <-- Other "chapters" represent further functionalities.
```

## Prerequisites

### Linux

- Installed python 3.9 or 3.10 or 3.11. It can be installed with `sudo apt update && sudo apt install python 3.10` command.
- To use the FT4222-based HSSPI transport, add the following udev rule:
```bash
echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="601c", GROUP="plugdev", MODE="0666"' > /etc/udev/rules.d/99-ftdi.rules
```

### Windows

- PowerShell
- Installed python 3.9 or 3.10 or 3.11. Python 3.10 can be downloaded from the [official web page](https://www.python.org/downloads/)
- FTDI drivers that can be downloaded from [the official FTDI website](https://ftdichip.com/drivers/d2xx-drivers/)

## Quick Start

### Install UQT

After installing the prerequisites mentioned above, UQT can be installed. Using Python virtual environments is recommended, but not required.

#### Linux
```bash
cd Samples/Python/UWB-Qorvo-Tools
python -m venv .venv
source .venv/bin/activate
# Using Pip
pip cache purge
pip install -r requirements.txt
```

#### Windows
```bash
cd Samples/Python/UWB-Qorvo-Tools
python -m venv .venv
.\.venv\Scripts\activate
# Using Pip
pip cache purge
pip install -r requirements.txt
```

> **Note**: If pip version used is <= 21.3, an extra option `--use-feature=in-tree-build` is needed for `pip install` command.

### Executing programs

If UQT is installed inside virtual environment then before executing scripts, virtual environment activation is required.
If UQT is installed globally, then skip this step.
Virtual environment activating can be done by executing:

**Linux**
```bash
cd Samples/Python/UWB-Qorvo-Tools
source .venv/bin/activate
```

**Windows**
```bash
cd Samples/Python/UWB-Qorvo-Tools
.\.venv\Scripts\activate
```

Once the virtual environment is activated, Python scripts can be used as regular Python scripts:
```bash
python /path/to/script1.py <arg1> <arg2> <...>
python /path/to/script2.py <arg1> <arg2> <...>
```

The second option is using entry points. In this case, an explicit call to `python` is not needed:
```bash
<entry_point1> <arg1> <arg2> <...>
<entry_point2> <arg1> <arg2> <...>
```

### How to check all possible entry points

```bash
uqt_ls
-> <script_name_1>                          One line description of <script_name_1>
-> <script_name_2>                          One line description of <script_name_2>
-> <script_name_3>                          One line description of <script_name_3>
...
```

In order to get more information about specific script run it with `-h` option or read `README.md` file in appropriate subdirectory in `script` folder.

For example:
```bash
<script_name_1> -h
```

### How to check basic communication with a device

#### PC Setup

After following the installation procedure listed above,
the Python FTDI libraries for FT4222 devices will be ready to use.

Check basic communication with a device via HSSPI.
```bash
get_device_info -p ftdi://FT4222
```

#### RPi Setup

After installing the UCI kernel driver (as described in a separate guide), basic communication can be checked via the Raspberry Pi native SPI interface:
```bash
# Linux, when port is /dev/ttyACM0
get_device_info -p /dev/ttyACM0
# Windows, when port is COM9
get_device_info -p COM9
```

**In order to get more information about specific script and/or use case, read subsequent chapter or `README.md` file in appropriate subdirectory**
