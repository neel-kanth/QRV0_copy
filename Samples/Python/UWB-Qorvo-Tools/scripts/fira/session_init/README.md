# session_init

Script **session_init** is provided to deinitialize and stop session

## Parameters

Arguments with expected parameter available in this script:

| Parameter      | Description                                                                                                             |
|----------------|-------------------------------------------------------------------------------------------------------------------------|
| -t / --time    | Sleep time (s) before closing the UCI traffic channel and exiting the script; `-1`  - up to key pressed (default: 0)    |
| -p / --port    | Specify communication interface                                                                                         |
| -v / --verbose | Set verbose mode                                                                                                        |
| -s / --session | Unique session id to use or a list of sessions to allow multiple session handling. <br>(default: 42)                    |

## Example usage

Ranging session example with COM24:

```
session_init -p COM24
```

Or, using HSSPI:

```
session_init -p ftdi://FT4222
```

Test session example with COM24:

```
session_init -p COM24 -s 0 --type test
```
