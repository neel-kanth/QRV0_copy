# get_config

This script retrieves the device configuration; it can be used to display
firmware debug traces configuration.

## Parameters

Arguments with expected parameter available in this script:

| Parameter       | Values                                           |
|-----------------|--------------------------------------------------|
| -p / --port     | Specify communication interface                  |
| -l / --list     | list available param names and their spec        |
| -v / --verbose  | prints additional debug information              |
| -b / --as-bytes | show the key value as a byte stream              |
| -x / --as-hex   | show the key value in hex format                 |
| -r / --as-repr  | show the key value in format used to set it back |

## Example

Execute the script using the port available in your system.

### Using `dev/uci0` port
```
get_config -p /dev/uci0
State                = DeviceState.Ready
LowPowerMode         = 1
ChannelNumber        = 0
PmMinInactivityS4    = Failed
```

### Using `ftdi://FT4222` port
```
get_config -p ftdi://FT4222
State                = DeviceState.Ready
LowPowerMode         = 0
ChannelNumber        = 0
PmMinInactivityS4    = 0
```
