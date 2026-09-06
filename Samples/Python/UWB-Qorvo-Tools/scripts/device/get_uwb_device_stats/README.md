# get_uwb_device_stats

This script retrieves the device statistics, such as current chip temperature.

## Parameters

Arguments with expected parameter available in this script:

| Parameter      | Values                                       |
|----------------|----------------------------------------------|
| -p / --port    | Specify communication interface              |
| -r / --refresh | refresh time in seconds to request the stats |
| -t / --time    | duration of test in seconds                  |
| -v / --verbose | prints additional debug information          |

## Example

```
get_uwb_device_stats -p ftdi://FT4222/12385
status: Ok (0)
chip_temperature: 20.22°C
```
