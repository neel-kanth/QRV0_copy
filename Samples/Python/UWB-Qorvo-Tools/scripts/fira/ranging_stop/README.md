# ranging_stop

Stop FiRa ranging

## Parameters

Arguments with expected parameter available in this script:

| Parameter         | Values                                                                                                     |
|-------------------|------------------------------------------------------------------------------------------------------------|
| -t / --time       | sleep time (s) before closing the UCI traffic channel and exiting the script; `-1`  - up to key pressed    |
| -p / --port       | Specify communication interface                                                                            |
| -s / --session_id | set session ID (default: 42)                                                                               |

## Example usage

```
ranging_stop -p /dev/ttyUSB1 -s 1 -t 30
```
