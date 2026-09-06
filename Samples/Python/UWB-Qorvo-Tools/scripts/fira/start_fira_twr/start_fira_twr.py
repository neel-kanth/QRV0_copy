#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

import argparse
import logging
import os
import sys
import binascii

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
    RangingPSDUSReport,
    RangingDiagData,
    NotImplementedData,
)
from uqt_utils.utils import (
    uqt_errno,
    str2bytes,
)

eng_ursk_prefix = "ed07a80d2beb00f785af2627"

# Below hack sometimes required when operating on windows git-bash/msys2
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8")

epilog = """
Note:
    - Default profiles:
        time-base scheduling.
        if --controllee:
            role=responder, mac="00:01", dest-mac="['00:00']"
        else:
            role=initiator , mac="00:00", dest-mac="['00:01']"
    - The default skey (sskey) used is the engineering hard coded key
      from EVB firmware. This eng key is used to mimic a (unavailable) SE.
      You may thus exercize different STS_CONFIG value without SE.
      It is constructed as below:
      {eng_ursk_prefix}<session_id on 4 bytes>

Specifying flag arguments:
        - use either an integer value representing the combined flags, e.g.: --round-ctrl 6
        - or specify individual flags separated by '|', e.g.: --round-ctrl 'cm|rcp'.
        This affects the following arguments: round-ctrl, diag-fields.

Example of use:
    - minimal setting with defaults:
      start_fira_twr -p /dev/ttyUSB0
    - contention -base scheduling:
      run_fira_twr -t 1 --schedule contention --node onetomany \
          --round-ctrl 'cm|rcp' --round ss-non-deferred --frame sp1 -t -1 --controlee
"""


