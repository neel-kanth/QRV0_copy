#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

import argparse
import logging
import os
import time

from uci import (
    Client,
    UciComError,
)
from uqt_utils.utils import uqt_errno
import datetime

# Below hack sometimes required when operating on windows git-bash/msys2
import sys

sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8")

epilog = """
Example of use:
 - Get UWB Device Statistics only once.
    get_uwb_device_stats.py -p /dev/ttyUSB0
 - Get UWB Device Statistics every 10 seconds for 120 seconds.
    get_uwb_device_stats.py -p /dev/ttyUSB0 -s 10 -t 120

"""


def main():
    default_port = os.getenv("UQT_PORT", "/dev/ttyUSB0")
    parser = argparse.ArgumentParser(
        description="Get UWB Device Statistics.",
        formatter_class=argparse.RawTextHelpFormatter,
        epilog=epilog,
    )
    parser.add_argument(
        "--description",
        action="store_true",
        help="show short description of the script",
        default=False,
    )
    parser.add_argument(
        "-p",
        "--port",
        type=str,
        default=default_port,
        help="communication port to use.",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        default=False,
        help="use logging.DEBUG level (default: %(default)s)",
    )
    parser.add_argument(
        "-r",
        "--refresh",
        type=int,
        default=0,
        help="refresh time in seconds to request the stats. (default: %(default)s)",
    )
    parser.add_argument(
        "-t",
        "--time",
        type=int,
        default=0,
        help="duration of test in seconds. (default: %(default)s)",
    )
    parser.add_argument(
        "--qtraces-logfile",
        type=str,
        metavar="LOG_PATH",
        help="Specifies a directory or file to save raw, undecoded Qtraces, applicable only in ft4222 mode."
        "If a directory is provided, logs will be saved in it with a timestamped filename"
        "If a full file path is provided, the file will be overwritten."
        "If no '.bin' extension is present, it will be automatically appended.",
    )

    args = parser.parse_args()

    if args.description:
        print(parser.description)
        sys.exit(0)

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    while True:
        try:
            client = None
            client = Client(port=args.port, qtraces_logfile=args.qtraces_logfile)
            # Get statistics only once
            if args.refresh == 0 or args.time == 0:
                rts, chip_temperature = client.get_uwb_device_stats()
                print(f"status: {rts.name} ({rts})")
                print(f"chip_temperature: {chip_temperature}°C")
            elif args.refresh > 0 and args.time > 0:
                # Store the statistics in a list
                steps = []
                temperatures = []
                for i in range((args.time // args.refresh) + 1):
                    rts, chip_temperature = client.get_uwb_device_stats()
                    steps.append(i * args.refresh)  # Calculate steps in seconds
                    temperatures.append(chip_temperature)
                    print(f"{steps[-1]}s: chip_temperature: {chip_temperature}°C")
                    time.sleep(args.refresh)

                # Store data into a csv
                log_name = (
                    "uwb_device_stats_"
                    f"{datetime.datetime.now().strftime('%y-%m-%d-%Hh%Mm%Ss')}.csv"
                )
                with open(log_name, "w") as f:
                    f.write("time,chip_temperature\n")
                    for i in range(len(steps)):
                        f.write(f"{steps[i]},{temperatures[i]}\n")
            else:
                print("Invalid arguments. Please check the usage or -t and -r.")
            break
        except UciComError as e:
            rts = e.n
            print(f"{e}")
            break

    if client:
        client.close()
    sys.exit(uqt_errno(rts))


if __name__ == "__main__":
    main()
