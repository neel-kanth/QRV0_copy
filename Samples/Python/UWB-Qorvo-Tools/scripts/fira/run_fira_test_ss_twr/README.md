# run_fira_test_ss_twr

Script **run_fira_test_ss_twr** is provided to demonstrate FiRa SS-TWR Test Mode. This mode can be used to measure single SS-TWR ToF using SP3 packets.

* Initiator transmits the POLL packet and waits for a RESPONSE packet from the responder, after SLOT_DURATION.
* Initiator then reports the time difference between the two packets as the round-trip time to AP.
* Similarly, responder waits for the POLL packet, transmits a RESPONSE packet after SLOT_DURATION and reports the time difference between the two packets as the reply time to AP. Based on these two measurements, AP can determine the ToF based on the SS-TWR formula.

## Parameters

Arguments with expected parameter available in this script:

| Parameter        | Values                                                          |
|------------------|-----------------------------------------------------------------|
| -p / --port      | Specify communication interface                                 |
| -c / --channel   | Refer to CHANNEL_NUMBER in ``FIRA UCI Technical Specification`` |
| -r / --responder | start as responder (default initiator) mode                     |
| -v / --verbose   | prints additional debug information                             |

## Application

Refer to "Application Configuration Parameters" in ``FIRA UCI Technical Specification`` for the possible values on APP configuration parameter.

**Test configuration parameters for FiRa SS-TWR Test**

| Parameter Name      | Len (octets) | ID   | Description                                                                      |
|---------------------|--------------|------|----------------------------------------------------------------------------------|
| STS_INDEX_AUTO_INCR | 1            | 0x08 | 0x00: STS_INDEX config value is used for all PER Rx/ Periodic TX test. (default) |

AP shall use TEST_SS_TWR_CMD command to trigger the SS-TWR ranging test for SP3 packets and UWBS shall respond by TEST_SS_TWR_RSP when command is accepted successfully.

The TEST_SS_TWR_CMD command shall be issued only after applying all the required configuration parameters and the “Device Test Mode” session SHALL be in SESSION_STATE_IDLE state, otherwise UWBS SHALL respond TEST_SS_TWR_RSP with Status of
STATUS_ERROR_SESSION_NOT_CONFIGURED, indicating that session is not configured. UWBS SHALL notify TEST_SS_TWR_NTF notification after completing single SS-TWR ranging measurement round involving POLL and RESPONSE packets.

**TEST_SS_TWR_CMD**

| Payload Field(s) | Size (octet) | Description                                                                    |
|------------------|--------------|--------------------------------------------------------------------------------|
| PSDU Data        | N Octets     | PSDU Data[0:N] bytes; <br>0 <= N <= 127 for BPRF; <br>0 <= N <= 4095 for HPRF. |

**TEST_SS_TWR_RSP**

| Payload Field(s) | Size (octet) | Description                         |
|------------------|--------------|-------------------------------------|
| Status           | 1            | Status code as per [FIRA_UCI_SPEC]. |

**TEST_SS_TWR_NTF**

| Payload Field(s) | Size (octet) | Description                                                                                                                                   |
|------------------|--------------|-----------------------------------------------------------------------------------------------------------------------------------------------|
| Status           | 1            | Refer to generic status codes                                                                                                                 |
| Measurement      | 4            | Contains Tround time of Initiator or Treply time of Responder depending on DEVICE_ROLE option. This is expressed in 1/(128 * 499.2Mhz) ticks. |
