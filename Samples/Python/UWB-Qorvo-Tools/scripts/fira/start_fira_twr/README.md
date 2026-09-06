# start_fira_twr

Script **start_fira_twr** allows to start fira two way ranging

## Parameters

Arguments with expected parameter available in this script:

| Parameter        | Values                                                                                                                |
|------------------|-----------------------------------------------------------------------------------------------------------------------|
| -t / --time      | Set the duration of the ranging session (in second); `-1` - range forever                                             |
| -p / --port      | Specify communication interface                                                                                       |
| -c / --channel   | Refer to CHANNEL_NUMBER in ``FIRA UCI Technical Specification``                                                       |
| -s / --session   | Set session ID (default: 42)                                                                                          |
| -v / --verbose   | Prints additional debug information                                                                                   |
| --role           | Refer to DEVICE_ROLE in ``FIRA UCI Technical Specification``                                                          |
| --round          | Refer to RANGING_ROUND_USAGE in ``FIRA UCI Technical Specification``                                                  |
| --round-ctrl     | Refer to RANGING_ROUND_CONTROL in ``FIRA UCI Technical Specification``                                                |
| --en-key-rot     | Refer to KEY_ROTATION in ``FIRA UCI Technical Specification``                                                         |
| --key-rot-rate   | Refer to KEY_ROTATION_RATE in ``FIRA UCI Technical Specification``                                                    |
| --sts            | Refer to STS_CONFIG in ``FIRA UCI Technical Specification``                                                           |
| --slot-span      | Refer to SLOT_DURATION in ``FIRA UCI Technical Specification``                                                        |
| --node           | Refer to MULTI_NODE_MODE in ``FIRA UCI Technical Specification``                                                      |
| --ranging-span   | Refer to RANGING_DURATION in ``FIRA UCI Technical Specification``                                                     |
| --diag-fields    | Set the Qorvo DIAGNOSTIC_FRAME_REPORTS_FIELD value OR <br>flags: metrics, aoa, cir, cfo. (default: 'metrics|aoa|cfo') |
| --meas-max       | Refer to MAX_NUMBER_OF_MEASUREMENTS in ``FIRA UCI Technical Specification``                                           |
| --skey SKEY      | Refer to SESSION_KEY in ``FIRA UCI Technical Specification``                                                          |
| --schedule       | Refer to SCHEDULE_MODE in ``FIRA UCI Technical Specification``                                                        |
| --cap-range      | Refer to CAP_SIZE_RANGE in ``FIRA UCI Technical Specification``                                                       |
| --mac            | Refer to DEVICE_MAC_ADDRESS in ``FIRA UCI Technical Specification``                                                   |
| --dest-mac       | Refer to DST_MAC_ADDRESS in ``FIRA UCI Technical Specification``                                                      |
| --frame          | Refer to RFRAME_CONFIG in ``FIRA UCI Technical Specification``                                                        |
| --ssession       | Refer to SUB_SESSION_ID in ``FIRA UCI Technical Specification``                                                       |
| --sskey          | Refer to SUB_SESSION_KEY in ``FIRA UCI Technical Specification``                                                      |
| --init-time      | Refer to UWB_INITIATION_TIME in ``FIRA UCI Technical Specification``                                                  |
| --antenna-set-id | Set the antenna set to use for the session                                                                            |
| --controlee      | Refer to DEVICE_TYPE in ``FIRA UCI Technical Specification``                                                          |
| --en-diag        | Set the Qorvo ENABLE_DIAGNOSTIC parameter to 1                                                                        |
| --en-psdu-dump   | Set the Qorvo PSDU_DUMP value to 1                                                                                    |
| --en-rssi        | Refer to RSSI_REPORTING in ``FIRA UCI Technical Specification``                                                       |

## Example usage

```
start_fira_twr -p COM21 -t -1 -c 9
```
