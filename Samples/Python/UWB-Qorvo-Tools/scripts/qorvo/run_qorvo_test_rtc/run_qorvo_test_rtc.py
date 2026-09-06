#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

import argparse
import logging
import time
import os

from uci import Client, Gid, OidQorvo, Status, UciComError
from uci import NotImplementedData, RtcTestOutput

# Below hack sometimes required when operating on windows git-bash/msys2
import sys

from uqt_utils.utils import uqt_errno

sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description="Run Qorvo test RTC.")
    parser.add_argument(
        "--description",
        action="store_true",
        help="show short description of the script",
    )
    parser.add_argument(
        "-p",
        "--port",
        type=str,
        help="serial port used. (default: %(default)s)",
        default=os.getenv("UQT_PORT", "/dev/ttyUSB0"),
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="use logging.DEBUG level. (default: %(default)s)",
        default=False,
    )
    parser.add_argument(
        "-g",
        "--gpio-on-time",
        type=int,
        help="GPIO de-assertion time (ms). (default: %(default)s)",
        default=1000,
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
    log = logging.getLogger()

    notif_handlers = {
        (Gid.Qorvo, OidQorvo.TestRtc): lambda x: print(RtcTestOutput(x)),
        ("default", "default"): lambda gid, oid, x: print(
            f"Warning: Unexpected notification: {NotImplementedData(gid, oid, x)}"
        ),
    }

    while True:
        try:
            client = None
            client = Client(
                port=args.port,
                notif_handlers=notif_handlers,
                qtraces_logfile=args.qtraces_logfile,
            )

            print("Starting RTC Test...")
            rts = client.test_rtc(args.gpio_on_time)
            if rts != Status.Ok:
                print(f"test_rtc failed: {rts.name} ({rts}).")
                break

            time.sleep(1 + int(args.gpio_on_time / 1000))

            print("Stopping RTC Test...")

            break

        except UciComError as e:
            rts = e.n
            log.critical(f"{e}")
            break

    if client:
        client.close()
    if rts == Status.Ok:
        print("Ok")
    sys.exit(uqt_errno(rts))


if __name__ == "__main__":
    main()
