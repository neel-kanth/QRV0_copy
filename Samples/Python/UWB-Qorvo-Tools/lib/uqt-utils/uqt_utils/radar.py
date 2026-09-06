#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2


__all__ = ["RadarClient"]

import math
import queue
import json
import struct
from pprint import pformat
from enum import IntEnum
from typing import Optional

from uci.fira import (
    App,
    Client,
    SessionType,
    Status,
    Gid,
    OidSession,
    OidCore,
    OidRanging,
    SessionState,
    SessionStateChangeReason,
)
from uci.fira_conf import Config
from uci.core import list_to_bytes, tlvs_from_bytes, tvs_to_bytes, DPF
from uci.utils import Uint8, Int16, Uint16


class BitsPerSample(IntEnum):
    RADAR_32_BITS_PER_SAMPLE = 0
    RADAR_48_BITS_PER_SAMPLE = 1
    RADAR_64_BITS_PER_SAMPLE = 2


class RadarSample:
    def __init__(self, data: bytes) -> None:
        assert len(data), "Empty sample data"

        num_of_bytes = len(data)
        self.re: int = self.__get_half_tap(data[: int(num_of_bytes / 2)])
        self.im: int = self.__get_half_tap(data[int(num_of_bytes / 2) :])
        self.z: float = math.sqrt(self.re * self.re + self.im * self.im)

    def __get_half_tap(self, half_tap: bytes) -> int:
        half_tap_size_bytes = len(half_tap)

        # Alignment to 4 bytes.
        alignment = b""
        for _ in range(4 - half_tap_size_bytes):
            alignment += b"\x00"
        value = struct.unpack("<i", half_tap[:half_tap_size_bytes] + alignment)[0]
        # Apply two's complement on 2 and 3 bytes
        if half_tap_size_bytes != 4:
            half_tap_size_bits = half_tap_size_bytes * 8
            if (value & (1 << (half_tap_size_bits - 1))) != 0:
                value = value - (1 << half_tap_size_bits)
        return value


class RadarSweep:
    def __init__(
        self, payload: bytes, bits_per_sample: int, samples_per_sweep: int
    ) -> None:
        self.sequence_number = int.from_bytes(payload[0:4], "little")
        self.timestamp = int.from_bytes(payload[4:8], "little")
        self.vendor_specific_data_length = int.from_bytes(payload[8:9], "little")
        if self.vendor_specific_data_length != 0:
            self.vendor_specific_data = int.from_bytes(
                payload[9 : (9 + self.vendor_specific_data_length)], "little"
            )
        self.sample_data: list[RadarSample] = list()

        samples_start = 9 + self.vendor_specific_data_length

        sample_size: int = 0
        if bits_per_sample == BitsPerSample.RADAR_32_BITS_PER_SAMPLE:
            sample_size = 4
        elif bits_per_sample == BitsPerSample.RADAR_48_BITS_PER_SAMPLE:
            sample_size = 6
        elif bits_per_sample == BitsPerSample.RADAR_64_BITS_PER_SAMPLE:
            sample_size = 8

        for i in range(samples_per_sweep):
            sample_start = int(samples_start + i * sample_size)
            sample_end = int(samples_start + (i + 1) * sample_size)
            self.sample_data.append(RadarSample(payload[sample_start:sample_end]))

    def get_amplitudes(self) -> list[float]:
        amps: list[float] = []
        for sample in self.sample_data:
            amps.append(sample.z)
        return amps

    def get_real_values(self) -> list[float]:
        re: list[float] = []
        for sample in self.sample_data:
            re.append(sample.re)
        return re

    def get_imaginary_values(self) -> list[float]:
        im: list[float] = []
        for sample in self.sample_data:
            im.append(sample.im)
        return im

    def __str__(self) -> str:
        return f"RadarSweep: \n {pformat(vars(self))}"


class RadarDataMessage:
    """
    This class is encapsulating Radar Data Message.
    All values are in 'natural number' format.
    """

    def __init__(self, payload: bytes) -> None:
        self.status_code = int.from_bytes(payload[0:1], "little")
        self.radar_data_type = int.from_bytes(payload[1:2], "little")
        self.number_of_sweeps = int.from_bytes(payload[2:3], "little")
        self.samples_per_sweep = int.from_bytes(payload[3:4], "little")
        self.bits_per_sample = int.from_bytes(payload[4:5], "little")
        self.sweep_offset = int.from_bytes(payload[5:7], "little", signed=True)
        self.sweep_data_size = int.from_bytes(payload[7:9], "little")
        self.sweep_data: list[RadarSweep] = list()

        sweeps_start = 9
        for i in range(1, self.number_of_sweeps + 1):
            self.sweep_data.append(
                RadarSweep(
                    payload[
                        sweeps_start
                        + (i - 1) * self.sweep_data_size : sweeps_start
                        + i * self.sweep_data_size
                    ],
                    self.bits_per_sample,
                    self.samples_per_sweep,
                )
            )

    def __str__(self) -> str:
        return f"RadarDataMessage: \n {pformat(vars(self))}"


