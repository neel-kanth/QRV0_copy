# Radar Introduction

## Basic principles

The **radar** directory introduces radar functionality of QM35 chip.

The QM35 can simultaneously transmit and receive frames.
This feature enables the use of the UWB Radar.

While the QM35 is transmitting a frame, its RX path is activated and
listens to the echoes from the environment. The computed output is
`Channel Impulse Response Estimation` (CIR). One CIR is produced per each Sweep.
Multiple Sweeps can be run in a series called burst. Timing between bursts and
sweeps are adjustable using parameters. Various post processing algorithms are
possible to further process CIR output.

Basic principle of UWB Radar:

1. The UWB chip syntheses a UWB pulse.
2. This pulse is transmitted over the air by the RF and Antenna part.
3. The pulse is reflected by the objects in the environment. Note, there
is a direct transmission of the pulse from the TX antenna to the RX one as well.
4. All reflections are received by the RX antenna.
5. The `Channel Impulse Response` (CIR) is estimated by the chip.

## Radar Parameters

Radar on QM35 chip can be parametrized through series of
`UWB Command Interface (UCI)` parameters as described in the table.


| Field                        | Description                                 |
|------------------------------|---------------------------------------------|
|``ChannelNumber``             |RF channel to be used. Supported values are: |
|                              | - 5 for 6489.6 MHz                          |
|                              | - 9 for 7987.2 MHz (Default)                |
+------------------------------+---------------------------------------------+
|``RframeConfig ``             | - 0x00: SP0                                 |
|                              | - 0x01: SP1                                 |
|                              | - 0x02: RFU                                 |
|                              | - 0x03: SP3 (Default)                       |
+------------------------------+---------------------------------------------+
|``PreambleCodeIndex``         |The device default preamble code             |
+------------------------------+---------------------------------------------+
|``PreambleDuration``          | - 0x00: 32 Symbols                          |
|                              | - 0x01: 64 Symbols (Default)                |
|                              | - 0x02: 128 Symbols                         |
|                              | - 0x02: 256 Symbols                         |
|                              | - 0x02: 512 Symbols                         |
|                              | - 0x02: 1024 Symbols                        |
|                              | - 0x02: 2048 Symbols                        |
|                              | - 0x02: 4096 Symbols                        |
+------------------------------+---------------------------------------------+
|``SessionPriority ``          |Value between 1 and 100. (Default 50)        |
+------------------------------+---------------------------------------------+
|``TimingParams``              |Radar Timing parameters.                     |
|                              | - `burst_period_ms`:                        |
|                              |   Duration between the start of two         |
|                              |   consecutive radar bursts in ms.           |
|                              | - `sweep_period_rstu`:                      |
|                              |   Duration between the start of two         |
|                              |   consecutive radar sweeps in ms.           |
|                              | - `sweeps_per_burst`:                       |
|                              |   Number of radar sweeps within the burst.  |
+------------------------------+---------------------------------------------+
|``SamplesPerSweep``           |Number of samples per sweep.                 |
|                              |Possible values are {1 to 255}               |
|                              |(Default = 64)                               |
+------------------------------+---------------------------------------------+
|``SweepOffset``               |Number of samples offset before First Path.  |
|                              |Possible values are {-32768 to 32767}        |
|                              |(Default = -10)                              |
+------------------------------+---------------------------------------------+
|``BitsPerSample``             | - 0x00: 32 bits per sample                  |
|                              | - 0x01: 48 bits per sample (default)        |
|                              | - 0x02: 64 bits per sample                  |
+------------------------------+---------------------------------------------+
|``NumberOfBursts``            |Configuration parameter to set maximum number|
|                              |of radar bursts to be executed in a session. |
|                              |The session is stopped when                  |
|                              |configured radar bursts are elapsed.         |
|                              |0x00 = Unlimited (Default)                   |
+------------------------------+---------------------------------------------+
|``RadarDataType``             |Type of radar data in data report:           |
|                              | - 0x00: Radar Sweep Samples (Default)       |
|                              | - 0x01 – 0xFF: RFU                          |
+------------------------------+---------------------------------------------+
|``AnntennaSetId``             |Antenna set ID used for Radar.               |
|                              |Possible values are {0 to 3} (Default: 3)    |
+------------------------------+---------------------------------------------+
|``TxProfileIdx``              |Radar Tx profile index:                      |
|                              | - 0x00 = TX_PROFILE_HIGH (Default)          |
|                              | - 0x01 = TX_PROFILE_LOW                     |
+------------------------------+---------------------------------------------+

