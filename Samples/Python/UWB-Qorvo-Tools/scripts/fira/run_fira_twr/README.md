# run_fira_twr

Script **run_fira_twr** is provided to demonstrate a FiRa session ranging which can be easily modified.

## Parameters

Arguments with expected parameter available in this script:

| Parameter               | Values                                                                                                                |
|-------------------------|-----------------------------------------------------------------------------------------------------------------------|
| -t / --time             | set the duration of the ranging session (in second); `-1` - range forever                                             |
| -p / --port             | Specify communication interface                                                                                       |
| -c / --channel          | Refer to CHANNEL_NUMBER in ``FIRA UCI Technical Specification``                                                       |
| -s / --session_id       | set session ID (default: 42)                                                                                          |
| -v / --verbose          | prints additional debug information                                                                                   |
| --role                  | Refer to DEVICE_ROLE in ``FIRA UCI Technical Specification``                                                          |
| --round                 | Refer to RANGING_ROUND_USAGE in ``FIRA UCI Technical Specification``                                                  |
| --round-ctrl            | Refer to RANGING_ROUND_CONTROL in ``FIRA UCI Technical Specification``                                                |
| --en-key-rot            | Refer to KEY_ROTATION in ``FIRA UCI Technical Specification``                                                         |
| --key-rot-rate          | Refer to KEY_ROTATION_RATE in ``FIRA UCI Technical Specification``                                                    |
| --sts                   | Refer to STS_CONFIG in ``FIRA UCI Technical Specification``                                                           |
| --slot-span             | Refer to SLOT_DURATION in ``FIRA UCI Technical Specification``                                                        |
| --node                  | Refer to MULTI_NODE_MODE in ``FIRA UCI Technical Specification``                                                      |
| --ranging-span          | Refer to RANGING_DURATION in ``FIRA UCI Technical Specification``                                                     |
| --diag-fields           | set the Qorvo DIAGNOSTIC_FRAME_REPORTS_FIELD value OR <br>flags: metrics, aoa, cir, cfo. (default: 'metrics|aoa|cfo') |
| --meas-max              | Refer to MAX_NUMBER_OF_MEASUREMENTS in ``FIRA UCI Technical Specification``                                           |
| --skey SKEY             | Refer to SESSION_KEY in ``FIRA UCI Technical Specification``                                                          |
| --schedule              | Refer to SCHEDULE_MODE in ``FIRA UCI Technical Specification``                                                        |
| --cap-range             | Refer to CAP_SIZE_RANGE in ``FIRA UCI Technical Specification``                                                       |
| --mac                   | Refer to DEVICE_MAC_ADDRESS in ``FIRA UCI Technical Specification``                                                   |
| --dest-mac              | Refer to DST_MAC_ADDRESS in ``FIRA UCI Technical Specification``                                                      |
| --controlees-with-sskey | Refer to SESSION_UPDATE_CONTROLLER_MULTICAST_LIST_CMD in ``FIRA UCI Technical Specification``                         |
| --frame                 | Refer to RFRAME_CONFIG in ``FIRA UCI Technical Specification``                                                        |
| --ssession              | Refer to SUB_SESSION_ID in ``FIRA UCI Technical Specification``                                                       |
| --sskey                 | Refer to SUB_SESSION_KEY in ``FIRA UCI Technical Specification``                                                      |
| --controlee             | Refer to DEVICE_TYPE in ``FIRA UCI Technical Specification``                                                          |
| --en-diag               | set the Qorvo ENABLE_DIAGNOSTIC parameter to 1                                                                        |
| --en-psdu-dump          | set the Qorvo PSDU_DUMP value to 1                                                                                    |
| --dis-rssi              | Refer to RSSI_REPORTING in ``FIRA UCI Technical Specification``                                                       |
| --stats                 | Enables Statistics report at end of the run                                                                           |
| --diag_dump             | Dump the Diagnostics in the provided JSON file.                                                                       |

> **Warning(!)**, `--diag_dump` has an effect only with the `--stats` option.