class RadarDataEncoder(json.JSONEncoder):
    """
    Custom encoder for json serialization of RadarData class.
    """

    def default(self, o):
        if isinstance(o, RadarDataMessage):
            return o.__dict__
        if isinstance(o, RadarSweep):
            return o.__dict__
        if isinstance(o, RadarSample):
            return o.__dict__
        if isinstance(o, Uint8):
            return o.value
        if isinstance(o, Int16):
            return o.value
        if isinstance(o, Uint16):
            return o.value
        return json.JSONEncoder.default(self, o)


def RadarConfigDecoder(obj):
    if "__type__" in obj and obj["__type__"] == "Uint8":
        return Uint8(obj["value"])
    if "__type__" in obj and obj["__type__"] == "Int16":
        return Int16(obj["value"])
    if "__type__" in obj and obj["__type__"] == "Uint16":
        return Uint16(obj["value"])
    return obj


class RadarClient:
    def __init__(self, *args, **kwargs):
        self.msg_queue = queue.Queue()
        self.session_handle = 1
        self._frame_nb = 0
        self._frame_ok = 0
        self._frame_error = 0
        kwargs["data_handlers"] = {
            (DPF.RadarDataMessage): self.radar_data_cb,
        }

        self._client = Client(*args, **kwargs)

    def get_client(self):
        return self._client

    def radar_enable(self, session_id=1):
        payload = (session_id).to_bytes(4, "little")
        payload += (SessionType.Radar).to_bytes(1, "little")

        payload = self._client.command(Gid.Session, OidSession.Init, payload)
        status = Status((int).from_bytes(payload[0:1], "little"))
        self.session_handle = (int).from_bytes(payload[1:5], "little")
        return status, self.session_handle

    def radar_disable(self) -> Status:
        payload = (self.session_handle).to_bytes(4, "little")

        payload = self._client.command(Gid.Session, OidSession.Deinit, payload)
        ret = Status((int).from_bytes(payload[0:1], "little"))
        return ret

    def radar_start(self) -> Status:
        self.frame_nb = 0
        payload = (self.session_handle).to_bytes(4, "little")

        payload = self._client.command(Gid.Ranging, OidRanging.Start, payload)
        ret = Status((int).from_bytes(payload[0:1], "little"))
        return ret

    def radar_stop(self) -> Status:
        payload = (self.session_handle).to_bytes(4, "little")

        payload = self._client.command(Gid.Ranging, OidRanging.Stop, payload)
        ret = Status((int).from_bytes(payload[0:1], "little"))
        return ret

    def radar_data_cb(self, payload):
        self.msg_queue.put(payload)

    def radar_get_data(self) -> Optional[RadarDataMessage]:
        # Get a new CIR message.
        payload = self.msg_queue.get(timeout=5)
        session_handle = int.from_bytes(payload[:4], byteorder="little")
        if self.session_handle == session_handle:
            return RadarDataMessage(payload=payload[4:])
        else:
            return None

    def radar_get_config(self) -> dict:
        payload = (self.session_handle).to_bytes(4, "little")
        payload += list_to_bytes([])

        payload = self._client.command(Gid.Session, OidSession.GetAppConfig, payload)
        status = Status((int).from_bytes(payload[0:1], "little"))
        values = tlvs_from_bytes(App, payload[1:])
        cfg = {}
        cfg["status"] = status.name
        for v in values:
            cfg[App(v[0]).name] = hex(v[2])

        return cfg

    def radar_set_config(self, cfg: dict) -> Status:
        payload = (self.session_handle).to_bytes(4, "little")
        to_send = []
        for k, v in cfg.items():
            to_send.append((App[k], v))

        payload += tvs_to_bytes(App.defs, to_send)
        payload = self._client.command(Gid.Session, OidSession.SetAppConfig, payload)
        ret = Status((int).from_bytes(payload[0:1], "little"))
        return ret

    def radar_set_core_config(self, cfg: dict) -> Status:
        to_send = []
        for k, v in cfg.items():
            to_send.append((Config[k], v))

        payload = tvs_to_bytes(Config.defs, to_send)
        payload = self._client.command(Gid.Core, OidCore.SetConfig, payload)
        ret = Status((int).from_bytes(payload[0:1], "little"))
        return ret

    def decorate_max_number_of_measurement_reached_status(
        self, original_callback, max_measurement_nr_reached_callback
    ):
        def wrapper(*args, **kwargs):
            result = original_callback(*args, **kwargs)
            payload = kwargs.pop("payload", None)
            if payload is None and len(args) > 0:
                payload = args[0]

            (status, reason) = (
                (int).from_bytes(payload[4:5], "little"),
                (int).from_bytes(payload[5:6], "little"),
            )

            if (
                SessionState(status) == SessionState.Idle
                and SessionStateChangeReason(reason)
                == SessionStateChangeReason.MaxNumberOfMeasurementReached
            ):
                max_measurement_nr_reached_callback()

            return result

        return wrapper

    def radar_set_max_measurement_nr_reached_clbk(self, callback):
        current_notif_handlers = self._client.get_notif_handlers()

        for handler in current_notif_handlers:
            if handler == (Gid.Session, OidSession.Status):
                current_notif_handlers[handler] = (
                    self.decorate_max_number_of_measurement_reached_status(
                        current_notif_handlers[handler], callback
                    )
                )
