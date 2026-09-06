# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

"""
spi_MPSSE.py: A python wrapper for the FTDI-provided libMPSSE DLL (SPI only)

Widely inspired from the https://github.com/jmbattle/pyMPSSE project from
Jason M. Battle with a MIT License.
"""

import ctypes
import logging
import time
from urllib.parse import urlparse

import pyft4222 as ft
from pyft4222.wrapper.common import ClockRate
from pyft4222.wrapper.gpio import Direction
from pyft4222.wrapper.spi import ClkPhase, ClkPolarity, DriveStrength
from pyft4222.wrapper.spi.master import ClkDiv, SsoMap

logger = logging.getLogger(__name__)

# Status Codes
STATUS_CODES = {
    0: "FT_OK",
    1: "FT_INVALID_HANDLE",
    2: "FT_DEVICE_NOT_FOUND",
    3: "FT_DEVICE_NOT_OPENED",
    4: "FT_IO_ERROR",
    5: "FT_INSUFFICIENT_RESOURCES",
    6: "FT_INVALID_PARAMETER",
    7: "FT_INVALID_BAUD_RATE",
    8: "FT_DEVICE_NOT_OPENED_FOR_ERASE",
    9: "FT_DEVICE_NOT_OPENED_FOR_WRITE",
    10: "FT_FAILED_TO_WRITE_DEVICE",
    11: "FT_EEPROM_READ_FAILED",
    12: "FT_EEPROM_WRITE_FAILED",
    13: "FT_EEPROM_ERASE_FAILED",
    14: "FT_EEPROM_NOT_PRESENT",
    15: "FT_EEPROM_NOT_PROGRAMMED",
    16: "FT_INVALID_ARGS",
    17: "FT_NOT_SUPPORTED",
    18: "FT_OTHER_ERROR",
    19: "FT_DEVICE_LIST_NOT_READY",
}

# Device Types
DEVICE_TYPES = {
    0: "FT_DEVICE_BM",
    1: "FT_DEVICE_BM",
    2: "FT_DEVICE_100AX",
    3: "FT_DEVICE_UNKNOWN",
    4: "FT_DEVICE_2232C",
    5: "FT_DEVICE_232R",
    6: "FT_DEVICE_2232H",
    7: "FT_DEVICE_4232H",
    8: "FT_DEVICE_232H",
    9: "FT_DEVICE_X_SERIES",
    10: "FT_DEVICE_4222H",
}


class FT_DEVICE_LIST_INFO_NODE(ctypes.Structure):
    _fields_ = [
        ("Flags", ctypes.c_ulong),
        ("Type", ctypes.c_ulong),
        ("ID", ctypes.c_ulong),
        ("LocID", ctypes.c_ulong),
        ("SerialNumber", ctypes.c_ubyte * 16),
        ("Description", ctypes.c_ubyte * 64),
        ("ftHandle", ctypes.c_ulonglong),
    ]


class CHANNEL_CONFIG(ctypes.Structure):
    _fields_ = [
        ("ClockRate", ctypes.c_ulong),
        ("LatencyTimer", ctypes.c_ubyte),
        ("configOptions", ctypes.c_ulong),
        ("Pin", ctypes.c_ulong),
        ("reserved", ctypes.c_ushort),
    ]


