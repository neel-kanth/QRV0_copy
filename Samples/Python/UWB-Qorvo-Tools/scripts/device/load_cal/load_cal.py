#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

import argparse
import logging
import sys
import os

from uci import Client
from uqt_utils.load_calibration import load_calibration


def main():
    default_port = os.getenv("UQT_PORT", "/dev/ttyUSB0")
    parser = argparse.ArgumentParser(
        description="Load calibration from given JSON file to device through UCI."
    )
    parser.add_argument(
        "--description",
        action="store_true",
        help="show short description of the script",
        default=False,
    )
    parser.add_argument(
        "-f",
        "--calibration",
        type=str,
        help="Path to calibration JSON file.",
        default=None,
    )
    parser.add_argument(
        "-p",
        "--port",
        type=str,
        default=default_port,
        help="Communication port to use. (default: %(default)s)",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="use logging.DEBUG level",
        default=False,
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

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    if args.description:
        print(parser.description)
        sys.exit(0)

    if not args.calibration:
        logging.error("Please provide a calibration file.")
        sys.exit(0)

    client = Client(port=args.port, qtraces_logfile=args.qtraces_logfile)

    load_calibration(client=client, calibration_filename=args.calibration)

    client.close()


if __name__ == "__main__":
    main()
