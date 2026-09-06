# HW Test Mode

## GPIOs

### Configure GPIO

To configure GPIO, use `run_qorvo_gpio_configure` command:

```shell
$ run_qorvo_gpio_configure --help
usage: run_qorvo_gpio_configure [-h] [--description] [-p PORT] [-v] [--output] [--input] [--flags FLAGS]
                                [--qtraces-logfile LOG_PATH] --gpio-id GPIO_ID

Run Qorvo GPIO configure.

options:
  -h, --help            show this help message and exit
  --description         show short description of the script (default: False)
  -p PORT, --port PORT  serial port used) (default: /dev/ttyUSB0)
  -v, --verbose         use logging.DEBUG level (default: False)
  --output              define GPIO as output (default: False)
  --input               define GPIO as input (default: False)
  --flags FLAGS         flags to apply (default: 0)
  --qtraces-logfile LOG_PATH
                        Specifies a directory or file to save raw, undecoded Qtraces, applicable only in ft4222 mode.If a
                        directory is provided, logs will be saved in it with a timestamped filenameIf a full file path is
                        provided, the file will be overwritten.If no '.bin' extension is present, it will be automatically
                        appended. (default: None)
  --gpio-id GPIO_ID     GPIO ID (default: None)
$
```

`flags` is a bit field:

- for an output:
  - bit 0: 1 = active high, 0 = active low
  * bit 1: 1 = active at configuration, 0 = inactive at configuration
- for an output:
  - bit 0: 1 = enable pull, 0 = disable pull
  * bit 1: 1 = pull up, 0 = pull down

For examples:

- to configure PIN_6 as active high output, inactive when configured:

```shell
$ run_qorvo_gpio_configure -v --output --flags 1 --gpio-id 6
15:43:03.479 DEBUG:     send: 2b.2a.00.04.06.01.01.00
15:43:03.486 DEBUG:     data_recv: 4b.2a.00.01.00
15:43:03.486 DEBUG:     packet_recv: 4b.2a.00.01.00
OK
$
```

- to configure PIN_6 as input with pull down:

```shell
$ run_qorvo_gpio_configure -v --input --flags 1 --gpio-id 6
15:42:40.484 DEBUG:     send: 2b.2a.00.04.06.00.01.00
15:42:40.493 DEBUG:     data_recv: 4b.2a.00.01.00
15:42:40.493 DEBUG:     packet_recv: 4b.2a.00.01.00
OK
$
```

### Set output GPIO

Configure GPIO as output thanks to the `run_qorvo_gpio_configure` command.\
Then to set the GPIO value based on active state (low or high):

```shell
$ run_qorvo_gpio_set -v --active --gpio-id 6
15:44:25.564 DEBUG:     send: 2b.2b.00.02.06.01
15:44:25.574 DEBUG:     data_recv: 4b.2b.00.01.00
15:44:25.574 DEBUG:     packet_recv: 4b.2b.00.01.00
OK
$ run_qorvo_gpio_set -v --inactive --gpio-id 6
15:44:39.080 DEBUG:     send: 2b.2b.00.02.06.00
15:44:39.089 DEBUG:     data_recv: 4b.2b.00.01.00
15:44:39.089 DEBUG:     packet_recv: 4b.2b.00.01.00
OK
$
```

### Get output GPIO

Configure GPIO as output or input thanks to the `run_qorvo_gpio_configure` command.\
Then to get the GPIO value (for output, returned value depend on active state: low or high):

For example, PIN_6 configured as active high output (inactive at configuration):

