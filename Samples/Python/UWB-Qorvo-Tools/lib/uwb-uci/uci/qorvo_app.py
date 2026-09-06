# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

"""
This library is handling Qorvo Session parameter definition
"""

from .utils import DynIntEnum
from .fira import App, TestParam


class App_extension(DynIntEnum):
    EnableUwbDiagnostics = 0xC6
    NbOfRangeMeasurements = 0xE3
    NbOfAzimuthMeasurements = 0xE4
    NbOfElevationMeasurements = 0xE5
    RxAntennaSelection = 0xE6
    TxAntennaSelection = 0xE7
    EnableDiagnostics = 0xE8
    DiagsFrameReportsFields = 0xE9
    MacPayloadEncryption = 0xEA
    EnablePSDUDump = 0xEB


App.extend(App_extension)


App.defs.extend(
    [
        (App.NbOfRangeMeasurements, 1),
        (App.NbOfAzimuthMeasurements, 1),
        (App.RxAntennaSelection, 1),
        (App.TxAntennaSelection, 1),
        (App.EnableDiagnostics, 1),
        (App.DiagsFrameReportsFields, 1),
        (App.NbOfElevationMeasurements, 1),
        (App.MacPayloadEncryption, 1),
        (App.EnablePSDUDump, 1),
        (App.EnableUwbDiagnostics, 4),
    ]
)


class TestParam_extension(DynIntEnum):
    RssiOutliers = 0xEB


TestParam.extend(TestParam_extension)

TestParam.defs[TestParam.RssiOutliers] = 2
