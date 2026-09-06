# run_fira_owr

This script demonstrate One Way Ranging (OWR) use case.

## How to Perform a Fira One Way Ranging Between 2 DUTs

```
# Use 2 different shells, one for each EVB called here-after
  observer and advertiser.
# In each shell:
    Identify your device connection & export the associated com port:
       export UQT_PORT=/dev/ttyUSB0
    Send some default calibration values:
    (Warning: your default calibration and configuration will be erased)
        send_calset ranging
# In 'shell responder, start the device as an observer:
  run_fira_owr -c 5 --observer
# In 'shell initiator, start the device as an advertiser:
  run_fira_owr -c 5
# Expected output from responder as below,  when initiator is running:
    ...
    # Ranging Data:
        session id:       42
        sequence n:       115
        ranging interval: 200 ms
        # Measurement 1:
            status:             Ok (0x0)
            Frame sequence num: 3
            Bloc idx:           47
            AoA azimuth:        -89.9921875 deg
            AoA az. FOM:        0.0 %
            AoA elevation:      0.0 deg
            AoA elev. FOM:      0.0
    ...
```
