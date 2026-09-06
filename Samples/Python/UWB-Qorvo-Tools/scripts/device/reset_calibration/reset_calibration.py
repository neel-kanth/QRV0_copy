#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

import argparse
import logging
import os

from uci import (
    Client,
    UciComError,
    Status,
    ConfigManagerTransactionEndType,
)
from uqt_utils.utils import uqt_errno

# Below hack sometimes required when operating on windows git-bash/msys2
import sys

sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8")


def main():
    default_port = os.getenv("UQT_PORT", "/dev/ttyUSB0")

    parser = argparse.ArgumentParser(
        description="Reset device calibration parameters to their default values."
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
        help="communication port to use. (default: %(default)s)",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        default=False,
        help="use logging.DEBUG level. (default: %(default)s)",
    )
    parser.add_argument(
        "-y",
        "--yes",
        action="store_true",
        default=False,
        help="by-pass prompt. (default: %(default)s)",
    )
    parser.add_argument(
        "key",
        type=str,
        nargs="*",
        help="blank separated calibration key (name or integer id). all: get all.",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=6,
        help="time in second until the script timeout. (default: %(default)s)",
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

    if args.key == ["all"]:  # ask target to output all defined cal params as  t,l,v
        args.key = []

    if args.key == []:
        print("Resetting all params...")
    else:
        print(f"Resetting {args.key}...")

    if not args.yes:
        print("Warning: your default calibration and configuration will be erased")
        input("press <ENTER> to continue")

    try:

        client = None
        client = Client(port=args.port, qtraces_logfile=args.qtraces_logfile)

        if args.key == []:
            rts = client.reset_calibration(args.timeout)
        else:
            rts = client.config_manager_transaction_start()
            if rts != Status.Ok:
                log.error(f"Could not start transaction {rts.name} ({rts})")
                sys.exit(0)

            rts, dummy = client.config_manager_transaction_reset_val(args.key)
            if rts == Status.Ok:
                transaction_end_type = ConfigManagerTransactionEndType.Commit
            else:
                print(f"Reset failed with status: {rts.name} ({hex(rts)}).")
                log.error("Must abort transaction")
                transaction_end_type = ConfigManagerTransactionEndType.Abort

            # Finally, end transaction
            rts = client.config_manager_transaction_end(transaction_end_type)
            if rts != Status.Ok:
                log.error(f"Could not end transaction {rts.name} ({rts})")

    except UciComError as e:
        rts = e.n
        log.critical(f"{e}")

    if client:
        client.close()
    sys.exit(uqt_errno(rts))


if __name__ == "__main__":
    main()
