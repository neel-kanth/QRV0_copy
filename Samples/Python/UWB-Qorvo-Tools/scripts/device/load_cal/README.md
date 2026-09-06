# load_cal

This script loads calibration values from a JSON file into the device.

## Parameters

Arguments with expected parameter available in this script:

| Parameter          | Values                                    |
|--------------------|-------------------------------------------|
| -p / --port        | Specify communication interface           |
| -f / --calibration | Calibration JSON file to be used in input |
| -v / --verbose     | prints additional debug information       |

> **Note:** Default calibration files for various setups are available in the `scripts/device/load_cal/calib_files` directory and can be used with the `--calibration` parameter.

## Example

```
load_cal -p <port> -f <calibration_file>
```

Output:
```
Device -> Ready
Setting provided calibration...

setting <parameter> to value <value>...Ok
[...]

Calibration done.
```
