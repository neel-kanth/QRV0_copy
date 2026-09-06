# l1_to_config

This script converts a configuration & calibration Json file into a default calibration Json file.

This script extracts the keys which are compatible with the default calibration framework from a
standard configuration & calibration json file.

It also translates the input key `ant_set0.tx_ant_path` into `ant_set0.tx_ant_paths` as well as its
associated value.

It explicitly adds the `ref_frame0.phy_cfg` and `ref_frame0.payload_size` keys with their default
values if they are not explicitly set in the input configuration & calibration file. This action is
done only if the `ant0.chX.ref_frame0.tx_power_index` key is set in the input file: the
tx_power_index depends on the reference frame parameters. Usually, for the reference frame 0, the
default value is kept and it is never set explicitly. As the default value is inside the FW, it
could (unlikely) change in a future FW update, making the default tx_power_index obsolete.


```
    l1_to_config -i input_calibration.json -o default_calibration.json
```
