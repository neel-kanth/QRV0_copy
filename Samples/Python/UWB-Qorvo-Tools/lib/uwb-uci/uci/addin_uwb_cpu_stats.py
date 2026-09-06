# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

# Do not put in __all__ your uci Client, new Gids, or other extension objects
# unless you want to block the addin mechanism.

from .utils import DynIntEnum, Buffer
from .fira_enums import Status
from . import fira
from . import qorvo
from dataclasses import dataclass

__all__ = ["GetUwbCpuStatsOutput"]


class Client_extension:
    def get_uwb_cpu_stats(self):
        payload = self.command(fira.Gid.Qorvo, qorvo.OidQorvo.GetUwbCpuStats, b"")
        b = Buffer(payload)
        rts = Status(b.pop_uint(1))
        if rts == Status.Ok:
            timestamp_since_last_reset = b.pop_uint(8)
            type_stats = b.pop_uint(1)
            probe_count = b.pop_uint(1)
            probes = []
            for _ in range(probe_count):
                probe_name_length = b.pop_uint(1)
                probe_name = b.pop_str(probe_name_length)
                cumulative_time_us = b.pop_uint(8)
                min_duration_us = b.pop_uint(4)
                max_duration_us = b.pop_uint(4)
                nb_histograms = b.pop_uint(1)
                histo_range_seperators = []
                for _ in range(nb_histograms - 1):
                    histo_range_seperators.append(b.pop_uint(4))
                histo_counter = []
                for _ in range(nb_histograms):
                    histo_counter.append(b.pop_uint(1))
                error = b.pop_uint(1)
                probes.append(
                    (
                        probe_name_length,
                        probe_name,
                        cumulative_time_us,
                        min_duration_us,
                        max_duration_us,
                        nb_histograms,
                        histo_range_seperators,
                        histo_counter,
                        error,
                    )
                )
        else:
            timestamp_since_last_reset = 0
            type_stats = 0
            probe_count = 0
            probes = []
        return (rts, timestamp_since_last_reset, type_stats, probe_count, probes)

    def reset_uwb_cpu_stats(self):
        payload = self.command(fira.Gid.Qorvo, qorvo.OidQorvo.ResetUwbCpuStats, b"")
        b = Buffer(payload)
        rts = Status(b.pop_uint(1))

        return rts


fira.Client.extend(Client_extension)


class OidQorvo_extension(DynIntEnum):
    GetUwbCpuStats = 0x25  # GET_UWB_CPU_STATS_CMD/RSP
    ResetUwbCpuStats = 0x26  # RESET_UWB_CPU_STATS_CMD/RSP


qorvo.OidQorvo.extend(OidQorvo_extension)


@dataclass
class GetUwbCpuStatsOutput:
    """
    GET_UWB_CPU_STATS_CMD data.
    """

    def __init__(self, payload: bytes):
        self.gid = fira.Gid.Qorvo
        self.oid = qorvo.OidQorvo.GetUwbCpuStats
        self.payload = payload
        b = Buffer(payload)
        status = Status(b.pop_uint(1))

        if status == Status.Ok:
            probe_count = b.pop_uint(1)
            probes = []
            for _ in range(probe_count):
                probe_name_length = b.pop_uint(1)
                probe_name = b.pop_str(probe_name_length)
                cumulative_time_us = b.pop_uint(8)
                min_duration_us = b.pop_uint(4)
                max_duration_us = b.pop_uint(4)
                nb_histograms = b.pop_uint(1)
                histo_range_seperators = b.pop_uint(nb_histograms - 1)
                histo_counter = b.pop_uint(nb_histograms)
                error = b.pop_uint(1)
                probes.append(
                    (
                        probe_name_length,
                        probe_name,
                        cumulative_time_us,
                        min_duration_us,
                        max_duration_us,
                        nb_histograms,
                        histo_range_seperators,
                        histo_counter,
                        error,
                    )
                )
        else:
            probe_count = 0
            probes = []

    def __str__(self) -> str:
        if self.status == Status.Success:
            rts = f"""# GET_UWB_CPU_STATS_CMD:
            status: {self.status.name} ({self.status.value})
            timestamp_since_last_reset: {self.timestamp_since_last_reset}
            type_stats: {self.type_stats}
            probe_count: {self.probe_count}
            probes: {self.probes}\n"""
        else:
            rts = f"""# GET_UWB_CPU_STATS_CMD:
            status: {self.status.name})\n"""
        return rts


@dataclass
class ResetUwbCpuStatsOutput:
    """
    RESET_UWB_CPU_STATS_CMD data.
    """

    def __init__(self, payload: bytes):
        self.gid = fira.Gid.Qorvo
        self.oid = qorvo.OidQorvo.ResetUwbCpuStats
        self.payload = payload
        b = Buffer(payload)
        Status(b.pop_uint(1))

    def __str__(self) -> str:
        if self.status == Status.Success:
            rts = f"""# RESET_UWB_CPU_STATS_CMD:
            status: {self.status.name} ({self.status.value})\n"""
        else:
            rts = f"""# RESET_UWB_CPU_STATS_CMD:
            status: {self.status.name})\n"""
        return rts


# =============================================================================
# UCI Message Extension
# =============================================================================

fira.uci_codecs.update(
    {
        (
            fira.MT.Command,
            fira.Gid.Qorvo,
            qorvo.OidQorvo.GetUwbCpuStats,
        ): fira.default_codec("Get UWB CPU Stats", no_data=True),
        (
            fira.MT.Response,
            fira.Gid.Qorvo,
            qorvo.OidQorvo.GetUwbCpuStats,
        ): GetUwbCpuStatsOutput,
        (
            fira.MT.Command,
            fira.Gid.Qorvo,
            qorvo.OidQorvo.ResetUwbCpuStats,
        ): fira.default_codec("Reset UWB CPU Stats", no_data=True),
        (
            fira.MT.Response,
            fira.Gid.Qorvo,
            qorvo.OidQorvo.ResetUwbCpuStats,
        ): ResetUwbCpuStatsOutput,
    }
)
