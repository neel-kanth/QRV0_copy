#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

import argparse
import logging
import time
import os

from uci import Client, UciComError, SessionState
from uci import Status
from uqt_utils.utils import uqt_errno

# Below hack sometimes required when operating on windows git-bash/msys2
import sys

sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8")

URSK = "ed07a80d2beb00f785af26270000002a"


def main():
    parser = argparse.ArgumentParser(description="Stop a Fira Two Way Ranging session.")
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
        "-s",
        "--session_handle",
        type=int,
        help="session handle. (default: %(default)s)",
        default=42,
    )
    parser.add_argument(
        "--session-key",
        type=str,
        default=URSK,
        help="Key to use for secured ranging. (default: %(default)s)",
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

    if not ((len(args.session_key) == 64) or (len(args.session_key) == 32)):
        sys.exit(
            f"Error: '{args.session_key}'  expected to be of 16 or 32 bytes. Quitting."
        )

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
    log = logging.getLogger()

    while True:
        try:

            client = None
            client = Client(port=args.port, qtraces_logfile=args.qtraces_logfile)

            rts, rtv = client.session_get_state(args.session_handle)
            if rts != Status.Ok:
                print(f"stop_fira_twr failed: {rts.name} ({rts})")
                client.session_deinit(args.session_handle)
                break

            if rtv == SessionState.Active:
                print("Stopping ranging...")
                rts = client.ranging_stop(args.session_handle)
                if rts != Status.Ok:
                    log.critical(
                        f"stop_fira_twr failed failed with status: {rts.name} ({rts})"
                    )
                    break
            print(rtv)
            time.sleep(0.1)

            rts = client.session_deinit(args.session_handle)
            print("# Stopping session:\n    ", rts.name)
            if rts != Status.Ok:
                print(f"session_deinit failed: {rts.name} ({rts})")
                break

            time.sleep(0.1)

            break

        except UciComError as e:
            rts = e.n
            log.critical(f"{e}")
            break

    if client:
        client.close()
    sys.exit(uqt_errno(rts))


if __name__ == "__main__":
    main()