The Diagnostics parameter save the diagnostics report in a json file named
``range_data_<year>-<month>-<day>-<time>.json``. For instance: ``range_data_23-04-12-10h47m25s.json``

The report contains one entry per Fira ranging sequence, each of these entries contains one entry per frame exchanged.

## Example with Default values

Initialize a TWR FiRa session as controller on the first board.

Example with COM24:

```
run_fira_twr -p COM24
```

Or, using HSSPI:

```
run_fira_twr -p ftdi://FT4222
```

Open a second command shell in the python script location.

Initialize a TWR FiRa session as controlee on the second board (in this example, in COM25).

```
run_fira_twr -p COM25 --controlee
```

Output of the script running on controller side with default parameter:

```
Initializing session 42...
Session 42 Init (StateChangeWithSessionManagementCommands)Setting session 42 config ...

DeviceRole (0x11):                  0x1
DeviceType (0x0):                   0x1
MultiNodeMode (0x3):                0x0
RangingRoundUsage (0x1):            0x2
DeviceMacAddress (0x6):             [0x0, 0x0]
ChannelNumber (0x4):                0x9
ScheduleMode (0x22):                0x1
CapSizeRange (0x20):                0x510
StsConfig (0x2):                    0x0
RframeConfig (0x12):                0x3
ResultReportConfig (0x2e):          0xf
VendorId (0x27):                    [7, 8]
StaticStsIv (0x28):                 [1, 2, 3, 4, 5, 6]
AoaResultReq (0xd):                 0x1
UwbInitiationTime (0x2b):           0x3e8
PreambleCodeIndex (0x14):           0x9
SfdId (0x15):                       0x2
SlotDuration (0x8):                 0x960
RangingInterval (0x9):              0xc8
SlotsPerRr (0x1b):                  0x19
MaxNumberOfMeasurements (0x32):     0x0
HoppingMode (0x2c):                 0x0
RssiReporting (0x13):               0x1
DstMacAddress (0x7):                [1]

[...]

# Ranging Data:
    session id:         42
    sequence n:         40
    ranging interval:   200 ms
    measurement type:   Twr
    Mac add size:       2
    primary session id: 0x0
    n of measurement:   1
    # Measurement 1:
        status:             Ok (0x0)
        mac address:        01.00
        is nlos meas:       no
        distance:           90.0 cm
        AoA azimuth:        0.0 deg
        AoA az. FOM:        0.0 %
        AoA elevation:      0.0 deg
        AoA elev. FOM:      0.0
        AoA dest azimuth:   0.0 deg
        AoA dest az. FOM:   0.0 %
        AoA dest elevation: 0.0 deg
        AoA dest elev. FOM: 0.0 %
        slot in error:      0
        rssi:               -60.5 dB

[...]

Stopping ranging...
Deinitializing session...
# Ranging Data:
    session id:         42
    sequence n:         45
    ranging interval:   200 ms
    measurement type:   Twr
    Mac add size:       2
    primary session id: 0x0
    n of measurement:   1
    # Measurement 1:
        status:             Ok (0x0)
        mac address:        01.00
        is nlos meas:       no
        distance:           90.0 cm
        AoA azimuth:        0.0 deg
        AoA az. FOM:        0.0 %
        AoA elevation:      0.0 deg
        AoA elev. FOM:      0.0
        AoA dest azimuth:   0.0 deg
        AoA dest az. FOM:   0.0 %
        AoA dest elevation: 0.0 deg
        AoA dest elev. FOM: 0.0 %
        slot in error:      0
        rssi:               -62.0 dB

Session 42 DeInit (StateChangeWithSessionManagementCommands)
Device Ready
Ok
```

## Example with Statistics

Initialize a TWR FiRa session as controller on the first board.

Example with COM37:

```
run_fira_twr -p COM37 --stats --en-diag
```

Open a second command shell in the python script location.

Initialize a TWR FiRa session as controlee on the second board (in this example, in COM40).

```
run_fira_twr -p COM40 --stats --en-diag --controlee
```

Output of the script running on controller side:

