# reset_device

This script resets the DUT by sending FiRa UCI Command CORE_DEVICE_RESET_CMD. In this example, device connected to ftdi://FT4222/9265.

## Parameters

Arguments with expected parameter available in this script:

| Parameter      | Values                              |
|----------------|-------------------------------------|
| -p / --port    | Specify communication interface     |
| -v / --verbose | prints additional debug information |

## Example

```
reset_device -p ftdi://FT4222/9265
```
