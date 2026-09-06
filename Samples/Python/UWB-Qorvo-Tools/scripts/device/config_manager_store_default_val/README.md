# config_manager_store_default_val

This script allows to store default values for Config Manager keys.

## Parameters

Arguments with expected parameter available in this script:

```{eval-rst}
+-------------------+-----------------------------------------------------------------+
| Parameter         | Values                                                          |
+===================+=================================================================+
| -p / --port       | Specify communication interface                                 |
+-------------------+-----------------------------------------------------------------+
| -v / --verbose    | Prints additional debug information                             |
+-------------------+-----------------------------------------------------------------+
| -i / --input-file | Path to JSON file containing default values to store            |
+-------------------+-----------------------------------------------------------------+
| -l / --lock       | Lock after storing default values                               |
|                   |                                                                 |
|                   | Warning: If default values are locked, it will not be possible  |
|                   | to modify the default values anymore on the current device.     |
+-------------------+-----------------------------------------------------------------+
```

## Example

```
config_manager_store_default_val -i default_values.json -p COM40
```
