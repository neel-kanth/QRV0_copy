#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

import argparse
import logging
import time
import os
import sys

from uci import (
    Client,
    Status,
    UciComError,
    notification_default_handlers,
)
from uqt_utils.utils import uqt_errno


# Below hack sometimes required when operating on windows git-bash/msys2
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8")

epilog = """
Note:
    A test mode session requires the use of a '0' session id.

Example of use:
    session_init
    session_init -s 0 --type test
"""


def main():
    default_port = os.getenv("UQT_PORT", "/dev/ttyUSB0")
    parser = argparse.ArgumentParser(
        description="Initialize a FIRA session",
        formatter_class=argparse.RawTextHelpFormatter,
        epilog=epilog,
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
        "-s",
        "--session",
        type=str,
        default="42",
        help="set a unique session id to use or a list of sessions to allow multiple session handling in "
        "the same uci single command script. (default: %(default)s)",
    )
    parser.add_argument(
        "-t",
        "--time",
        type=int,
        default=0,
        help="sleep time (s) before closing the UCI traffic channel\n"
        "and exiting the script. -1: up to key pressed. (default: %(default)s)",
    )
    parser.add_argument(
        "session_type",
        choices=["ranging", "test"],
        default="ranging",
        nargs="?",
        help="set the session type to use. (default: %(default)s)",
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

    # Handle user inputs & conversions
    try:
        user_mapping = dict(ranging=0, test=0xD0)
        sessions = eval(args.session)
        if not isinstance(sessions, (int, list)):
            raise ValueError(
                "Invalid session format. Please provide a single session ID or a list of session IDs."
            )
    except Exception as e:
        print(f"Error while handling user input: {e}")
        sys.exit(uqt_errno(2))

    if isinstance(sessions, (int, str)):
        sessions = [int(args.session)]

    while sessions:
        session_id = sessions.pop(0)
        try:

            client = None
            client = Client(
                port=args.port,
                notif_handlers=notification_default_handlers,
                qtraces_logfile=args.qtraces_logfile,
            )

            print(f"Initializing {args.session_type} session {session_id}...")
            rts, session_handle = client.session_init(
                session_id, user_mapping[args.session_type]
            )
            if rts != Status.Ok:
                print(f"session_init {session_id} failed: {rts.name} ({rts})")
                break

            if session_handle is None:
                print(
                    f"Using Fira 1.3 (session handle == session ID) is : {session_id}"
                )
            else:
                print(f"Using Fira 2.0 session handle is : {session_handle}")

            if args.time == -1:
                input("Press <RETURN> to stop\n")
            else:
                time.sleep(args.time)

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
