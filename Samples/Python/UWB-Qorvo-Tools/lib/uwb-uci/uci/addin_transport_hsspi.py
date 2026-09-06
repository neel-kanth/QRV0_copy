# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

# Do not put in __all__ your uci Client, new Gids, or other extension objects
# unless you want to block the addin mechanism.

__all__ = []

from .transport import ITransport, logger

try:
    from . import hsspi
except ImportError:
    logger.debug("HSSPITransport not registered")
except RuntimeError as e:
    logger.debug(f"HSSPITransport: {e}")
else:

    class HSSPITransport(hsspi.HsspiTransportProtocol, ITransport):
        def __init__(self, callback, *args, **kwargs):
            if kwargs["port"].startswith("hsspi:"):
                kwargs["port"] = kwargs["port"][6:]
            super().__init__(*args, **kwargs)

            self.set_msg_handler(callback)
            if "coredump_cb" in kwargs:
                self.set_coredump_handler(kwargs["coredump_cb"])
            self.start()

        def write(self, packet, ul=hsspi.UL_UCI):
            super().write(ul, packet)

        @staticmethod
        def handle(port):
            return port[:7] == "ftdi://" or port.startswith("hsspi:")


try:
    from . import hsspi_transport_v2 as hsspi_v2
except ImportError:
    logger.debug("HSSPITransport_v2 not registered")
except RuntimeError as e:
    logger.debug(f"HSSPITransport_v2: {e}")
else:

    class HSSPITransportV2(hsspi_v2.HsspiTransportProtocolV2, ITransport):
        def __init__(self, callback, *args, **kwargs):
            kwargs["port"] = kwargs["port"][9:]
            super().__init__(*args, **kwargs)

            self.set_msg_handler(callback)
            if "coredump_cb" in kwargs:
                self.set_coredump_handler(kwargs["coredump_cb"])
            self.start()

        def write(
            self,
            packet,
            ul=hsspi_v2.UL_UCI,
        ):
            super().write(ul, packet)

        @staticmethod
        def handle(port):
            return port.startswith("hsspi_v2:")
