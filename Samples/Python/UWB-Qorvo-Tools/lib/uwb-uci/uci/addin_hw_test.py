# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

# Do not put in __all__ your uci Client, new Gids, or other extension objects
# unless you want to block the addin mechanism.

import struct
import logging
from enum import IntEnum, IntFlag

from . import fira
from .fira_enums import Status

logger = logging.getLogger(__name__)


__all__ = [
    "GpioId",
    "PinId",
    "GpioDirection",
    "GpioFlagInput",
    "GpioFlagOutput",
    "GpioState",
    "ClockIdQm357",
    "ClockIdQm358",
    "ClockControl",
]


class OidHwTestMode(IntEnum):
    GpioConfigure = 0x2A
    GpioSet = 0x2B
    GpioGet = 0x2C
    ClockOutConfig = 0x2D
    ClockOutControl = 0x2E


class GpioId(IntEnum):
    Gpio0 = 0
    Gpio1 = 1
    Gpio2 = 2
    Gpio3 = 3
    Gpio4 = 4
    Gpio5 = 5
    Gpio6 = 6
    Gpio7 = 7
    Gpio8 = 8
    Gpio9 = 9
    Gpio10 = 10
    Gpio11 = 11
    Gpio12 = 12
    Gpio13 = 13
    Gpio14 = 14
    Gpio15 = 15
    Gpio16 = 16
    Gpio17 = 17
    Gpio18 = 18
    Gpio19 = 19
    Gpio20 = 20
    Gpio21 = 21
    Gpio22 = 22
    Gpio23 = 23
    Gpio24 = 24
    Gpio25 = 25
    Gpio26 = 26
    Gpio27 = 27
    Gpio28 = 28
    Gpio29 = 29
    Gpio30 = 30


class PinId(IntEnum):
    Pin0 = 0
    Pin1 = 1
    Pin2 = 2
    Pin3 = 3
    Pin4 = 4
    Pin5 = 5
    Pin6 = 6
    Pin7 = 7
    Pin8 = 8
    Pin9 = 9
    Pin10 = 10
    Pin11 = 11
    Pin12 = 12
    Pin13 = 13
    Pin14 = 14
    Pin15 = 15
    Pin16 = 16
    Pin17 = 17
    Pin18 = 18
    Pin19 = 19
    Pin20 = 20
    Pin21 = 21
    Pin22 = 22
    Pin23 = 23
    Pin24 = 24
    Pin25 = 25
    Pin26 = 26
    Pin27 = 27
    Pin28 = 28
    Pin29 = 29
    Pin30 = 30
    PinExton = 33


class GpioDirection(IntEnum):
    GpioInput = 0
    GpioOutput = 1


class GpioFlagInput(IntFlag):
    GpioInputPullDisable = 0x00
    GpioInputPullEnable = 0x01 << 0
    GpioInputPullDown = 0x00
    GpioInputPullUp = 0x01 << 1


class GpioFlagOutput(IntFlag):
    GpioOutputActiveLow = 0x00
    GpioOutputActiveHigh = 0x01 << 0
    GpioOutputInitInactive = 0x00
    GpioOutputInitActive = 0x01 << 1


class GpioState(IntEnum):
    GpioInactive = 0
    GpioActive = 1


class ClockIdQm357(IntEnum):
    ClockIdLposc = 1
    ClockIdXti = 2
    ClockIdFoscDiv4 = 3
    ClockIdRtc = 4
    ClockIdRc32k = 5


class ClockIdQm358(IntEnum):
    ClockIdLposc = 1
    ClockIdRtc = 2
    ClockIdRc32k = 3
    ClockIdXti = 4
    ClockIdFoscDiv4 = 5


class ClockControl(IntEnum):
    ClockControlStop = 0
    ClockControlStart = 1


# =============================================================================
# Additional Client functionalities
# =============================================================================
class Client:
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def gpio_configure(
        self,
        gpio_id: GpioId,
        direction: GpioDirection,
        flags: GpioFlagInput | GpioFlagOutput,
    ) -> Status:
        payload = bytes([gpio_id, direction])
        payload += struct.pack("<H", flags)
        response_payload = self.command(
            fira.Gid.Qorvo,
            OidHwTestMode.GpioConfigure,
            payload,
        )
        return Status(response_payload[0])

    def gpio_set(
        self,
        gpio_id: GpioId,
        state: GpioState,
    ) -> Status:
        payload = bytes([gpio_id, state])
        response_payload = self.command(
            fira.Gid.Qorvo,
            OidHwTestMode.GpioSet,
            payload,
        )
        return Status(response_payload[0])

    def gpio_get(
        self,
        gpio_id: GpioId,
    ) -> tuple[Status, GpioState | None]:
        payload = bytes([gpio_id])
        response_payload = self.command(fira.Gid.Qorvo, OidHwTestMode.GpioGet, payload)
        status = Status(response_payload[0])
        state = GpioState(response_payload[1]) if status == Status.Ok else None
        return (status, state)

    def clock_out_config(
        self,
        clock_id: ClockIdQm357 | ClockIdQm358,
        pin_id: PinId,
        flags: int,
    ) -> Status:
        payload = bytes([clock_id, pin_id])
        payload += struct.pack("<H", flags)
        response_payload = self.command(
            fira.Gid.Qorvo,
            OidHwTestMode.ClockOutConfig,
            payload,
        )
        return Status(response_payload[0])

    def clock_out_control(
        self,
        clock_id: ClockIdQm357 | ClockIdQm358,
        control: ClockControl,
    ) -> Status:
        payload = bytes([clock_id, control])
        response_payload = self.command(
            fira.Gid.Qorvo,
            OidHwTestMode.ClockOutControl,
            payload,
        )
        return Status(response_payload[0])


fira.Client.extend(Client)


# =============================================================================
# UCI Message Extension
# =============================================================================
for i in OidHwTestMode:
    fira.uci_codecs[(fira.MT.Command, fira.Gid.Qorvo, i)] = fira.default_codec(
        f"HwTestMode.{i.name}"
    )
    fira.uci_codecs[(fira.MT.Response, fira.Gid.Qorvo, i)] = fira.default_codec(
        f"HwTestMode.{i.name}"
    )