```
Initializing session 42...
Session 42 Init (StateChangeWithSessionManagementCommands)Setting session 42 config ...
DeviceRole (0x11):                  0x1

DeviceType (0x0):                   0x1
MultiNodeMode (0x3):                0x0
RangingRoundUsage (0x1):            0x2
DeviceMacAddress (0x6):             [0x0, 0x0]
ChannelNumber (0x4):                0x9
ScheduleMode (0x22):                0x1
CapSizeRange (0x20):                0x510
StsConfig (0x2):                    0x0
RframeConfig (0x12):                0x3
ResultReportConfig (0x2e):          0xf
VendorId (0x27):                    [7, 8]
StaticStsIv (0x28):                 [1, 2, 3, 4, 5, 6]
AoaResultReq (0xd):                 0x1
UwbInitiationTime (0x2b):           0x3e8
PreambleCodeIndex (0x14):           0x9
SfdId (0x15):                       0x2
SlotDuration (0x8):                 0x960
RangingInterval (0x9):              0xc8
SlotsPerRr (0x1b):                  0x19
MaxNumberOfMeasurements (0x32):     0x0
HoppingMode (0x2c):                 0x0
RssiReporting (0x13):               0x1
DstMacAddress (0x7):                [1]
EnableDiagnostics (0xe8):           0x1
DiagsFrameReportsFields (0xe9):     0x2a
Starting ranging...
Session 42 Idle (StateChangeWithSessionManagementCommands)
Device Active
Session 42 Active (StateChangeWithSessionManagementCommands)

[...]

# Ranging Data:
    session id:         42
    sequence n:         44
    ranging interval:   200 ms
    measurement type:   Twr
    Mac add size:       2
    primary session id: 0x0
    n of measurement:   1
    # Measurement 1:
        status:             RangingNegativeDistance (0x1b)
        mac address:        01.00
        is nlos meas:       no
        distance:           89.0 cm
        AoA azimuth:        11.2421875 deg
        AoA az. FOM:        88.0 %
        AoA elevation:      0.0 deg
        AoA elev. FOM:      0.0
        AoA dest azimuth:   -31.1875 deg
        AoA dest az. FOM:   92.0 %
        AoA dest elevation: 0.0 deg
        AoA dest elev. FOM: 0.0 %
        slot in error:      0
        rssi:               -65.0 dB

# Ranging Diagnostic Data:
    Session id:     42
    Sequence n:     44
    Nbr of reports: 6
    # Ranging Diag. Report 0:
        Message id:    Control
        Action:        Tx
        Antenna_set:   0
        Nbr of fields: 1
        # Frame Status Report:
            is processing ok  : 1
            is wifi activated : 0
    # Ranging Diag. Report 1:
        Message id:    RangingInitiation
        Action:        Tx
        Antenna_set:   0
        Nbr of fields: 1
        # Frame Status Report:
            is processing ok  : 1
            is wifi activated : 0
    # Ranging Diag. Report 2:
        Message id:    RangingResponse
        Action:        Rx
        Antenna_set:   0
        Nbr of fields: 4
        # Frame Status Report:
            is processing ok  : 1
            is wifi activated : 0
        # CFO Report:
            cfo:     0.015 ppm
        # Segment Metrics Reports:
            Nbr of Segment Metrics: 2
            # Segment Metrics 0:
                segment type: 1
                primary_recv: 1
                receiver Id:  0x1
                RSL q8:       16845
                path1_idx:    354
                path1_snr:    29980
                path1_t:      22681
                peak_idx:     365
                peak_snr:     16925
                peak_t:       23360
            # Segment Metrics 1:
                segment type: 1
                primary_recv: 0
                receiver Id:  0x0
                RSL q8:       16861
                path1_idx:    357
                path1_snr:    30464
                path1_t:      22855
                peak_idx:     367
                peak_snr:     16941
                peak_t:       23488
        # AoA Report on axis 0:
            TDoA:     0.0849609375
            PDoA:     33.61676453025478 deg
            AoA:      11.252239251592357  deg
            AoA FOM:  225 %
            AoA Type: XAxis
    # Ranging Diag. Report 3:
        Message id:    RangingFinal
        Action:        Tx
        Antenna_set:   0
        Nbr of fields: 1
        # Frame Status Report:
            is processing ok  : 1
            is wifi activated : 0
    # Ranging Diag. Report 4:
        Message id:    MeasurementReport
        Action:        Tx
        Antenna_set:   0
        Nbr of fields: 1
        # Frame Status Report:
            is processing ok  : 1
            is wifi activated : 0
    # Ranging Diag. Report 5:
        Message id:    RangingResultReport
        Action:        Rx
        Antenna_set:   0
        Nbr of fields: 3
        # Frame Status Report:
            is processing ok  : 1
            is wifi activated : 0
        # CFO Report:
            cfo:     -0.075 ppm
        # Segment Metrics Reports:
            Nbr of Segment Metrics: 2
            # Segment Metrics 0:
                segment type: 0
                primary_recv: 1
                receiver Id:  0x1
                RSL q8:       16099
                path1_idx:    739
                path1_snr:    32778
                path1_t:      47318
                peak_idx:     759
                peak_snr:     16179
                peak_t:       48576
            # Segment Metrics 1:
                segment type: 0
                primary_recv: 0
                receiver Id:  0x0
                RSL q8:       16439
                path1_idx:    738
                path1_snr:    -32688
                path1_t:      47254
                peak_idx:     761
                peak_snr:     16519
                peak_t:       48704

[...]

Deinitializing session...
Session 42 DeInit (StateChangeWithSessionManagementCommands)
Device Ready
Ok
Device: 01.00
                27 Successful/ 46 Total
                AVG Ranging: 70.89
                STDEV Ranging: 57.45
                AVG AoA Azimuth: -33.780
                STDEV AoA Azimuth: 43.579
                AVG AoA Elevation: 0.000
                STDEV AoA Elevation: 0.000
                Diagnostics:
                    X Axis:
                    AVG Raw AoA: -33.800
                    STDEV Raw AoA: 43.605
                    AVG Raw Pdoa: -84.002
                    STDEV Raw Pdoa: 102.376
                    Y Axis:
                    AVG Raw AoA: 0.000
                    STDEV Raw AoA: 0.000
                    AVG Raw Pdoa: 0.000
                    STDEV Raw Pdoa: 0.000
```

