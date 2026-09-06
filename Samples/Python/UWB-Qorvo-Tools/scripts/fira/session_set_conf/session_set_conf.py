#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

import argparse
import logging
import time
import os
import sys
import binascii

from uci import (
    Client,
    Status,
    UciComError,
    App,
    notification_default_handlers,
)
from uqt_utils.utils import uqt_errno

# Below hack sometimes required when operating on windows git-bash/msys2
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8")

epilog = """
Example of use:
    - session_set_conf DeviceType 0 MultiNodeMode 2
    - session_set_conf -s SESSION_HANDLE StaticStsIv "[0x1, 0x2, 0x3, 0x4, 0x5, 0x6]"
    - session_set_conf -s SESSION_HANDLE DeviceMacAddress "[0, 0]" DeviceType 0 DeviceRole 0 StaticStsIv "[0x1, 0x2, 0x3, 0x4, 0x5, 0x6]"
    - session_set_conf -s SESSION_HANDLE DeviceMacAddress "[0xa0, 0x00]" DstMacAddress "[0, 2, 4]"

Observations:
    - In the parameters defined by a list, the bytes are interpreted in little-endian order.
    - In the DstMacAddress, each item of the list is the mac address of a different destination.
"""


def main():
    default_port = os.getenv("UQT_PORT", "/dev/ttyUSB0")
    parser = argparse.ArgumentParser(
        description="Set session configuration parameters",
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
        "-t",
        "--time",
        type=int,
        default=0,
        help="sleep time (s) before closing the UCI traffic channel\n"
        "and exiting the script. -1: up to key pressed. (default: %(default)s)",
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
        default="2147483649",
        help="The unique session handle of the session to set the configuration. (default: %(default)s)",
    )
    parser.add_argument(
        "-l",
        "--list",
        action="store_true",
        help="List available configuration parameters",
    )
    parser.add_argument(
        "params",
        nargs="*",
        help="space separated <param> <value> list of parameters to set",
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

    if args.list:
        for a in App:
            print(f"{a.name} ({hex(a.value)})")
        sys.exit(uqt_errno(Status.Ok))

    # Handle user inputs & conversions
    try:
        sessions = eval(args.session)
        if not isinstance(sessions, (int, list)):
            raise ValueError(
                "Invalid session format. Please provide a single session handle or a list of session handles."
            )
        app_configs = []
        for i in range(0, len(args.params), 2):
            p = args.params[i]
            v = args.params[i + 1]
            if p == "SessionKey" or p == "SubSessionKey":
                if not ((len(v) == 64) or (len(v) == 32) or (len(v) == 16)):
                    print(f"'{v}' expected to be a 16 or 32 bytes {p} value")
                    sys.exit(uqt_errno(2))
            else:
                v = eval(args.params[i + 1])
                if type(v) is int:
                    v = hex(v)
                elif type(v) is list:
                    v = str(v)
                else:
                    v = hex(v.encode("utf-8"))

            app_configs.append(
                (
                    eval(f"App.{p}"),
                    (
                        eval(v)
                        if (p != "SessionKey" and p != "SubSessionKey")
                        else binascii.unhexlify(v)
                    ),
                )
            )
    except Exception as e:
        print(f'Error while handling user input "{p}, {v}":\n{e}')
        sys.exit(uqt_errno(2))

    if isinstance(sessions, (int, str)):
        sessions = [int(args.session)]

    print("List of parameters to set:\n")
    for i in app_configs:
        p = f"{i[0].name} ({hex(i[0])}):"
        try:
            v = hex(i[1])
        except Exception:
            v = repr(i[1])
        print(f"    {p:<35} {v}")
    print("\n")

    while sessions:
        session_handle = sessions.pop(0)
        try:

            client = None
            client = Client(
                port=args.port,
                notif_handlers=notification_default_handlers,
                qtraces_logfile=args.qtraces_logfile,
            )
            print(
                f"session: {session_handle}, session configuration with the parameters\n"
            )
            rts, rtv = client.session_set_app_config(session_handle, app_configs)
            if rts != Status.Ok:
                print(
                    f"session_set_app_config {session_handle} failed: {rts.name} ({rts}).{rtv}"
                )

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
