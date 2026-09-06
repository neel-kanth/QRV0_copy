# session_deinit

Script **session_deinit** is provided to initialize and start session

## Parameters

Arguments with expected parameter available in this script:

| Parameter      | Description                                                                                                                             |
|----------------|-----------------------------------------------------------------------------------------------------------------------------------------|
| -t / --time    | Sleep time (s) before closing the UCI traffic channel and exiting the script; `-1`  - up to key pressed (default: 0)                    |
| -p / --port    | Specify communication interface                                                                                                         |
| -v / --verbose | Set verbose mode                                                                                                                        |
| -s / --session | Unique session id to use or a list of sessions to allow multiple session handling. <br>(default: 42)                                    |
| session_type   | Type of session to use. It can be either "ranging" or "test". Note that for test, the session id have to be 0. <br>(default: "ranging") |

## Example usage

Ranging session example with COM24:

```
session_deinit -p COM24 -s 32
```

Or, using HSSPI:

```
session_deinit -p ftdi://FT4222 -s 32
```
