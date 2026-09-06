# run_fira_test_periodic_tx

Script **run_fira_test_periodic_tx** is provided to demonstrate FiRa Periodic TX Test. In the Periodic TX Test mode, the UWBS continues to send UWB packets until a configured number of packets (NUM_PACKETS) have been sent. This test mode may be used to measure characteristics like signal power, bandwidth and IEEE spectral mask of the transmitted signal.

## Parameters

Arguments with expected parameter available in this script:

| Parameter                | Values                                                                        |
|--------------------------|-------------------------------------------------------------------------------|
| -t / --time              | set the duration of the ranging session (in second); `-1` - range forever     |
| -p / --port              | Specify communication interface                                               |
| -c / --channel           | Refer to CHANNEL_NUMBER in ``FIRA UCI Technical Specification``               |
| -i / --input-file        | recover test configuration from a test profile file                           |
| -v / --verbose           | prints additional debug information                                           |
| --randomized-psdu        | See RANDOMIZED_PSDU (default 0)                                               |
| --preamble-code-index    | Refer to PREAMBLE_CODE_INDEX in ``FIRA UCI Technical Specification``          |
| --sfd-id                 | Refer to SFD_ID in ``FIRA UCI Technical Specification``                       |
| --rf-frame-config        | See RF_FRAME_CONFIG (default 0)                                               |
| --psdu-data-rate         | Refer to PSDU_DATA_RATE in ``FIRA UCI Technical Specification``               |
| --preamble-duration      | Refer to PREAMBLE_DURATION in ``FIRA UCI Technical Specification``            |
| --nb-sts-segments        | Refer to NUMBER_OF_STS_SEGMENTS in ``FIRA UCI Technical Specification``       |
| --psdu                   | ':' or '.' separated list of bytes                                            |
| --prf-mode               | Refer to PRF_MODE in ``FIRA UCI Technical Specification``                     |
| --num-packets            | No. of packets (default = 1000)                                               |
| --gap                    | See T_GAP, Gap between start of one packet to the next in us (default = 2000) |

## Application

Refer to "Application Configuration Parameters" in ``FIRA UCI Technical Specification`` for the possible values on APP configuration parameter.

```{eval-rst}
+----------------+-------------+------+------------------------------------------------------------------------------------------------------+---------+
| Parameter Name | Len (octet) | ID   | Description                                                                                          | Default |
+================+=============+======+======================================================================================================+=========+
| NUM_PACKETS    | 4           | 0x00 | No. of packets                                                                                       | 1000    |
+----------------+-------------+------+------------------------------------------------------------------------------------------------------+---------+
| T_GAP          | 4           | 0x01 | Gap between start of one packet to the next in us.                                                   | 2000    |
+----------------+-------------+------+------------------------------------------------------------------------------------------------------+---------+
| RANDOMIZE_PSDU | 1           | 0x04 | 0 - No randomization                                                                                 | 0       |
|                |             |      |                                                                                                      |         |
|                |             |      | 1 - Take first byte of data supplied by command and it shall be used as a seed for randomizing PSDU  |         |
+----------------+-------------+------+------------------------------------------------------------------------------------------------------+---------+
```

The periodic TX Test shall be triggered by using TEST_PERIODIC_TX_CMD command.

The TEST_PERIODIC_TX_CMD command SHALL be issued only after applying all required configuration parameters and the “Device Test Mode” session shall be in SESSION_STATE_IDLE Session State, otherwise UWBS SHALL respond TEST_PERIODIC_TX_RSP with Status of STATUS_ERROR_SESSION_NOT_CONFIGURED indicating that Test Session is not configured.

The UWBS shall respond TEST_PERIODIC_TX_RSP with Status of STATUS_OK and starts Periodic Test. The UWBS SHALL periodically starts sending UWB packet with PSDU Data as a payload. The periodicity is configured by T_GAP Test Configuration Parameter and number of packets to be transferred is configured by NUM_PACKETS APP Configuration Parameter.

The UWBS shall notify TEST_PERIODIC_TX_NTF notification with Status of STATUS_OK after NUM_PACKETS packets are sent over UWB to the intended destination UWB device.

**TEST_PERIODIC_TX_CMD**

| Payload Field(s) | Size (octet)    | Description                                                                    |
|------------------|-----------------|--------------------------------------------------------------------------------|
| PSDU Data        | N Octets        | PSDU Data[0:N] bytes; <br>0 <= N <= 127 for BPRF; <br>0 <= N <= 4095 for HPRF. |

**TEST_PERIODIC_TX_RSP**

| Payload Field(s) | Size (octet) | Description                         |
|------------------|--------------|-------------------------------------|
| Status           | 1            | Status code as per [FIRA_UCI_SPEC]. |

**TEST_PERIODIC_TX_NTF**

| Payload Field(s) | Size (octet) | Description                         |
|------------------|--------------|-------------------------------------|
| Status           | 1            | Status code as per [FIRA_UCI_SPEC]. |