class SpiFT4222:
    def __init__(self, port, frequency, gpio_direction, gpio_initial):
        self._device = None
        self._spi_master = None
        self._gpio = None
        self._gpio_dir = (
            Direction.OUTPUT if gpio_direction[0] else Direction.INPUT,
            Direction.OUTPUT if gpio_direction[1] else Direction.INPUT,
            Direction.OUTPUT if gpio_direction[2] else Direction.INPUT,
            Direction.INPUT,
        )
        logger.debug(f"gpio_dir: {self._gpio_dir}")

        self._handle = 0

        self.spiChannel = self.get_channel_info()
        ft_ch, io_ch = self.find_channels(self.spiChannel, port)
        if not ft_ch:
            raise ValueError(f'FT4222 device "{port}" not found')

        ft_idx = self.spiChannel.index(ft_ch)
        if io_ch:
            io_idx = self.spiChannel.index(io_ch)
        else:
            io_idx = ft_idx + 1

        channel_info = (
            "Ch"
            + str(ft_idx)
            + " - "
            + str(ft_ch.loc_id)
            + " - "
            + str(ft_ch.description)
        )
        logger.debug("Opening SPI device : " + channel_info)
        self._device = ft.open_by_idx(ft_idx)

        if io_ch:
            channel_info = (
                "Ch"
                + str(io_idx)
                + " - "
                + str(io_ch.loc_id)
                + " - "
                + str(io_ch.description)
            )
            logger.debug("Opening IO device: " + str(channel_info))
        else:
            logger.debug("Using IO device following SPI device")
        self._device_io = ft.open_by_idx(io_idx)

        self._handle = self._device.val
        self._handle_io = self._device_io.val

        if isinstance(self._handle, ft.FtError) or isinstance(
            self._handle_io, ft.FtError
        ):
            raise ValueError(f'FT4222 device "{port}" not properly opened')

        # Set system clock to 48MHz (default value:60MHz)
        sys_clk, sys_clk_div = self.get_clock_div(frequency)
        self._handle.set_clock(sys_clk)

        logger.debug(self._handle.get_clock())

        self._spi_master = self._handle.init_single_spi_master(
            sys_clk_div,
            ClkPolarity.CLK_IDLE_LOW,
            ClkPhase.CLK_LEADING,
            SsoMap.SS_0,
        )
        self._gpio = self._handle_io.init_gpio(self._gpio_dir)
        # Disable suspend out, else we cannot control GPIO2
        self._gpio.set_suspend_out(False)
        # Disable wakeup interrupt, else we cannot control GPIO3
        self._gpio.set_wakeup_interrupt(False)

        # Set drive strength to 4mA (default value:4mA)
        self._spi_master.set_driving_strength(
            DriveStrength.DS_16MA, DriveStrength.DS_16MA, DriveStrength.DS_16MA
        )

        for idx, dir in enumerate(self._gpio_dir):
            if dir == Direction.OUTPUT:
                self._gpio.write(idx, gpio_initial[idx])

        logger.debug("Init channel : OK")
        time.sleep(0.5)

    def get_clock_div(self, frequency):
        closest_clk_div = (ClockRate.SYS_CLK_48, ClkDiv.CLK_DIV_4)
        closest_freq = 1.0 * closest_clk_div[0] / closest_clk_div[1]
        if not frequency:
            logger.debug(f"Setting frequency {closest_freq}")
            return closest_clk_div

        sys_clks = [
            (ClockRate.SYS_CLK_80, 80000000),
            (ClockRate.SYS_CLK_60, 60000000),
            (ClockRate.SYS_CLK_48, 48000000),
            (ClockRate.SYS_CLK_24, 24000000),
        ]
        sys_divs = [
            (ClkDiv.CLK_DIV_2, 2),
            (ClkDiv.CLK_DIV_4, 4),
            (ClkDiv.CLK_DIV_8, 8),
            (ClkDiv.CLK_DIV_16, 16),
            (ClkDiv.CLK_DIV_32, 32),
            (ClkDiv.CLK_DIV_64, 64),
            (ClkDiv.CLK_DIV_128, 128),
            (ClkDiv.CLK_DIV_256, 256),
            (ClkDiv.CLK_DIV_512, 512),
        ]

        for sys_clk_sel, sys_clk in sys_clks:
            for sys_div_sel, sys_div in sys_divs:
                new_freq = 1.0 * sys_clk / sys_div
                if abs(frequency - new_freq) < abs(frequency - closest_freq):
                    closest_freq = new_freq
                    closest_clk_div = [sys_clk_sel, sys_div_sel]
        if closest_freq != frequency * 1.0:
            logger.info(
                f"Setting frequency {frequency} to closest freq {int(closest_freq)}"
            )
        return closest_clk_div

    def get_channel_info(self) -> dict:
        channels_list = ft.get_device_info_list()
        logger.debug("Available channels: " + str(len(channels_list)))
        for dev in channels_list:
            logger.debug(dev)

        return channels_list

    def find_channels(self, channels, port) -> list:

        # The port should look like the following:
        # The frequency is optional and is 12Mbps by default
        # - ftdi://FT4222/xxx[@frequency] with xxx the loc_id or serial number
        # - ftdi://FT4222[@frequency] with loc_id = 0
        # - hsspi:FT4222/xxx[@frequency] with xxx the loc_id or serial number
        # - hsspi:FT4222[@frequency] with loc_id = 0

        p = urlparse(port)
        loc_id_or_sn_str = p.path.rsplit("/")[-1]
        try:
            loc_id = int(loc_id_or_sn_str)
        except ValueError:
            loc_id = 0

        logger.debug(f"using loc_id: {loc_id}")

        channels_list = ft.get_device_info_list()
        channels_list.sort()

        ft_channels = []
        io_channels = []

        ft4222_device_types = []
        for dt in DEVICE_TYPES.keys():
            if "4222" in DEVICE_TYPES[dt]:
                ft4222_device_types.append(dt)

        for key, ch in enumerate(channels_list):
            if ch.dev_type not in ft4222_device_types:
                # Wrong ftdi device...
                continue

            # If the loc id or SN is found, return channel A (ch) and B (key+1)
            # (the list is sorted, hence key+1 -> channel B).
            # the SN can be given either by specifying the "A" at the end or not.
            if (
                ch.loc_id == loc_id
                or ch.serial_number == loc_id_or_sn_str
                or ch.serial_number == loc_id_or_sn_str + "A"
            ):
                return ch, channels_list[key + 1]

            # No loc_id / SN is provided, so it should separate the "A" and "B" channels and return the first ones.
            if loc_id_or_sn_str == "":
                if ch.description == "FT4222 A":
                    ft_channels.append(ch)
                elif ch.description == "FT4222 B":
                    io_channels.append(ch)
                elif ch.description == "":
                    logger.warning(
                        "You have access issue with your FT4222 FTDI driver."
                    )
                    logger.warning(f"Got: {ch}")
                    raise IOError("Unable to access the FTDI device")

        if len(ft_channels) == 0:
            logger.error("No FTDI chips found.")
            return

        logger.warning(
            f"Loc_id or SN {loc_id_or_sn_str} not found. Using first device available in this list."
        )
        for ch in ft_channels:
            logger.warning(
                f"Device type {ch.dev_type}, desc {ch.description}, locid {ch.loc_id}, sn {ch.serial_number}"
            )

        return ft_channels[0], io_channels[0]

    def close_channel(self):
        self._spi_master.close()
        self._gpio.close()

    def transceive(self, data: bytes, length: int) -> bytes:
        to_send = data
        if length > len(data):
            to_send = to_send + bytes(([0] * (length - len(data))))
        elif not length:
            length = len(data)

        read_data = self._spi_master.single_read_write(to_send, True)
        return read_data[:length]

    def get_gpio(self, id: int) -> int:
        assert id >= 0 and id <= 3
        return self._gpio.read(id)

    def set_gpio(self, id: int, level: bool):
        assert id >= 0 and id <= 3
        self._gpio.write(id, level)
