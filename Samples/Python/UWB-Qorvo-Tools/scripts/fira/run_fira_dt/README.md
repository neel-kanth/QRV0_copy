# run_fira_dt

This script demonstrate Downlink Time Difference of Arrival (DL-TDoA) use case.

## How to Set-Up a DL-TDoA Use-case Using 2 DUTs

This is a non-real life use case, but this allow to validate, for instance, the
control messages received by the tag.
```
# Use 2 different shells, one for each EVB called here-after
  anchor-init, and tag.
# In each shell:
    Identify your device connection & export the associated com port:
       export UQT_PORT=...
# In shell 'anchor-init': start an initiator which is advertising 4 anchors:
  run_fira_dt -t -1 --role=dt-anchor --mac="00:a1" --role-per-round="[(0,init)]" \
        --dest-per-slot="[ (0, [(0xa2,),(0xa3,),(0xa4,)]) ]" --is-time-ref
# In shell 'tag': start the device as an active tag in round 0:
    run_fira_dt -t -1 --role dt-tag     --mac="00:b1"  --activity="[0]"
# Expected output: 5 measurement messages expected:
    # Ranging Data:
        session id:         42
        sequence n:         8
        ranging interval:   200 ms
        measurement type:   OwrDltdoa
        ...
        n of measurement:   5
        # Measurement 1:
            status:             Ok (0x0)
            mac address:        00:a1 hex
            ...
        # Measurement 2:
            status:             RangingRxTimeout (0x21)
            ...
        ...
        # Measurement 5:
            status:             RangingRxTimeout (0x21)
            ...
    ...
```

## How to Set-Up a DL-TDoA Use-case Using 3 DUTs
This is the minimum set-up to operate the DL-TDoA use-case


```
# Use 3 different shells, one for each EVB called here-after
  anchor-init, anchor-resp, and tag.
# In each shell:
    Identify your device connection & export the associated com port:
       export UQT_PORT=...
# In shell 'anchor-init': start the device as an initiator anchor in round 0:
  run_fira_dt -t -1 --role=dt-anchor  --mac="12:34" --role-per-round="[(0,init)]" \
          --dest-per-slot="[ (0, [(0xabcd,), ]) ]" --is-time-ref
# In shell 'anchor-resp': start the device as a responder Anchor in round 0:
  run_fira_dt -t -1 --role dt-anchor  --mac="ab:cd" --role-per-round "[(0,resp)]"
# Expected output: both Anchors are expected to range between each-other:
    ...
    # Ranging Data:
        session id:         42
        sequence n:         821
        ranging interval:   200 ms
        measurement type:   Twr
        Mac add size:       2
        primary session id: 0x0
        n of measurement:   1
        # Measurement 1:
            status:             Ok (0x0)
    ...
# In shell 'tag': start the device as an active tag in round 0:
    run_fira_dt -t -1 --role dt-tag     --mac="00:b1"  --activity="[0]"
# Expected output: Tag should range with both mac address identified anchors:
    # Ranging Data:
        session id:         42
        sequence n:         8
        ranging interval:   200 ms
        measurement type:   OwrDltdoa
        Mac add size:       2
        primary session id: 0x0
        n of measurement:   3
        # Measurement 1:
            status:             Ok (0x0)
            mac address:        12:34 hex
            ...
        # Measurement 2:
            status:             Ok (0x0)
            mac address:        ab:cd hex
            ...
    ...
```

## How to Set-Up a DL-TDoA Use-case Using 4 DUTs

Use the same kind of set-up as above, sending below commands to DUTs.

First use-case: only operate on round 0.
```
anchor init:  run_fira_dt -t -1 --role=dt-anchor --mac="00:a1" \
    --role-per-round="[(0,init)]" --dest-per-slot="[ (0, [(0xa2,), (0xa3,)]) ]" \
    --is-time-ref
anchor resp1: run_fira_dt -t -1 --role dt-anchor --mac="00:a2" \
    --role-per-round "[(0,resp)]"
anchor resp2: run_fira_dt -t -1 --role dt-anchor --mac="00:a3" \
   --role-per-round "[(0,resp)]"
tag:          run_fira_dt -t -1 --role dt-tag    --mac="00:b1" \
   --activity="[0]"
```

Second use-case: operate on round 0 & 1:
```
anchor init:  run_fira_dt -t -1 --role=dt-anchor --mac="00:a1" \
    --role-per-round="[(0,init), (1,init)]" \
    --dest-per-slot="[ (0, [(0xa2,)]), (1, [(0xa3,)]) ]" \
    --is-time-ref
anchor resp1: run_fira_dt -t -1 --role dt-anchor --mac="00:a2" \
    --role-per-round "[(0,resp)]"
anchor resp2: run_fira_dt -t -1 --role dt-anchor --mac="00:a3" \
    --role-per-round "[(1,resp)]"
tag:          run_fira_dt -t -1 --role dt-tag    --mac="00:b1" \
    --activity="[0, 1]"
```
