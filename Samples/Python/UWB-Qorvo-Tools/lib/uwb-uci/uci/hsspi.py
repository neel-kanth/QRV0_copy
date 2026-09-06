# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2
import datetime
import logging
import pathlib
import struct
import threading
import time
from typing import Optional
from enum import IntEnum
from time import sleep

from uci.spi.spi_ft2232 import SpiFT2232
from uci.spi.spi_ft4222 import SpiFT4222

STC_FLAG = "<BBh"
STC_FLAG_LEN = struct.calcsize(STC_FLAG)
STC_HDR_LEN = 4

UL_FLASHING = 1
UL_UCI = 2
UL_COREDUMP = 3
UL_LOGS = 4
UL_QTRACE = 5

SS_RDY_COUNTER_MAX = 100
MAX_HSSPI_RETRY_TIMES = 100

logger = logging.getLogger(__name__)


class Gpio_dir(IntEnum):
    IN = 0
    OUT = 1


class HostStcFlag(IntEnum):
    WR = 1 << 7
    PRD = 1 << 6
    RD = 1 << 5


class SocStcFlag(IntEnum):
    OWD = 1 << 7
    OA = 1 << 6
    RDY = 1 << 5
    ERR = 1 << 4


class CoredumpData:
    """Container class to build coredump data retrieved from SPI logs on a dedicated UL.

    Args:
        expected_size: Coredump size as indicated in the ntf header.
        expected_crc: Coredump crc as indicated in the ntf header.

    Raises:
        ValueError: If ``expected_size`` is invalid.
    """

    HEADER_SIZE = 7

    #  `command_id` uint8 field for coredump packets on the dedicated UL
    NTF_HEADER = 0
    NTF_DATA = 1
    RCV_STATUS = 2
    FORCE_COREDUMP = 3

    # Coredump acknowledge status for RCV_STATUS command
    COREDUMP_RCV_NACK = 0
    COREDUMP_RCV_ACK = 1

    def __init__(self, expected_size: int, expected_crc: int) -> None:
        self.expected_size = expected_size
        self.expected_crc = expected_crc
        self._data = bytearray()
        # Condition to protect self._data, and notify about new content received in it
        self._lock_c = threading.Condition()

        if expected_size <= 0:
            raise ValueError("invalid expected size")

    @classmethod
    def create(cls, ntf_header: bytes) -> "CoredumpData":
        """Factory method using a raw coredump notification header.

        Args:
            ntf_header: Coredump notification header received via spi.

        Returns:
            CoredumpData instance.

        Raises:
            ValueError: if ``ntf_header`` is not a valid header.

        """
        if len(ntf_header) != cls.HEADER_SIZE or ntf_header[0] != cls.NTF_HEADER:
            raise ValueError("invalid coredump notification header")
        return CoredumpData(
            expected_size=struct.unpack("<I", ntf_header[1:5])[0],
            expected_crc=struct.unpack("<H", ntf_header[5:])[0],
        )

    def add(self, ntf_data: bytes) -> None:
        """Add newly received data for the coredump.

        Args:
            ntf_data: Coredump notification data received via spi.

        Raises:
            ValueError: if ``ntf_data`` is not valid data.
        """
        if not ntf_data:
            return
        if ntf_data[0] != self.NTF_DATA:
            raise ValueError(
                "invalid ntf type (new coredump while the previous one was not over?)"
            )
        with self._lock_c:
            self._data.extend(ntf_data[1:])
            self._lock_c.notify_all()

    @property
    def crc(self) -> int:
        """Checksum of the current coredump data stored.

        Raises:
            ValueError: If no data was received so far.
        """
        with self._lock_c:
            if not self._data:
                raise ValueError("no coredump data")
            return sum(self._data) & 0xFFFF

    @property
    def size(self) -> int:
        """Size of the current coredump data stored."""
        with self._lock_c:
            return len(self._data)

    def is_complete(self) -> bool:
        """Check if the current coredump data stored is full.

        It is considered as full if it matches at least the size announced in the
        initial header.

        A complete coredump does not necessarily mean it is valid (matching its expected
        checksum, only that no more incoming data is expected.

        Returns:
            Whether all the expected coredump data was received.
        """
        return self.size >= self.expected_size

    def is_valid(self) -> bool:
        """Check if the current coredump data stored is full and valid.

        It is considered valid if it matches exactly the size and crc announced in the
        initial header.

        Returns:
            Whether the coredump data is complete and valid.
        """
        if not self.is_complete():
            print(
                "spi coredump not complete"
                f" (size expected={self.expected_size} received={self.size})"
            )
            return False
        if self.size > self.expected_size:
            print(
                "spi coredump bigger than expected size"
                f" (size expected={self.expected_size} received={self.size})"
            )
            return False
        try:
            if self.crc != self.expected_crc:
                print(
                    "spi coredump corrupted"
                    f" (crc expected={self.expected_crc} received={self.crc})"
                )
                return False
        except ValueError:
            print("spi coredump: could not compute crc")
            return False
        return True

    def save(self, filepath: pathlib.Path) -> None:
        """Save the coredump on disk.

        Args:
            filepath: File location where to save the coredump data.

        Raises:
            RuntimeError: If the coredump is not ready to be saved (incomplete or
                invalid).

        To make sure the coredump data saved is usable, a prior call to
        :meth:`wait_for_complete` and then :meth:`is_valid` is advised.

        """
        if not self.is_complete():
            raise RuntimeError("Coredump is not complete")
        if not self.is_valid():
            raise RuntimeError("Coredump is corrupted")
        filepath.parent.mkdir(parents=True, exist_ok=True)
        filepath.write_bytes(self._data)


