# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

from pyftdi.spi import SpiController


class SpiFT2232:
    def __init__(self, port, frequency, gpio_direction, gpio_initial):
        self.master = SpiController()
        if frequency is None:
            frequency = 1000000  # default frequency for ft2232

        gpio_direction_bitfield = (
            (gpio_direction[0] << 4)
            + (gpio_direction[1] << 5)
            + (gpio_direction[2] << 6)
        )

        gpio_initial_bitfield = (
            (gpio_initial[0] << 4) + (gpio_initial[1] << 5) + (gpio_initial[2] << 6)
        )

        self.master.configure(
            port, direction=gpio_direction_bitfield, initial=gpio_initial_bitfield
        )
        self.slave = self.master.get_port(cs=0, freq=frequency, mode=0)

        self.gpio = self.master.get_gpio()
        self.gpio.set_direction(
            (1 << 4) | (1 << 5) | (1 << 6),
            gpio_direction_bitfield,
        )

    def transceive(self, data: bytes, length: int) -> bytes:
        return self.slave.exchange(data, length, duplex=True)

    def get_gpio(self, id: int) -> int:
        assert id >= 0 and id <= 4
        return (self.gpio.read() >> (id + 4)) & 1

    def set_gpio(self, id: int, level: bool):
        assert id >= 0 and id <= 3
        self.gpio.write((level << id) << 4)
