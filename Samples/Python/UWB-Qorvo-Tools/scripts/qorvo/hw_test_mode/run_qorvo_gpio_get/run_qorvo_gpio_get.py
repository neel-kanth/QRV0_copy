#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

import argparse
import logging
import os
import sys

from uci import Client, Status, UciComError, GpioId

# Below hack sometimes required when operating on windows git-bash/msys2

from uqt_utils.utils import uqt_errno

sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(
        description="Run Qorvo GPIO get.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--description",
        action="store_true",
        help="show short description of the script",
    )
    parser.add_argument(
        "-p",
        "--port",
        type=str,
        default=os.getenv("UQT_PORT", "/dev/ttyUSB0"),
        help="serial port used)",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        default=False,
        help="use logging.DEBUG level",
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
    parser.add_argument(
        "--gpio-id",
        type=int,
        help="GPIO ID",
    )
    args = parser.parse_args()

    if args.description:
        print(parser.description)
        sys.exit(0)
    if not args.gpio_id:
        sys.exit("Missing required gpio-id argument")

    log = logging.getLogger()
    if args.verbose:
        log.setLevel(logging.DEBUG)

    try:
        with Client(
            port=args.port,
            qtraces_logfile=args.qtraces_logfile,
        ) as client:
            rts, state = client.gpio_get(GpioId(args.gpio_id))
            assert rts == Status.Ok, f"gpio_get failed: {rts.name} ({rts})"
    except UciComError as e:
        rts = e.n
        log.critical(f"{e}")
        sys.exit(uqt_errno(rts))

    print(f"{state}")
    sys.exit(uqt_errno(rts))


if __name__ == "__main__":
    main()