Statistics of the ranging are calculated at the end of the script.

## Example for one-to-many Ranging

Initialize a One-To-Many TWR FiRa session as controller on the first board.

Example with COM37:

```
run_fira_twr -p COM37 --node onetomany --dest-mac "['00:01','00:02']" --n_controlees 2
```

Open a second command shell in the python script location.

Initialize a TWR FiRa session as controlee on the second board (in this example, in COM40).

```
run_fira_twr -p COM40 --node onetomany --controlee --mac 00:01
```

Open a third command shell in the python script location.

Initialize a TWR FiRa session as controlee on the second board (in this example, in COM18).

```
run_fira_twr -p COM18 --node onetomany --controlee --mac 00:02
```

Output of the script running on controller side:


```
Initializing session 42...
Session 42 Init (StateChangeWithSessionManagementCommands)
Setting session 42 config ...
DeviceRole (0x11):                  0x1
DeviceType (0x0):                   0x1
MultiNodeMode (0x3):                0x1
RangingRoundUsage (0x1):            0x2
DeviceMacAddress (0x6):             [0x0, 0x0]
ChannelNumber (0x4):                0x9
ScheduleMode (0x22):                0x1
CapSizeRange (0x20):                0x510
StsConfig (0x2):                    0x0
RframeConfig (0x12):                0x3
ResultReportConfig (0x2e):          0xf
VendorId (0x27):                    [7, 8]
StaticStsIv (0x28):                 [1, 2, 3, 4, 5, 6]
AoaResultReq (0xd):                 0x1
UwbInitiationTime (0x2b):           0x3e8
PreambleCodeIndex (0x14):           0x9
SfdId (0x15):                       0x2
SlotDuration (0x8):                 0x960
RangingInterval (0x9):              0xc8
SlotsPerRr (0x1b):                  0x19
MaxNumberOfMeasurements (0x32):     0x0
HoppingMode (0x2c):                 0x0
RssiReporting (0x13):               0x1
NumberOfControlees (0x5):           0x2
DstMacAddress (0x7):                [1, 2]
Starting ranging...
Session 42 Idle (StateChangeWithSessionManagementCommands)
Device Active
Session 42 Active (StateChangeWithSessionManagementCommands)

[...]

# Ranging Data:
    session id:         42
    sequence n:         0
    ranging interval:   200 ms
    measurement type:   Twr
    Mac add size:       2
    primary session id: 0x0
    n of measurement:   2
    # Measurement 1:
        status:             Ok (0x0)
        mac address:        01.00
        is nlos meas:       no
        distance:           45.0 cm
        AoA azimuth:        -30.9375 deg
        AoA az. FOM:        88.0 %
        AoA elevation:      0.0 deg
        AoA elev. FOM:      0.0
        AoA dest azimuth:   10.375 deg
        AoA dest az. FOM:   92.0 %
        AoA dest elevation: 0.0 deg
        AoA dest elev. FOM: 0.0 %
        slot in error:      0
        rssi:               -63.0 dB
    # Measurement 2:
        status:             Ok (0x0)
        mac address:        02.00
        is nlos meas:       no
        distance:           22.0 cm
        AoA azimuth:        -66.578125 deg
        AoA az. FOM:        92.0 %
        AoA elevation:      0.0 deg
        AoA elev. FOM:      0.0
        AoA dest azimuth:   0.0 deg
        AoA dest az. FOM:   0.0 %
        AoA dest elevation: 0.0 deg
        AoA dest elev. FOM: 0.0 %
        slot in error:      0
        rssi:               -35.5 dB

[...]

Stopping ranging...
Deinitializing session...
Session 42 DeInit (StateChangeWithSessionManagementCommands)
Device Ready
Ok
```