## Parametrization tips

Current Radar demo scripts utilize **single sweep per burst**. Most meaningful parameter
in this case is ``burst_period_ms`` which is acting as sampling period.

**Important:** Setting ``burst_period_ms`` too low could overflow the transport medium
and cause Radar measurments drop.

The next significant parameter is ``PreambleDuration``. It controls how long the Radar
packet's preamble is, thus increasing the range and accuracy of the measurements.

**Important:** UWB Stack will prevent of using conflicting combinations of
``PreambleDuration`` and ``burst_period_ms`` as there are cases when Preamble Duration,
with the needed overhead for frame porcessing, could exceed Burst Period.

User can select the window of samples to be taken from the QM35. It saves the bandwidth
of transport medium as it could be overflown in certain combination of parameters.
``SamplesPerSweep`` is amount of samples (taps) per single measurement. Single tap covers
range of approx. 15cm, so effectively ``SamplesPerSweep`` selects the distance of measurement.
``SweepOffset`` is the offset of samples in relation to First Path. Together with
``SamplesPerSweep`` it is used to select window of samples from sweeps.

### Timing Params limitations

The global Radar processing chain is made of the following elements:

* The QM35 UWB chip in itself;
* The frame processor inside the chip;
* The HSSPI protocol;
* The SPI bus transfer rate and the used adapter;
* The provided demonstration Python script.

When Timing Params are set too tight the system cannot be able to run.
The rate of receiving Radar Data Messages is depending on ``burst_period_ms`` but also on the amount of data to be transferred
in one message (``sweeps_per_burst`` and ``SamplesPerSweep``). The performance is limited by the slowest part of this
processing chain. To increase the performances, it is important to identify first the bottleneck.

The minimal achievable ``burst_period_ms`` is currently measured to 7 ms. This can be done with the following setup:

* HSSPI transport by using onboard FT4222 chip or external UMFT4222 USB to SPI adapter;
* Set the ``SamplesPerSweep`` per CIR to 64 taps (default).
* Set the ``sweeps_per_burst`` to 1.

The bottleneck currently comes from the Python script together with the FT4222 adapter (especially its Python interface). The provided
Python script is multi-threaded, but it is important to know that Python threads are
limited by the Python Global Interpreter Lock, preventing the use of more than 1 CPU core. Moreover, the underlying OS (Linux/Windows)
is not a real time OS. So, when the ``burst_period_ms`` becomes below 10ms, the timings are close to the host OS tick timer, and then jitter and
scheduling may lead to frame lost.

Another limitation comes from the realtime plotting of the CIR. So, in order to get the best ``burst_period_ms`` possible, the realtime plotting shall
be disabled (with the ``--plot static`` option).

Moreover, the ``SamplesPerSweep`` per CIR have an impact on the achievable Radar Date Message reception rate. Increasing this value will increase
the amount of data to be transferred on the SPI bus and on the chip's memory, so it will necessary increase the minimal achievable ``burst_period_ms``.
On the contrary, decreasing the samples per sweep number per CIR will allow to reduce the ``burst_period_ms``. In addition when multiple sweeps per burst
are used (``sweeps_per_burst`` different than 1) data from all the sweeps are send in one Radar Data Message so best performance is possible with this
setting set to 1.
