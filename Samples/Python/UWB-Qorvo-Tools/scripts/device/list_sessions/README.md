# list_sessions

This script lists all active UWB sessions run on a device.

## Parameters

Arguments with expected parameter available in this script:

| Parameter          | Values                                    |
|--------------------|-------------------------------------------|
| -p / --port        | Specify communication interface           |
| -v / --verbose     | Prints additional debug information       |


## Example

```
list_sessions -p <port>
```

Output:
```
# Active Sessions:
        status: Ok (0)
        count: 2
        # Session:
            Id:       2147483650
            Type:     Ranging (0)
            State:    Active (2)
        # Session:
            Id:       2147483649
            Type:     Ranging (0)
            State:    Active (2)
```