class HsspiTransportProtocol:
    def __init__(self, port, frequency=None, qtraces_logfile=None):
        self.qtraces_file = None
        self._write_lock = threading.Lock()
        self._write_cmd = None
        if qtraces_logfile:
            log_path = pathlib.Path(qtraces_logfile)
            if log_path.is_dir():
                timestamp = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
                qtraces_file_path = log_path / f"qtraces_logs_{timestamp}.bin"
            else:
                if log_path.suffix != ".bin":
                    log_path = log_path.with_suffix(".bin")
                qtraces_file_path = log_path

            qtraces_file_path.parent.mkdir(parents=True, exist_ok=True)

            self.qtraces_file = qtraces_file_path.open("wb")
            print(f"Qtraces log file will be saved to {qtraces_file_path}")

        if "FT4222" in port:
            self.spi = SpiFT4222(
                port,
                frequency,
                gpio_direction=(Gpio_dir.OUT, Gpio_dir.IN, Gpio_dir.IN),
                gpio_initial=(True, False, False),
            )
            self.ss_rdy_gpio = 2
            self.ss_irq_gpio = 1
            self.reset_gpio = 0
        else:
            self.spi = SpiFT2232(
                port,
                frequency,
                gpio_direction=(Gpio_dir.IN, Gpio_dir.IN, Gpio_dir.OUT),
                gpio_initial=(False, False, True),
            )

            self.ss_rdy_gpio = 0
            self.ss_irq_gpio = 1
            self.reset_gpio = 2

        self.spi.set_gpio(self.reset_gpio, True)
        self.running = False
        self.coredump_handlers = None
        # Start a thread to catch messages from the slave
        self.read_thread = threading.Thread(target=self.wait_for_msg, daemon=True)

    def start(self):
        self.read_thread.start()

    def reset(self):
        print("Reset device")
        self.spi.set_gpio(self.reset_gpio, False)
        sleep(0.2)
        self.spi.set_gpio(self.reset_gpio, True)

    def get_ss_irq(self) -> int:
        return self.spi.get_gpio(self.ss_irq_gpio)

    def wait_ss_irq(self):
        while not ((self.get_ss_irq() == 1) and (self.get_ss_rdy() == 1)):
            if not self.running:
                return

    def get_ss_rdy(self) -> int:
        return self.spi.get_gpio(self.ss_rdy_gpio)

    def wait_ss_rdy(self):
        ss_rdy_counter = 0
        while self.get_ss_rdy() != 1:
            ss_rdy_counter += 1
            if ss_rdy_counter >= SS_RDY_COUNTER_MAX:
                # Waited long enough for SS_RDY, do the wake-up pulse on CS
                # by forcing pre-read
                ss_rdy_counter = 0
                self.force_pre_read()
            if not self.running:
                return

    def construct_write(self, ul: int, payload: bytes) -> bytes:
        data = struct.pack(STC_FLAG, HostStcFlag.WR, ul, len(payload)) + payload
        return data

    def construct_pre_read(self, ul: int) -> bytes:
        data = struct.pack(STC_FLAG, HostStcFlag.PRD, ul, 0)
        return data

    def construct_read(self, ul: int, length: int) -> bytes:
        data = struct.pack(STC_FLAG, HostStcFlag.RD, ul, length)
        return data

    def write(self, ul: int, payload: bytes) -> bytes:
        return self._spi_write(ul, payload)

    def pre_read(self) -> bytes:
        data = self.construct_pre_read(UL_UCI)
        return self.transceive(data, STC_HDR_LEN)

    def force_pre_read(self) -> bytes:
        """
        Special pre_read command that do not checks SS_RDY pin. It will force
        SPI_SC pin toggle resulting in QM35 wake-sp if such wake-up source is enabled
        in firmware
        """
        data = self.construct_pre_read(UL_UCI)
        return self.spi.transceive(data, STC_HDR_LEN)

    def read(self, ul: int, length: int) -> bytes:
        # logging.debug("read ul=%d; length=%d", ul, length)
        data = self.construct_read(ul, length)
        return self.transceive(data, length + STC_HDR_LEN)

    def _spi_write(
        self,
        ul: int,
        payload: bytes,
        timeout_s: Optional[float] = 3,
        deferred: bool = True,
    ) -> bytes:
        data = self.construct_write(ul, payload)
        write_done_flag = threading.Event()
        answer: bytes = b""

        def _write_cmd() -> None:
            nonlocal write_done_flag, answer
            answer = self.transceive(data, STC_HDR_LEN)
            write_done_flag.set()

        # The sending of the command is deferred to the threaded poll loop, so that it
        # can be free of any (time-consuming) medium access lock at each loop iteration.
        # Instead, the only lock is here on the write side, less frequent and less on
        # a critical path, to protect _write_cmd during the processing of the command
        with self._write_lock:
            if deferred:
                self._write_cmd = _write_cmd
                if write_done_flag.wait():
                    return answer
            else:
                # For cases where the write() method is called from inside the polling
                # loop it cannot be deferred (or it would deadlock, as the write
                # normally occurs in that loop as well)
                _write_cmd()
                return answer

    def transceive(self, data: bytes, length: int) -> bytes:
        self.wait_ss_rdy()
        received_bytes = self.spi.transceive(data, length)
        retry_times = MAX_HSSPI_RETRY_TIMES
        while retry_times > 0:
            # Check if all received bytes are 0x00 or 0xFF.
            if all(rb == 0 for rb in received_bytes) or all(
                rb == 0xFF for rb in received_bytes
            ):
                # Device is in sleep - retry communication to generate CS
                # pulse and wake up the device.
                received_bytes = self.spi.transceive(data, length)
                retry_times -= 1
            else:
                break
        return received_bytes

    def get_stc(self, frame) -> int:
        return frame[0]

    def get_ul(self, frame) -> int:
        return frame[1]

    def get_length(self, frame) -> int:
        return struct.unpack(STC_FLAG, frame[:STC_FLAG_LEN])[2]

    def set_msg_handler(self, handler):
        self.msg_handlers = handler

    def set_coredump_handler(self, handler):
        self.coredump_handlers = handler

    def ack_coredump(self) -> None:
        try:
            self._spi_write(
                UL_COREDUMP,
                bytes([CoredumpData.RCV_STATUS, CoredumpData.COREDUMP_RCV_ACK]),
                timeout_s=1,
                deferred=False,
            )
            print(f"{__name__}: coredump acknowledged")

            # Reset after coredump
            print(
                f"{__name__}: a mandatory reset is needed after acking the coredump. Resetting device..."
            )
            self.reset()
        except TimeoutError as exc:
            print(f"{__name__}: couldn't acknowledge coredump ({exc})")

    def wait_for_msg(self):
        if self.qtraces_file:
            # Request the qtrace header: it contains important information (time scale
            # and mapping ids <-> module names) and is only sent at boot otherwise
            self._spi_write(UL_QTRACE, b"", deferred=False)

        self.running = True
        while self.running:
            # Give opportunities to other threads to wake up and work too
            time.sleep(0.0005)
            # If a write request was done, process it first
            if (write_cmd := self._write_cmd) is not None:
                self._write_cmd = None
                write_cmd()

            # Poll incoming data
            if not self.get_ss_irq():
                continue
            prd_resp = self.pre_read()
            if not prd_resp:
                continue
            time.sleep(0.00004)  # Wait at least 40us between PRD and RD
            resp = self.read(self.get_ul(prd_resp), self.get_length(prd_resp))
            if not resp:
                continue
            flags = self.get_stc(resp)
            if not flags & SocStcFlag.OA:
                continue
            ul = self.get_ul(resp)
            if ul == UL_UCI:
                self.msg_handlers()(resp[STC_HDR_LEN:])
            elif ul == UL_QTRACE:
                if self.qtraces_file:
                    self.qtraces_file.write(resp[STC_HDR_LEN:])
                    self.qtraces_file.flush()
            elif ul == UL_LOGS:
                try:
                    msg = resp[STC_HDR_LEN + 4 :].decode("utf-8").strip()
                except UnicodeDecodeError:
                    msg = repr(resp).strip()
                if msg:
                    print("QM35-LOG: " + msg)
            elif ul == UL_COREDUMP:
                if self.coredump_handlers:
                    self.coredump_handlers(resp[STC_HDR_LEN:])
                else:
                    print(f"{__name__}: coredump detected in device.")
                    self.ack_coredump()
        self.running = False

    def close(self):
        self.running = False
        try:
            self.read_thread.join(timeout=1)
        except:
            pass
        if self.qtraces_file is not None and not self.qtraces_file.closed:
            self.qtraces_file.close()
            self.qtraces_file = None

    def __del__(self):
        self.close()
