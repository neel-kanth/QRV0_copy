# stop_fira_twr

Request to stop FiRa session.

## Parameters

Arguments with expected parameter available in this script:

| Parameter             | Values                                        |
|-----------------------|-----------------------------------------------|
| -p / --port           | Specify communication interface               |
| -v / --verbose        | Prints additional debug information           |
| -s / --session_handle | Handle of the session requested to be stopped |
| --session-key         | Key to use for secured ranging                |


## Example

```
stop_fira_twr p <port> -s <session_handle>
```

Output:
```
[...]
Stopping ranging...
[...]
SessionState.Active
# Stopping session:
     Ok
```
