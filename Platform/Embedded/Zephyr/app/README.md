QTRACE logging
--------------

The application is able to get and forward to the host PC all received qtrace
from QM firmware using JLink RTT connection on channel 1.
To start qtrace logging, the command <qtrace -s> must be entered.

On Host PC, you must before install JLink tool package from SEGGER, available here:
https://www.segger.com/downloads/jlink/

Once tools package has been installed, you must start a log monitoring
on RTT SWD channel 1 using this command:

JLinkRTTLogger -Device <board_name> -If SWD -Speed 4000 -RTTChannel 1 <logfile_name>

For example with a nRF52832 board it could be:
JLinkRTTLogger -Device nRF52832_xxAA -If SWD -Speed 4000 -RTTChannel 1 output.log

Then you can start your application on nRF board. All received qtrace will be
stored in log file (for example: output.log)

To get human-readable qtrace, you can now use qtrace_show with the generated
log file and qtrace_strings.bin matching with used firmware.
