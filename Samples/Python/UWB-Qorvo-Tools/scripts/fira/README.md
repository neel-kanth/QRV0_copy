# FiRa Introduction

The **fira** directory introduces device ranging capabilities according to FiRa standard.

In order to know detailed description of each tool please read appropriate subchapters below or README.md files in appropriate subfolders.

## Use cases utilized multiple tools

### How to mimic run_fira_twr using low-level commands

```
# Initialize a ranging session:
  session_init -s 45 ranging
# Configure the session as a Controller TWR-one:
  session_set_conf -s 45 DeviceRole 0x1 DeviceType  0x1 MultiNodeMode 0x0  \
		RangingRoundUsage 0x2 DeviceMacAddress 0x0  DstMacAddress 0x1
# Start a forever ranging and view notifications:
  ranging_start -s 45  -t -1
# Stop the ranging_start script:
  <enter>
# Stop ranging:
  ranging_stop -s 45
# Deinitialize the session:
  session_deinit -s 45
```

### How to Set DL-TDoA Block Striding within an Active Session

```
# Initialize a ranging session with default Id and session type:
  session_init
# Configure the session as a OWR DL-TDoA TAG one:
  session_set_conf  DeviceRole 0x8 DeviceType 0x1 MultiNodeMode 0x1  RangingRoundUsage 0x5 \
         DeviceMacAddress 0xb1 ScheduleMode 0x1 RframeConfig 0x1
# Define the Tag Round activity:
  session_set_tag_activity  "[0]"
# Start a forever ranging and view notifications:
  ranging_start -t -1
# Stop the ranging_start script:
  <enter>
# Stop ranging:
  ranging_stop
# Configure Block Striding:
  session_set_conf  DlTdoaBlockStriding 1  BlockStrideLength 10
# Start again a ranging:
  ranging_start
# View notifications
  listen_to_ntf -t -1
# End use-case & clean-up
  <enter>
  ranging_stop
  session_deinit
```
