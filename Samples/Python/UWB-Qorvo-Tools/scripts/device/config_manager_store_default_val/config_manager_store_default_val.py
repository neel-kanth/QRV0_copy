#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

import argparse
import logging
import os
import sys
import json

from uci import (
    Client,
    Status,
    ConfigManagerTransactionType,
    ConfigManagerTransactionEndType,
)
from uqt_utils.utils import uqt_errno


# Below hack sometimes required when operating on windows git-bash/msys2
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8")


def load_default_values(client, filename):
    """
    Store default values to device from given JSON file.
    """
    default_file = open(filename, "r")
    values = json.load(default_file)

    print("\nSetting default values...")

    try:
        for key, value in values["values"].items():
            to_set = int(value, 0)
            print(f"setting {key} to value {to_set}", end="")
            ret, _ = client.config_manager_transaction_set_val([(key, to_set)])
            print(f"...{ret.name}")
            if ret != Status.Ok:
                print("\n*******************ERROR*********************")
                print(f"Error Setting key '{key}' to value '{value}'")
                print("Device refused to set default value with reason:" f" {ret.name}")
                print("*********************************************")
                return False
    except KeyError as e:
        print("\n*******************ERROR*********************")
        print(f"Expected key {e} not found in JSON file")
        print("*********************************************")
    except Exception as e:
        print("\n*******************ERROR*********************")
        print(f"Error Setting key '{key}' to value '{value}'")
        print(f"Exception: {e}")
        print("*********************************************")

    print("Done.")
    return True


def main():
    default_port = os.getenv("UQT_PORT", "/dev/ttyUSB0")
    parser = argparse.ArgumentParser(
        description="Set a parameter value.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="",
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
        help="use logging.DEBUG level. (default: %(default)s)",
        default=False,
    )
    parser.add_argument(
        "-l",
        "--lock",
        action="store_true",
        help="lock default values after storing values. (default: %(default)s)",
        default=False,
    )
    parser.add_argument(
        "-i",
        "--input-file",
        type=str,
        help="Path to JSON file containing default values to store.",
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

    if not args.input_file:
        log.error("Please provide a JSON file.")
        sys.exit(0)

    if args.lock:
        print("Warning: default values will be locked at the end of the transaction.")
        print(
            "It will not be possible to modify default values anymore on the current device."
        )
        input("Press <ENTER> if you want to continue.")

    client = Client(port=args.port, qtraces_logfile=args.qtraces_logfile)

    # First, start transaction of type "DEFAULT_VALUE"
    rts = client.config_manager_transaction_start(ConfigManagerTransactionType.Default)
    if rts != Status.Ok:
        log.error(f"Could not start transaction {rts.name} ({rts})")
        sys.exit(0)

    success = load_default_values(client=client, filename=args.input_file)
    if success:
        transaction_end_type = ConfigManagerTransactionEndType.Commit
    else:
        log.error("Must abort transaction")
        transaction_end_type = ConfigManagerTransactionEndType.Abort

    # Finally, end transaction
    rts = client.config_manager_transaction_end(transaction_end_type)
    if rts != Status.Ok:
        log.error(f"Could not end transaction {rts.name} ({rts})")

    if success and args.lock:
        rts = client.config_manager_lock_default_values()
        if rts != Status.Ok:
            log.error(f"Could not lock default values {rts.name} ({rts})")
        else:
            log.info("Default values locked successfully")

    client.close()

    if client:
        client.close()
    sys.exit(uqt_errno(rts))


if __name__ == "__main__":
    main()
