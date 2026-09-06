#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

import argparse
import logging
import time
import os

from uci import (
    Gid,
    OidRanging,
    OidQorvo,
    Status,
    Client,
    SessionType,
    UciComError,
    App,
    RangingData,
    RangingDiagData,
    NotImplementedData,
)
from uci.qorvo_msg import SessionDataTransferStatus
from uqt_utils.utils import (
    uqt_errno,
    str2bytes,
)

# Below hack sometimes required when operating on windows git-bash/msys2
import sys

sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8")

epilog = """
Note:
Example of usage for sending a UCI Data Packet:
    - run_fira_owr -d '[1, 2, 3]*10' -n 1
    - run_fira_owr -d 0x0504030201 -n 1
    ** default : advertiser mode and device_mac_address="00:0d"
"""


def main():
    parser = argparse.ArgumentParser(
        description="Fira One Way Ranging using UCI.", epilog=epilog
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
        help="serial port used. (default: %(default)s)",
        default=os.getenv("UQT_PORT", "/dev/ttyUSB0"),
    )
    parser.add_argument(
        "--observer",
        action="store_true",
        default=False,
        help="start in observer mode (default advertiser mode)",
    )
    parser.add_argument(
        "-t",
        "--time",
        type=int,
        help="duration of the ranging session. -1: forever. (default: %(default)s)",
        default=10,
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="use logging.DEBUG level. (default: %(default)s)",
        default=False,
    )
    parser.add_argument(
        "-c",
        "--channel",
        type=int,
        help="channel number. (default: %(default)s)",
        default=9,
    )
    parser.add_argument(
        "-s",
        "--session_id",
        type=int,
        help="session id. (default: %(default)s)",
        default=42,
    )
    parser.add_argument(
        "--device_mac_address",
        type=str,
        default="00:0d",
        help="set DeviceMacAddress value. (default: %(default)s)",
    )
    parser.add_argument(
        "--dst_mac_address",
        type=str,
        default="['00:0c']",
        help="set DstMacAddress value. (default: %(default)s)",
    )
    parser.add_argument(
        "--number_of_controlees",
        type=int,
        default=1,
        help="set NumberOfControlees value. (default: %(default)s)",
    )
    parser.add_argument(
        "--preamble_code_index",
        type=int,
        default=10,
        help="set PreambleCodeIndex value. (default: %(default)s)",
    )
    parser.add_argument(
        "--sfd_id", type=int, default=2, help="set SfdId value. (default: %(default)s)"
    )
    parser.add_argument(
        "--slot_duration",
        type=int,
        default=2400,
        help="set SlotDuration value. (default: %(default)s)",
    )
    parser.add_argument(
        "--slots_per_rr",
        type=int,
        default=25,
        help="set SlotsPerRr value. (default: %(default)s)",
    )
    parser.add_argument(
        "--hopping_mode",
        type=int,
        default=0,
        help="set HoppingMode value. (default: %(default)s)",
    )
    parser.add_argument(
        "--en-psdu-dump",
        action="store_true",
        default=False,
        help="set the Qorvo PSDU_DUMP value to True. (default: %(default)s)",
    )
    parser.add_argument(
        "-r",
        "--data-repetition-count",
        type=lambda x: int(x, 0),
        default=0,
        help="Data Repetition Count. Allowed range is [0-255]. Accepts hex and decimal values. \
            0xFF=Infinite; 0=No repetition. (default: %(default)s)",
    )
    parser.add_argument(
        "-d",
        "--data-packet",
        type=str,
        default=None,
        help="set UCI Data Messages and configure the sequence_number argument. (default: %(default)s)",
    )
    parser.add_argument(
        "-n",
        "--sequence-number",
        type=int,
        default=None,
        help="Data Sequence Number for the UCI Data Message for a given session. (default: %(default)s)",
    )
    parser.add_argument(
        "--en-data-xfer-ntf",
        action="store_true",
        default=False,
        help="set the DATA_TRANSFER_STATUS to Enabled. (default: %(default)s)",
    )
    parser.add_argument(
        "--enable_diagnostics",
        action="store_true",
        default=False,
        help="set EnableDiagnostics to 1. (default: %(default)s)",
    )
    parser.add_argument(
        "--diag-fields",
        type=str,
        default="metrics|aoa|cfo",
        help="set the Qorvo DIAGNOSTIC_FRAME_REPORTS_FIELD value.\n"
        "OR flags: metrics, aoa, cir, cfo. (default: %(default)s)",
    )
    parser.add_argument(
        "--min_frames_per_rr",
        type=int,
        default=4,
        help="set MinFramesPerRr value. (default: %(default)s)",
    )
    #  Below argument is a temp. work-around
    parser.add_argument(
        "--device",
        choices=["controller", "controlee"],
        help="set the DEVICE_TYPE value. (default: %(default)s)",
    )
    parser.add_argument(
        "--antenna-set-id",
        type=int,
        choices=[0, 1, 2, 3],
        default=0,
        help="set the antenna set to use for the session. (default: %(default)s)",
    )
    parser.add_argument(
        "--psdu-data-rate",
        type=int,
        choices=[0, 1, 2, 3, 4],
        default=0,
        help="This value configures the data rate for PHY service Data Unit (PSDU):"
        " 0 6.81Mbps / 1 7.80Mbps / 2 27.2Mbps / 3 31.2Mbps / 4 850Kbps, (default: %(default)s)",
    )
    parser.add_argument(
        "--prf-mode",
        type=int,
        choices=[0, 1, 2],
        default=0,
        help="This parameter is used to configure the mean Pulse Repetition Frequency (PRF):"
        " 0 BPRF / 1 HPRF / 2 HPRF High Rate, (default: %(default)s)",
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

    # Handle user inputs & conversions
    try:
        user_mapping = dict(
            aoa=0x2,
            cfo=0x8,
            metrics=0x20,
            cir=0x40,  # diag_fields
        )
        args.device_mac_address = list(str2bytes(args.device_mac_address, "little"))
        args.dst_mac_address = list(
            int.from_bytes(str2bytes(v), "big") for v in eval(args.dst_mac_address)
        )

        args.diag_fields = eval(args.diag_fields, user_mapping)
    except Exception as e:
        print(f"Error while handling user input:\n{e}")
        sys.exit(uqt_errno(2))

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
    log = logging.getLogger()

    if args.data_packet and args.observer is False:
        try:
            try:
                data = eval(f"bytes({args.data_packet})")
            except Exception:
                x = int(args.data_packet)
                data = eval(f"{x}.to_bytes(l, 'little', False)")
        except Exception as e:
            print(f"Error while handling user input :\n{e}")
            sys.exit(uqt_errno(2))

    notif_handlers = {
        (Gid.Ranging, OidRanging.Start): lambda x: print(RangingData(x)),
        (Gid.Qorvo, OidQorvo.TestDiag): lambda x: print(RangingDiagData(x)),
        (Gid.Qorvo, OidQorvo.SessionDataXferStatusNtf): lambda x: print(
            SessionDataTransferStatus(x)
        ),
        ("default", "default"): lambda gid, oid, x: print(
            NotImplementedData(gid, oid, x)
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

            print(f"Initializing session {args.session_id}...")
            rts, session_handle = client.session_init(
                args.session_id, SessionType.Ranging
            )
            if rts != Status.Ok:
                print(f"session_init failed: {rts.name} ({rts})")
                break

            if session_handle is None:
                print(
                    f"Using Fira 1.3 (session handle == session ID) is : {args.session_id}"
                )
                session_handle = args.session_id
            else:
                print(f"Using Fira 2.0 session handle is : {session_handle}")

            if args.observer:
                print("Sending session config to observer...")
                rts, rtv = client.session_set_app_config(
                    session_handle,
                    [
                        (App.DeviceRole, 6),
                        (App.ScheduleMode, 1),
                        (App.RangingRoundUsage, 6),
                    ],
                )
            else:
                print("Sending session config to advertiser...")
                rts, rtv = client.session_set_app_config(
                    session_handle,
                    [
                        (App.DeviceRole, 5),
                        (App.ScheduleMode, 1),
                        (App.RangingRoundUsage, 6),
                    ],
                )

            if rts != Status.Ok:
                print(f"session_set_app_config failed: {rts.name} ({rts}).")
                print(f"{rtv}")
                client.session_deinit(session_handle)
                break

            app_configs = [
                (App.DeviceMacAddress, args.device_mac_address),
                (App.DstMacAddress, args.dst_mac_address),
                (App.NumberOfControlees, args.number_of_controlees),
                (App.PreambleCodeIndex, args.preamble_code_index),
                (App.SfdId, args.sfd_id),
                (App.SlotDuration, args.slot_duration),
                (App.SlotsPerRr, args.slots_per_rr),
                (App.HoppingMode, args.hopping_mode),
                (App.RangingDuration, 200),
                (App.MinFramesPerRr, args.min_frames_per_rr),
                (App.InterFrameInterval, 20),
                (App.ChannelNumber, args.channel),
                (App.RframeConfig, 1),
                (App.MultiNodeMode, 1),
                (App.DataRepetitionCount, args.data_repetition_count),
                (
                    App.SessionDataTransferStatusNtfConfig,
                    1 if args.en_data_xfer_ntf else 0,
                ),
                (
                    App.EnablePSDUDump,
                    1 if args.en_psdu_dump else 0,
                ),  # 0: disable / 1: enable
                (App.EnableDiagnostics, 1 if args.enable_diagnostics else 0),
                (App.DiagsFrameReportsFields, args.diag_fields),
                (App.PsduDataRate, args.psdu_data_rate),
                (App.PrfMode, args.prf_mode),
            ]

            #  Below is a temp. work-around
            if "device" in vars(args):
                app_configs.append(
                    (App.DeviceType, 0 if args.device == "controlee" else 1)
                )
            if "antenna_set_id" in vars(args):
                app_configs.append((App.TxAntennaSelection, args.antenna_set_id))
                app_configs.append((App.RxAntennaSelection, args.antenna_set_id))

            for i in app_configs:
                p = f"{i[0].name} ({hex(i[0])}):"

                try:
                    v = hex(i[1])
                except Exception:
                    try:
                        v = i[1].hex(".")
                    except Exception:
                        v = repr(i[1])
                print(f"    {p:<35} {v}")

            print("Sending session config to observer...")
            rts, rtv = client.session_set_app_config(session_handle, app_configs)

            if rts != Status.Ok:
                print(f"session_set_app_config failed: {rts.name} ({rts}).")
                print(f"{rtv}")
                client.session_deinit(session_handle)
                break

            print("Starting ranging...")
            rts = client.ranging_start(session_handle)
            if rts != Status.Ok:
                print(f"ranging_start failed: {rts.name} ({rts})")
                client.session_deinit(session_handle)
                break

            if args.data_packet and args.observer is False:
                print("Sending UCI data packets")
                if args.sequence_number is None:
                    client.session_deinit(session_handle)
                    print("The 'sequence_number' argument is missing.")
                    break
                client.session_send_data(
                    session_handle, args.device_mac_address, args.sequence_number, data
                )

            if args.time == -1:
                input("Press <RETURN> to stop\n")
            else:
                time.sleep(args.time)

            print("Stopping ranging...")
            rts = client.ranging_stop(session_handle)
            if rts != Status.Ok:
                print(f"ranging_stop failed: {rts.name} ({rts})")
                client.session_deinit(session_handle)
                break
            time.sleep(2)

            print("Deinitializing session...")
            rts = client.session_deinit(session_handle)
            print(f"    {rts.name}")
            if rts != Status.Ok:
                print(f"session_deinit failed: {rts.name} ({rts})")
                break

            time.sleep(2)

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