def main():
    default_port = os.getenv("UQT_PORT", "/dev/ttyUSB0")
    parser = argparse.ArgumentParser(
        description="start a Fira two way ranging session.",
        formatter_class=argparse.RawTextHelpFormatter,
        epilog=epilog,
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
        "-t",
        "--time",
        type=int,
        default=10,
        help="duration of the ranging session. -1: forever. (default: %(default)s)",
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
        help="set the session id to use. (default: %(default)s)",
    )
    parser.add_argument(
        "-c",
        "--channel",
        type=int,
        default=9,
        help="set the CHANNEL_NUMBER value. (default: %(default)s)",
    )
    parser.add_argument(
        "--controlee",
        action="store_true",
        default=False,
        help="set the DEVICE_TYPE value and related default profile.\n"
        "use -h to review the default profiles.)",
    )
    parser.add_argument(
        "--round",
        choices=["ss-deferred", "ds-deferred", "ss-non-deferred", "ds-non-deferred"],
        default="ds-deferred",
        help="set the RANGING_ROUND_USAGE value. (default: %(default)s)",
    )
    parser.add_argument(
        "--round-ctrl",
        type=str,
        help="set the RANGING_ROUND_CONTROL values.\n"
        "OR flags: rrrm, cm, rcp, mrp, mrm (default: %(default)s)",
    )
    parser.add_argument(
        "--en-key-rot", action="store_true", default=False, help="set KEY_ROTATION to 1"
    )
    parser.add_argument(
        "--key-rot-rate", type=int, default=0, help="set KEY_ROTATION_RATE"
    )
    parser.add_argument(
        "--sts",
        choices=["static", "dyn", "dyn-key", "provisioned", "provisioned-key"],
        help="set the STS_CONFIG value. (default=static)",
    )
    parser.add_argument(
        "--slot-span",
        type=int,
        default=2400,
        help="set the SLOT_DURATION value. (default: %(default)s)",
    )
    parser.add_argument(
        "--node",
        choices=dict(unicast=0, onetomany=1, manytomany=2),
        default="unicast",
        help="set the MULTI_NODE_MODE value. (default: %(default)s)",
    )
    parser.add_argument(
        "--ranging-span",
        type=int,
        default=200,
        help="set the RANGING_DURATION param. (default: %(default)s)\n"
        "(previously RANGING_INTERVAL)",
    )
    parser.add_argument(
        "--en-diag",
        action="store_true",
        default=False,
        help="set the Qorvo ENABLE_DIAGNOSTIC parameter to 1.",
    )
    parser.add_argument(
        "--diag-fields",
        type=str,
        default="metrics|aoa|cfo",
        help="set the Qorvo DIAGNOSTIC_FRAME_REPORTS_FIELD value.\n"
        "OR flags: metrics, aoa, cir, cfo. (default: %(default)s)",
    )
    parser.add_argument(
        "--meas-max",
        type=int,
        default=0,
        help="set the MAX_NUMBER_OF_MEASUREMENTS value, 0 (unlimited). (default: %(default)s)",
    )
    parser.add_argument(
        "--skey",
        type=str,
        help="set the SESSION_KEY 16 or 32 bytes value.\n"
        '"default" is an accepted value (see help)',
    )
    parser.add_argument(
        "--en-psdu-dump",
        action="store_true",
        default=False,
        help="set the Qorvo PSDU_DUMP value to 1",
    )
    parser.add_argument(
        "--schedule",
        choices=["contention", "time"],
        default="time",
        help="set the SCHEDULE_MODE value. (default: %(default)s)",
    )
    parser.add_argument(
        "--cap-range",
        type=str,
        default="0x0510",
        help="set the CAP_SIZE_RANGE  value which is a 2 bytes word. (default: %(default)s)\n"
        "MSB: minimum cap size (default:5).\n"
        "LSB: maximum (Default: SLOTS_PER_RR-1). Default: 0x10",
    )
    parser.add_argument(
        "--mac",
        type=str,
        help="set the DEVICE_MAC_ADDRESS value.\n"
        "default: 00:00 if controlee else 00:01.",
    )
    parser.add_argument(
        "--dest-mac",
        type=str,
        help="set the DST_MAC_ADDRESS value which is a list.\n"
        'default: ["00:01"] if controlee or ["00:00"].',
    )
    parser.add_argument(
        "--frame",
        choices=["sp0", "sp1", "sp3"],
        default="sp3",
        help="set the RFRAME_CONFIG value.  Frame Config. (default: %(default)s)",
    )
    parser.add_argument("--ssession", type=str, help="set the SUB_SESSION_ID value.")
    parser.add_argument(
        "--sskey",
        type=str,
        help="set the SUB_SESSION_KEY 16 or 32 bytes value.\n"
        '"default" is an accepted value (see help)',
    )
    parser.add_argument(
        "--en-rssi",
        action="store_true",
        default=False,
        help="set the RSSI_REPORTING value to 1. (default: %(default)s)",
    )
    parser.add_argument(
        "--init-time",
        type=int,
        default=0,
        help="set the UWB_INITIATION_TIME value. (default: %(default)s)",
    )
    parser.add_argument(
        "--antenna-set-id",
        type=int,
        choices=[0, 1, 2, 3],
        default=0,
        help="set the antenna set to use for the session. (default: %(default)s)",
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
        "--n_controlees",
        type=int,
        default=1,
        help="set the number of controlee in case of onetomany ranging. (default: %(default)s)",
    )

    opts = parser.parse_args()

    if opts.description:
        print(parser.description)
        sys.exit(0)

    if opts.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
    log = logging.getLogger()

    default_URSK = (
        eng_ursk_prefix + eval(opts.session).to_bytes(4, "big", signed=False).hex()
    )

    default_config = dict(
        # Fira Mandatory:
        device="controlee" if opts.controlee else "controller",  # DEVICE_TYPE
        mac="00:01" if opts.controlee else "00:00",  # DEVICE_MAC_ADDRESS
        # Other:
        report="tof|azimuth|elevation|fom",  # RESULT_REPORT_CONFIG
        vendor=[0x07, 0x08],  # VENDOR_ID
        static_sts=[0x01, 0x02, 0x03, 0x04, 0x05, 0x06],  # STATIC_STS_IV
        aoa_report=1,  # AOA_RESULT_REQ
        preamble_idx=10,  # PREAMBLE_CODE_INDEX
        sfd=2,  # SFD_ID
        slots_per_rr=25,  # SLOTS_PER_RR
        hop=0,  # HOPPING_MODE
        sts="static",  # STS_CONFIG
    )
    if opts.sts in ["dyn-key", "provisioned", "provisioned-key"]:
        default_config["skey"] = default_URSK
    if opts.schedule == "time":
        default_config["n_controlees"] = 1  # NUMBER_OF_CONTROLEES
        default_config["dest_mac"] = (
            "['00:00']" if opts.controlee else "['00:01']"
        )  # DEST_MAC-ADDRESS

    args = type("args", (), {})()
    args.__dict__.update(default_config)
    args.__dict__.update({k: v for k, v in vars(opts).items() if v is not None})

    # Handle user inputs & conversions
    try:
        user_mapping = dict(
            unicast=0,
            onetomany=1,
            manytomany=2,  # node
            controlee=0,
            controller=1,  # device
            aoa=0x2,
            cfo=0x8,
            metrics=0x20,
            cir=0x40,  # diag_fields
            ut=0,
            ss_deferred=1,
            ds_deferred=2,
            ss_non_deffered=3,  # round
            ds_non_deffered=4,
            dt=5,
            owr=6,
            ess=7,
            ads=8,  # round
            rrrm=1,
            cm=2,
            rcp=4,
            mrp=64,
            mrm=128,  # round_ctrl
            sp0=0,
            sp1=1,
            sp3=3,  # frame
            contention=0,
            time=1,  # schedule
            tof=1,
            azimuth=2,
            elevation=4,
            fom=8,  # report
            static=0,
            dyn=1,
            dyn_key=2,
            provisioned=3,
            provisioned_key=4,  # sts
        )
        args.mac = list(str2bytes(args.mac, "little"))

        # in contention based ranging the dest_mac should not be set
        if args.schedule == "time":
            args.dest_mac = list(
                int.from_bytes(str2bytes(v), "big") for v in eval(args.dest_mac)
            )

        for p in (
            "session",
            "ssession",
            "cap_range",
            "diag_fields",
            "round",
            "frame",
            "schedule",
            "report",
            "sts",
            "node",
            "device",
            "round_ctrl",
        ):
            if hasattr(args, p):
                setattr(args, p, eval(getattr(args, p).replace("-", "_"), user_mapping))
        for p in ["skey", "sskey"]:
            if hasattr(args, p):
                setattr(args, p, eval(getattr(args, p).replace("-", "_"), user_mapping))
        for p in ["skey", "sskey"]:
            if hasattr(args, p):
                if getattr(args, p) == "default":
                    setattr(args, p, default_URSK)
                if not ((len(getattr(args, p)) == 64) or (len(getattr(args, p)) == 32)):
                    raise ValueError(
                        f"'{getattr(args, p)}'  expected to be a 16 or 32 bytes {p} value. Quitting."
                    )
                setattr(args, p, binascii.unhexlify(getattr(args, p)))
    except Exception as e:
        print(f'Error while handling user input "{p}":\n{e}')
        sys.exit(uqt_errno(2))

    notif_handlers = {
        (Gid.Ranging, OidRanging.Start): lambda x: print(RangingData(x)),
        (Gid.Qorvo, OidQorvo.TestDiag): lambda x: print(RangingDiagData(x)),
        (Gid.Qorvo, OidQorvo.PSDUSReport): lambda x: print(RangingPSDUSReport(x)),
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
                qtraces_logfile=opts.qtraces_logfile,
            )
            print(f"Initializing session {args.session}...")
            rts, session_handle = client.session_init(args.session, SessionType.Ranging)
            if rts != Status.Ok:
                print(f"session_init failed: {rts.name} ({rts})")
                break

            if session_handle is None:
                print(
                    f"Using Fira 1.3 (session handle == session ID) is : {args.session}"
                )
                session_handle = args.session
            else:
                print(f"Using Fira 2.0 session handle is : {session_handle}")

            # Fira Mandatory/minimal session config:
            app_configs = [
                (App.DeviceType, args.device),
                (App.DeviceRole, 0 if args.controlee else 1),
                (App.MultiNodeMode, args.node),
                (App.RangingRoundUsage, args.round),
                (App.DeviceMacAddress, args.mac),
                # Additional config:
                (App.ChannelNumber, args.channel),
                (App.ScheduleMode, args.schedule),
                (App.CapSizeRange, args.cap_range),
                (App.StsConfig, args.sts),
                (App.RframeConfig, args.frame),
                (App.ResultReportConfig, args.report),
                (App.VendorId, args.vendor),
                (App.StaticStsIv, args.static_sts),
                (App.AoaResultReq, args.aoa_report),
                (App.UwbInitiationTime, args.init_time),
                (App.PreambleCodeIndex, args.preamble_idx),
                (App.SfdId, args.sfd),
                (App.SlotDuration, args.slot_span),
                (App.RangingDuration, args.ranging_span),
                (App.SlotsPerRr, args.slots_per_rr),
                (App.MaxNumberOfMeasurements, args.meas_max),
                (App.HoppingMode, args.hop),
                (App.RssiReporting, 1 if args.en_rssi else 0),
            ]
            if "ssession" in vars(args):
                app_configs.append((App.SubSessionId, args.ssession))

            # in contention based ranging the n_controlees should not be set
            if "n_controlees" in vars(args) and args.schedule == "time":
                app_configs.append((App.NumberOfControlees, args.n_controlees))
            if "dest_mac" in vars(args):
                app_configs.append((App.DstMacAddress, args.dest_mac))
            if "round_ctrl" in vars(args):
                app_configs.append((App.RangingRoundControl, args.round_ctrl))
            if args.en_key_rot:
                app_configs.append((App.KeyRotation, 1))
            if "key_rot_rate" in vars(args):
                app_configs.append((App.KeyRotationRate, args.key_rot_rate))
            if args.en_diag:
                app_configs.extend(
                    [
                        (App.EnableDiagnostics, 1),
                        (App.DiagsFrameReportsFields, args.diag_fields),
                    ]
                )
            if args.en_psdu_dump:
                app_configs.append((App.EnablePSDUDump, 1))
            if "skey" in vars(args):
                app_configs.append((App.SessionKey, args.skey))
            if "sskey" in vars(args):
                app_configs.append((App.SubSessionKey, args.sskey))
            if "antenna_set_id" in vars(args):
                app_configs.append((App.TxAntennaSelection, args.antenna_set_id))
                app_configs.append((App.RxAntennaSelection, args.antenna_set_id))

            for i in app_configs:
                p = f"{i[0].name} ({hex(i[0])}):"
                try:
                    v = hex(i[1])
                except Exception:
                    v = repr(i[1])
                print(f"    {p:<35} {v}")

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
