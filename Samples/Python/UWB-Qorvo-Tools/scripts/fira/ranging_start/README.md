# ranging_start

Start FiRa ranging

## Parameters

Arguments with expected parameter available in this script:

| Parameter         | Values                                                                        |
|-------------------|-------------------------------------------------------------------------------|
| -t / --time       | set the duration of the ranging session (in second); `-1` - range forever     |
| -p / --port       | Specify communication interface                                               |
| -c / --channel    | Refer to CHANNEL_NUMBER in ``FIRA UCI Technical Specification``               |
| -s / --session_id | set session ID (default: 42)                                                  |

## Example usage

```
ranging_start -p /dev/ttyUSB1 -s 1 -t 30
```
