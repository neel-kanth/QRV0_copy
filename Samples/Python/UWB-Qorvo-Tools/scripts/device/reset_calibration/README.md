# reset_calibration

This script resets the DUT's calibration parameters to their default values. In this example, device connected to ftdi://FT4222/9265.

## Parameters

Arguments with expected parameter available in this script:

| Parameter      | Values                                  |
|----------------|-----------------------------------------|
| -p / --port    | Specify communication interface         |
| -v / --verbose | prints additional debug information     |
| --timeout      | time in second until the script timeout |

## Example

```
reset_calibration -p ftdi://FT4222/9265 --timeout 4
```
