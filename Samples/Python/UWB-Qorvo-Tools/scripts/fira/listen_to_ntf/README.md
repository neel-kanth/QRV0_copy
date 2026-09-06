# listen_to_ntf

Script **listen_to_ntf** is provided to listen to notifications

## Parameters

Arguments with expected parameter available in this script:

| Parameter      | Description                                                                                                             |
|----------------|-------------------------------------------------------------------------------------------------------------------------|
| -t / --time    | sleep time (s) before closing the UCI traffic channel and exiting the script; `-1`  - up to key pressed (default: 5)    |
| -p / --port    | Specify communication interface                                                                                         |
| -v / --verbose | Set verbose mode                                                                                                        |

## Example usage

Example with COM24:

```
listen_to_ntf -p COM24
```

Or, using HSSPI:

```
listen_to_ntf -p ftdi://FT4222
```