```shell
$ run_qorvo_gpio_configure -v --output --flags 1 --gpio-id 6
15:45:00.613 DEBUG:     send: 2b.2a.00.04.06.01.01.00
15:45:00.621 DEBUG:     data_recv: 4b.2a.00.01.00
15:45:00.621 DEBUG:     packet_recv: 4b.2a.00.01.00
OK
$ run_qorvo_gpio_get -v --gpio-id 6
15:45:30.048 DEBUG:     send: 2b.2c.00.01.06
15:45:30.058 DEBUG:     data_recv: 4b.2c.00.02.00.00
15:45:30.058 DEBUG:     packet_recv: 4b.2c.00.02.00.00
0
$ run_qorvo_gpio_set -v --active --gpio-id 6
15:45:43.919 DEBUG:     send: 2b.2b.00.02.06.01
15:45:43.928 DEBUG:     data_recv: 4b.2b.00.01.00
15:45:43.928 DEBUG:     packet_recv: 4b.2b.00.01.00
OK
$ run_qorvo_gpio_get -v --gpio-id 6
15:45:55.510 DEBUG:     send: 2b.2c.00.01.06
15:45:55.519 DEBUG:     data_recv: 4b.2c.00.02.00.01
15:45:55.519 DEBUG:     packet_recv: 4b.2c.00.02.00.01
1
$ run_qorvo_gpio_set -v --inactive --gpio-id 6
15:46:14.623 DEBUG:     send: 2b.2b.00.02.06.00
15:46:14.632 DEBUG:     data_recv: 4b.2b.00.01.00
15:46:14.632 DEBUG:     packet_recv: 4b.2b.00.01.00
OK
$ run_qorvo_gpio_get -v --gpio-id 6
15:46:30.962 DEBUG:     send: 2b.2c.00.01.06
15:46:30.974 DEBUG:     data_recv: 4b.2c.00.02.00.00
15:46:30.974 DEBUG:     packet_recv: 4b.2c.00.02.00.00
0
$
```

For example, PIN_6 configured as input (pull up):

```shell
$ run_qorvo_gpio_configure -v --input --flags 3 --gpio-id 6
15:48:11.804 DEBUG:     send: 2b.2a.00.04.06.00.03.00
15:48:11.814 DEBUG:     data_recv: 4b.2a.00.01.00
15:48:11.814 DEBUG:     packet_recv: 4b.2a.00.01.00
OK
$ run_qorvo_gpio_get -v --gpio-id 6
15:49:15.894 DEBUG:     send: 2b.2c.00.01.06
15:49:15.904 DEBUG:     data_recv: 4b.2c.00.02.00.01
15:49:15.904 DEBUG:     packet_recv: 4b.2c.00.02.00.01
1
$
```

## Output clocks

On QM357 and QM358, only EXTON pin (33) can be used to output a clock.

### Configure clock output

Select clock to output to pin id:

```shell
$ run_qorvo_clock_out_config -v --soc qm358 --pin-id 33 --clock-id 2
16:01:02.605 DEBUG:     send: 2b.2d.00.04.02.21.00.00
16:01:02.614 DEBUG:     data_recv: 4b.2d.00.01.00
16:01:02.614 DEBUG:     packet_recv: 4b.2d.00.01.00
OK
$
```

QM357 clocks
| Clock ID | Clock Name | Mappable Pins |
|----------|-------------|---------------|
| 1 | LPOSC | EXTON |
| 2 | XTI | EXTON |
| 3 | FSOC_DIV4 | EXTON |
| 4 | RTC | EXTON |
| 5 | RC32K | EXTON |

QM358 clocks
| Clock ID | Clock Name | Mappable Pins |
|----------|-------------|---------------|
| 1 | LPOSC | EXTON |
| 2 | RTC | EXTON |
| 3 | RC32K | EXTON |

### Start/stop clock output

```shell
$ run_qorvo_clock_out_control -v --start --clock-id 2
16:04:32.803 DEBUG:     send: 2b.2e.00.02.02.01
16:04:32.813 DEBUG:     data_recv: 4b.2e.00.01.00
16:04:32.813 DEBUG:     packet_recv: 4b.2e.00.01.00
OK
$ run_qorvo_clock_out_control -v --stop --clock-id 2
16:04:57.529 DEBUG:     send: 2b.2e.00.02.02.00
16:04:57.538 DEBUG:     data_recv: 4b.2e.00.01.00
16:04:57.539 DEBUG:     packet_recv: 4b.2e.00.01.00
OK
$
```
