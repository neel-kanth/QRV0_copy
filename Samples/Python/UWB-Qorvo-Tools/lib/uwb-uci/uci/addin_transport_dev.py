# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

# Do not put in __all__ your uci Client, new Gids, or other extension objects
# unless you want to block the addin mechanism.

__all__ = []

import os
import select
import threading

from .transport import ITransport


class DevTransport(ITransport):
    def __init__(self, callback, *args, **kwargs):
        self.device = os.open(kwargs["port"], os.O_RDWR)
        self.cb = callback

        (self.rpipe, self.wpipe) = os.pipe()

        self.reader_thread = threading.Thread(target=self.reader_fn, daemon=True)
        self.reader_thread.start()

    def reader_fn(self):
        poller = select.poll()

        poller.register(self.rpipe, select.POLLIN)
        poller.register(self.device, select.POLLIN)

        while True:
            for fd, _ in poller.poll():
                if fd == self.rpipe:
                    return

                packet = os.read(fd, 4 + 255)
                self.cb()(packet)

    def write(self, packet):
        os.write(self.device, packet)

    def close(self):
        os.close(self.rpipe)
        os.close(self.wpipe)
        self.reader_thread.join()
        os.close(self.device)

    @staticmethod
    def handle(port):
        return port.startswith("/dev/uci")
