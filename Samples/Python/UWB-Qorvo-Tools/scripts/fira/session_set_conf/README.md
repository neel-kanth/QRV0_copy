# session_set_conf

Script **session_set_conf** is provided to set configuration to a session

## Parameters

Arguments with expected parameter available in this script:

| Parameter      | Description                                                                                                             |
|----------------|-------------------------------------------------------------------------------------------------------------------------|
| -t / --time    | Sleep time (s) before closing the UCI traffic channel and exiting the script; `-1`  - up to key pressed (default: 0)    |
| -p / --port    | Specify communication interface                                                                                         |
| -v / --verbose | Set verbose mode                                                                                                        |
| -s / --session | Unique session id to use or a list of sessions to allow multiple session handling. <br>(default: 42)                    |
| -l / --list    | List available configuration parameters                                                                                 |
| params         | Space separated <param> <value> list of parameters to set                                                               |

## Example usage

Ranging session example:

```
Example of use:
    - session_set_conf DeviceType 0 MultiNodeMode 2
    - session_set_conf -s SESSION_HANDLE StaticStsIv [0x1, 0x2, 0x3, 0x4, 0x5, 0x6]
        (The bytes are interpreted in little-endian order)
```
