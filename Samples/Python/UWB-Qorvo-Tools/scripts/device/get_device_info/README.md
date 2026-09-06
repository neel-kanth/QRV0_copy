# get_device_info

This script displays information about the device.

## Parameters

Arguments with expected parameter available in this script:

| Parameter      | Values                              |
|----------------|-------------------------------------|
| -p / --port    | Specify communication interface     |
| -v / --verbose | prints additional debug information |

## Example

Display information about the device connected to COM18:

```
$ get_device_info -p COM18
Pinging device at COM18:
# Get Device Info:
    status:              Ok (0x0)
    uci version:         2.0.0
    mac version:         2.0.0
    phy version:         2.0.0
    uci test version:    1.1.0
    QMF version:         7.4.1
    OEM version:         0.5.0
    build job:           5327001663
    soc id:              8fa025eb52ceb9c901001d00d8e3857574ed1e07ff5f77f1cdfbed7cf0a28846
    device id:           deca0430
    packaging id:        sip
```
