# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

# Do not put in __all__ your uci Client, new Gids, or other extension objects
# unless you want to block the addin mechanism.

from .utils import DynIntEnum
from .fira import App, SessionType, OidRanging

__all__ = []


class RadarSessionTypeExtension(DynIntEnum):
    Radar = 0xA1


SessionType.extend(RadarSessionTypeExtension)


class RadarAppExtension(DynIntEnum):
    TimingParams = 0xB0
    SamplesPerSweep = 0xB1
    SweepOffset = 0xB2
    BitsPerSample = 0xB3
    NumberOfBursts = 0xB4
    RadarDataType = 0xB5
    AntennaSetId = 0xB6
    TxProfileIdx = 0xB7


App.extend(RadarAppExtension)

App.defs.extend(
    [
        (App.TimingParams, [7, [4, 2, 1]]),
        (App.SamplesPerSweep, 1),
        (App.SweepOffset, 2),
        (App.BitsPerSample, 1),
        (App.NumberOfBursts, 2),
        (App.RadarDataType, 1),
        (App.AntennaSetId, 1),
        (App.TxProfileIdx, 1),
    ]
)


class RadarOidRangingExtension(DynIntEnum):
    Radar = 0x0A


OidRanging.extend(RadarOidRangingExtension)
