#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

import argparse
import logging
import os
import sys
from textwrap import dedent

from uci import (
    Client,
    Status,
    UciComError,
    ClockIdQm357,
    ClockIdQm358,
    ClockControl,
)

# Below hack sometimes required when operating on windows git-bash/msys2

from uqt_utils.utils import uqt_errno

sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8")


class MyFormatter(
    argparse.ArgumentDefaultsHelpFormatter,
    argparse.RawDescriptionHelpFormatter,
):
    pass


def main():
    parser = argparse.ArgumentParser(
        description="Run Qorvo Clock output control.",
        formatter_class=MyFormatter,
        epilog=dedent(
            f"""
            QM357: {", ".join([f"{clock.value} => {clock.name}" for clock in ClockIdQm357])}
            QM358: {", ".join([f"{clock.value} => {clock.name}" for clock in ClockIdQm358])}
            """,
        ),
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
        "--soc",
        type=str,
        choices=["qm357", "qm358"],
        default="qm358",
        help="Specify the clock ID to use. (choices: 'qm357', 'qm358')",
    )
    parser.add_argument(
        "--stop",
        action="store_true",
        help="stop clock output",
    )
    parser.add_argument(
        "--start",
        action="store_true",
        help="start clock output",
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
        "--clock-id",
        type=int,
        help="Clock ID",
    )
    args = parser.parse_args()

    if args.description:
        print(parser.description)
        sys.exit(0)
    if not args.clock_id:
        sys.exit("Missing required clock-id argument")
    if not args.start and not args.stop:
        sys.exit("Select --start or --stop")
    if all([args.start, args.stop]):
        sys.exit("ERROR: clock output cannot be started and stopped at the same time.")

    log = logging.getLogger()
    if args.verbose:
        log.setLevel(logging.DEBUG)

    clock_id = (
        ClockIdQm358(args.clock_id)
        if args.soc == "qm358"
        else ClockIdQm357(args.clock_id)
    )
    control = (
        ClockControl.ClockControlStart if args.start else ClockControl.ClockControlStop
    )
    try:
        with Client(
            port=args.port,
            qtraces_logfile=args.qtraces_logfile,
        ) as client:
            rts = client.clock_out_control(clock_id, control)
            assert rts == Status.Ok, f"clock_out_control failed: {rts.name} ({rts})"
    except UciComError as e:
        rts = e.n
        log.critical(f"{e}")
        sys.exit(uqt_errno(rts))

    print("OK")
    sys.exit(uqt_errno(rts))


if __name__ == "__main__":
    main()