## How to Calibrate PDoA Offset Using 2 facing DUT

Send default appropriate cal:

```
send_calset ranging_evb_pdoa_1_azimut
(Warning: your default calibration and configuration will be erased)
in above parameter set, lut index 0 is mapped to channel 5 axis x
```

Set an 'identity' AoaLut at responder side:

```
set_cal pdoa_lut0.data 'identity'
```

Start a responder ranging from one shell:

```
run_fira_twr -c 5 -m ss -s 38 -p <port> --controlee
```

Start an initiator ranging from another shell:

```
run_fira_twr -c 5 -m ss -s 38 -p <port> --stats
```

Write down the aoa (so pdoa...) value <x> for this 0 deg alignment
Set it back to the reponser in radian:

```
set_cal ant0.ch5.pdoa.axisx.offset $(python -c 'print(<x>*3.14/180)')
```

Load a proper AOA to PDOA LUT to the responder:

```
set_cal pdoa_lut0.data theory channel=5 antenna_dist_mm=20.8
```

## How to Perform a Contention based Fira TWR Between 2 DUTs

Use 2 different shells, one for each EVB called here-after initiator and responder.

In each shell:
Identify your device connection & export the associated com port:

```
export UQT_PORT=/dev/ttyUSB0
```

Send some default calibration values:
(Warning: your default calibration and configuration will be erased)

```
send_calset rangingranging_evb_pdoa_5
```

In 'shell responder, start the device as a responder:

```
run_fira_twr -t 1 --schedule=contention --node=onetomany --round-ctrl="cm|rcp" --round=ss --frame sp1 -t -1 --controlee
```

In shell initiator, start the device as an initiator:

```
run_fira_twr -t 1 --schedule=contention --node=onetomany --round-ctrl="cm|rcp" --round=ss --frame sp1 -t -1
```

Expected output from responder as below,  when initiator is running:

```
...
# Ranging Data:
    session id:         42
    sequence n:         38
    ranging interval:   200 ms
    measurement type:   Twr
    primary session id: 0x0
    n of measurement:   1
    # Measurement 1:
        status:             Ok (0x0)
        mac address:        00:01 hex
        is nlos meas:       no
        distance:           1.0 cm
        AoA azimuth:        9.9453125 deg
        AoA az. FOM:        92.0 %
        AoA elevation:      8.5546875 deg
...
```

## How to run FiRa TWR between 2 DUTs with Provisioned STS with Responder specific Sub-Session Key

Initialize a TWR FiRa session as controller with all required parameters on the first board:

```
run_fira_twr -p COM37 --mac 00:0A --node onetomany --sts provisioned-key
--skey "F1221354652697189900116234236422"
--controlees-with-sskey "[0x0B, 0xC, '00:01:02:03:04:05:06:07:08:09:0A:0B:0C:0D:0E:0F']"
```

Open a second command shell in the python script location.

Initialize a TWR FiRa session as controlee with all required parameters on the second board:

```
run_fira_twr -p COM40 --controlee --mac 00:0B --dest-mac "['00:0A']" --node onetomany --sts provisioned-key
--skey "F1221354652697189900116234236422" --ssession 0xC --sskey "000102030405060708090A0B0C0D0E0F"
```

Output of the script running on controller side:

```
Initializing session 42...
Session 6 -> Init (StateChangeWithSessionManagementCommands)
Using Fira 2.0 session handle is : 6
Setting session 6 config ...
    DeviceType (0x0):                   0x1
    DeviceRole (0x11):                  0x1
    MultiNodeMode (0x3):                0x1
    RangingRoundUsage (0x1):            0x2
    DeviceMacAddress (0x6):             [0xa, 0x0]
    ChannelNumber (0x4):                0x9
    ScheduleMode (0x22):                0x1
    StsConfig (0x2):                    0x4
    RframeConfig (0x12):                0x3
    ResultReportConfig (0x2e):          0xb
    VendorId (0x27):                    [7, 8]
    StaticStsIv (0x28):                 [1, 2, 3, 4, 5, 6]
    AoaResultReq (0xd):                 0x1
    UwbInitiationTime (0x2b):           0x0
    PreambleCodeIndex (0x14):           0xa
    SfdId (0x15):                       0x2
    SlotDuration (0x8):                 0x960
    RangingInterval (0x9):              0xc8
    SlotsPerRr (0x1b):                  0x19
    MaxNumberOfMeasurements (0x32):     0x0
    HoppingMode (0x2c):                 0x0
    RssiReporting (0x13):               0x0
    BlockStrideLength (0x2d):           0x0
    NumberOfControlees (0x5):           0x1
    DstMacAddress (0x7):                [1]
    KeyRotationRate (0x24):             0x0
    SessionKey (0x45):                  f1.22.13.54.65.26.97.18.99.00.11.62.34.23.64.22
    StsLength (0x35):                   0x1
Session 6 -> Idle (StateChangeWithSessionManagementCommands)
Updating the multicast list of controlees...
session_update_multicast_list: Ok (0).
Starting ranging...
Device -> Active
Session 6 -> Active (StateChangeWithSessionManagementCommands)
Press <RETURN> to stop
# Ranging Data:
        session handle:         6
        sequence n:         0
        ranging interval:   200 ms
        measurement type:   Twr
        Mac add size:       2
        primary session id: 0x0
        n of measurement:   1
        # Measurement 1:
            status:             Ok (0x0)
            mac address:        00:0b hex
            is nlos meas:       Unknown
            distance:           42.0 cm
            AoA azimuth:        0.0 deg
            AoA az. FOM:        0.0 %
            AoA elevation:      0.0 deg
            AoA elev. FOM:      0.0
            AoA dest azimuth:   0.0 deg
            AoA dest az. FOM:   0.0 %
            AoA dest elevation: 0.0 deg
            AoA dest elev. FOM: 0.0 %
            slot in error:      0
            rssi:               -0.0 dBm
```

Get more informations on possible options:

```
run_fira_twr -h
```
