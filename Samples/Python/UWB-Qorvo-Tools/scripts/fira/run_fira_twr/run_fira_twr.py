#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

import argparse
import logging
import time
import os
import sys
import binascii

import dataclasses
import datetime
import json
from queue import Queue

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
from uqt_utils.ranging_stats import RangingStats
from uqt_utils.utils import (
    uqt_errno,
    str2bytes,
)


class DataclassJSONEncoder(json.JSONEncoder):
    def default(self, o):
        if dataclasses.is_dataclass(o):
            return dataclasses.asdict(o)
        return super().default(o)


eng_ursk_prefix = "ed07a80d2beb00f785af2627"

eng_ursk_prefix = "ed07a80d2beb00f785af2627"

# Below hack sometimes required when operating on windows git-bash/msys2
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8")

epilog = """
Note:
    - Default profiles:
        time-base scheduling.
        if --controlee:
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
    - Forever ranging (up to key pressed) on time-base scheduling:
      shell 1: run_fira_twr -p /dev/ttyUSB0 -t -1
      shell 2: run_fira_twr -p /dev/ttyUSB1 --controlee -t -1
    - Forever ranging (up to key pressed) on contention-base scheduling:
      shell 1: run_fira_twr -t 1 --schedule contention --node onetomany \
            --round-ctrl='cm|rcp' --round ss-non-deferred --frame sp1 -t -1 --controlee
      shell 2: run_fira_twr -t 1 --schedule contention --node onetomany \
            --round-ctrl 'cm|rcp' --round ss-non-deferred --frame sp1 -t -1
"""


def main():
    default_port = os.getenv("UQT_PORT", "/dev/ttyUSB0")
    parser = argparse.ArgumentParser(
        description="run a Fira two way ranging session.",
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
        help="set the DEVICE_TYPE value and related default profile. (default: %(default)s)\n"
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
        "--en-key-rot",
        action="store_true",
        default=False,
        help="set KEY_ROTATION to 1. (default: %(default)s)",
    )
    parser.add_argument(
        "--key-rot-rate", type=int, default=0, help="set KEY_ROTATION_RATE"
    )
    parser.add_argument(
        "--sts",
        choices=["static", "dyn", "dyn-key", "provisioned", "provisioned-key"],
        default="static",
        help="set the STS_CONFIG value. (default: %(default)s)\n"
        "Note: choosing provisioned-key for this parameter also requires setting:\n"
        "--skey and --controlees-with-sskey for Controller/Initiator\n"
        "--skey, --ssession and --sskey for Controlee/Responder",
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
        help="set the Qorvo ENABLE_DIAGNOSTIC parameter to 1. (default: %(default)s)",
    )
    parser.add_argument(
        "--diag-fields",
        type=str,
        default="metrics|aoa|cfo",
        help="set the Qorvo DIAGNOSTIC_FRAME_REPORTS_FIELD value.\n"
        "OR flags: metrics, aoa, cir, cfo, conf_raw_metrics. (default: %(default)s)",
    )
    parser.add_argument(
        "--uwb-diagnostics-flags",
        type=str,
        default="conf_metrics",
        help="set the Qorvo ENABLE_UWB_DIAGNOSTICS_FIELD value.\n"
        "OR flags: conf_metrics. (default: %(default)s)",
    )
    parser.add_argument(
        "--meas-max",
        type=int,
        default=0,
        help="set the MAX_NUMBER_OF_MEASUREMENTS value (0: unlimited)..\n"
        "(default: %(default)s)",
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
        help="set the Qorvo PSDU_DUMP value to 1. (default: %(default)s)",
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
        'default: ["00:01"] if controlee or ["00:01"].',
    )
    parser.add_argument(
        "--controlees-with-sskey",
        type=str,
        help="This parameter is used to trigger SESSION_UPDATE_CONTROLLER_MULTICAST_LIST_CMD\n"
        "needed for Provisioned STS for Responder specific Sub-Session Key.\n"
        "List of controlees is provided with the format below:\n"
        "[short_addr_1, ssID_1, ssKey_1 ..., short_addr_n, ssID_n, ssKey_n]\n"
        "with:\n"
        "    short_addr_i: short address of the Controlee\n"
        "    ssID_i: Controlee specific Sub-Session ID (Uint32)\n"
        "    ssKey_n: 16 or 32 bytes Sub-Session Key.\n"
        "        May be expressed as a space, dot or colon delimited list of bytes as below:\n"
        '        "000102030405060708090A0b0c0d0e0f"\n'
        '        "00.01.02.03.04.05.06.07.08.09.0A.0b.0c.0d.0e.0f"\n'
        '        "00 01 02 03 04 05 06 07 08 09 0A 0b 0c 0d 0e 0f"\n'
        '        "00:01:02:03:04:05:06:07:08:09:0A:0b:0c:0d:0e:0f"\n',
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
        "--dis-encryption",
        action="store_true",
        help="set MAC_PAYLOAD_ENCRYPTION to 0. Default: 1. \n"
        "(Warning: supported only on specific firmware.)",
    )
    parser.add_argument(
        "--stats",
        action="store_true",
        default=False,
        help="Enables Statistics report at end of the run. (default: %(default)s)",
    )
    parser.add_argument(
        "--diag_dump",
        action="store_true",
        default=False,
        help="Dump the Diagnostics into a JSON file following the naming scheme \n"
        '"diagnostic_data_<date>_<time>" in the directory where the script is executed. (default: %(default)s) \n'
        "(Info: This option is automatically enabling --en-diag and --stats options as well.)",
    )
    parser.add_argument(
        "--range_dump",
        action="store_true",
        default=False,
        help="Dump the Ranging measurements into a JSON file following the naming scheme \n"
        '"ranging_data_<date>_<time>" in the directory where the script is executed. (default: %(default)s) \n',
    )
    parser.add_argument(
        "--n_controlees",
        type=int,
        default=1,
        help="set the number of controlee in case of onetomany ranging. (default: %(default)s)",
    )
    parser.add_argument(
        "--block_stride_length",
        type=int,
        default=0,
        help="set the BLOCK_STRIDE_LENGTH value..\n" "(default: %(default)s)",
    )
    parser.add_argument(
        "--sts-length",
        type=int,
        choices=[0, 1, 2],
        default=1,
        help="Number of symbols in a STS segment. 0 = 32 symbols; 1 = 64 symbols; 2 = 128 symbols. (default: %(default)s)",
    )
    parser.add_argument(
        "--vendor-id",
        type=str,
        default="[0x07, 0x08]",
        help="Unique ID for a specific Vendor used to generate static STS (vUpper64[15:0]). (default: %(default)s)",
    )
    parser.add_argument(
        "--static-sts",
        type=str,
        default="[0x01, 0x02, 0x03, 0x04, 0x05, 0x06]",
        help="static sts value. (default: %(default)s)",
    )
    parser.add_argument(
        "--aoa-report",
        choices=["all-disabled", "all-enabled", "azimuth-only", "elevation-only"],
        default="all-enabled",
        help="aoa report value. (default: %(default)s)",
    )
    parser.add_argument(
        "--init-time",
        type=int,
        default=0,
        help="init time value in us. (default: %(default)s)",
    )
    parser.add_argument(
        "--preamble-idx",
        type=int,
        default=10,
        help="preamble value. (default: %(default)s)",
    )
    parser.add_argument(
        "--preamble-duration",
        type=int,
        choices=[0, 1],
        default=1,
        help="Preamble duration. 0 = 32 symbols; 1 = 64 symbols. (default: %(default)s)",
    )
    parser.add_argument(
        "--nb-sts-segments",
        type=int,
        choices=[0, 1, 2, 3, 4],
        default=1,
        help="Number of STS segments in the frame. (default: %(default)s)",
    )
    parser.add_argument(
        "--sfd", type=int, default=2, help="sfd value. (default: %(default)s)"
    )
    parser.add_argument(
        "--slots-per-rr",
        type=int,
        default=25,
        help="Number of slots in a ranging round. (default: %(default)s)",
    )
    parser.add_argument(
        "--hopping-mode",
        choices=["disabled", "enabled"],
        default="disabled",
        help="hopping mode value. (default: %(default)s)",
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

    opts = parser.parse_args()

    if opts.description:
        print(parser.description)
        sys.exit(0)

    if isinstance(opts.vendor_id, str):
        opts.vendor_id = eval(opts.vendor_id)
    if isinstance(opts.static_sts, str):
        opts.static_sts = eval(opts.static_sts)

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
        vendor_id=[0x7, 0x8],  # VENDOR_ID
        static_sts=[0x1, 0x2, 0x3, 0x4, 0x5, 0x6],  # STATIC_STS_IV
        aoa_report="all-enabled",  # AOA_RESULT_REQ
        init_time=0,  # UWB_INITIATION_TIME
        preamble_idx=10,  # PREAMBLE_CODE_INDEX
        sfd=2,  # SFD_ID
        slots_per_rr=25,  # SLOTS_PER_RR
        hopping_mode="disabled",  # HOPPING_MODE
        sts="static",  # STS_CONFIG
    )
    if opts.sts in ["dyn-key", "provisioned", "provisioned-key"]:
        default_config["skey"] = default_URSK
    if opts.schedule == "time":
        default_config["n_controlees"] = 1  # NUMBER_OF_CONTROLEES
        default_config["dest_mac"] = (
            "['00:00']" if opts.controlee else "['00:01']"
        )  # DEST_MAC-ADDRESS

    # Enable en-diag and stats parameters when diag_dump is enabled
    if opts.diag_dump:
        opts.en_diag = True
        opts.stats = True

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
            cir=0x40,
            conf_raw_metrics=0x80,  # diag_fields
            conf_metrics=0x400,  # uwb_diagnostic_flags
            ut=0,
            ss_deferred=1,
            ds_deferred=2,
            ss_non_deferred=3,
            ds_non_deferred=4,
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
            all_disabled=0,
            all_enabled=1,
            azimuth_only=2,
            elevation_only=3,  # aoa_report
            disabled=0,
            enabled=1,  # hopping_mode
        )

        args.mac = list(str2bytes(args.mac, "little"))

        if "dest_mac" in vars(args):
            args.dest_mac = list(
                int.from_bytes(str2bytes(v), "big") for v in eval(args.dest_mac)
            )

        for p in (
            "session",
            "controlees_with_sskey",
            "ssession",
            "cap_range",
            "diag_fields",
            "uwb_diagnostics_flags",
            "round",
            "frame",
            "schedule",
            "report",
            "sts",
            "node",
            "device",
            "round_ctrl",
            "aoa_report",
            "hopping_mode",
        ):
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

    if opts.stats:
        range_ntf_queue = Queue()
        diag_ntf_queue = Queue()

        def show_range_data_ntf(payload):
            try:
                decoded_ntf = RangingData(payload)
                range_ntf_queue.put(decoded_ntf)
            except Exception as exp:
                ntf_message = (
                    f"<{RangingData.__name__} - decode "
                    + f"error: >> {exp} << for payload {payload}>"
                )
            else:
                ntf_message = f"{decoded_ntf}"

            print(ntf_message)

        def process_range_diagnostic_ntf(payload):
            r = RangingDiagData(payload)
            diag_ntf_queue.put(r)
            print(r)

        notif_handlers = {
            (Gid.Ranging, OidRanging.Start): show_range_data_ntf,
            (Gid.Qorvo, OidQorvo.TestDiag): process_range_diagnostic_ntf,
            (Gid.Qorvo, OidQorvo.PSDUSReport): lambda x: print(RangingPSDUSReport(x)),
            ("default", "default"): lambda gid, oid, x: print(
                NotImplementedData(gid, oid, x)
            ),
        }
    else:
        notif_handlers = {}  # use default

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

            print(f"Setting session {session_handle} config ...")

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
                (App.VendorId, args.vendor_id),
                (App.StaticStsIv, args.static_sts),
                (App.AoaResultReq, args.aoa_report),
                (App.UwbInitiationTime, args.init_time),
                (App.PreambleCodeIndex, args.preamble_idx),
                (App.SfdId, args.sfd),
                (App.SlotDuration, args.slot_span),
                (App.RangingDuration, args.ranging_span),
                (App.SlotsPerRr, args.slots_per_rr),
                (App.MaxNumberOfMeasurements, args.meas_max),
                (App.HoppingMode, args.hopping_mode),
                (App.RssiReporting, 1 if args.en_rssi else 0),
                (App.BlockStrideLength, args.block_stride_length),
                (App.PsduDataRate, args.psdu_data_rate),
                (App.PrfMode, args.prf_mode),
                (App.PreambleDuration, args.preamble_duration),
                (App.NumberOfStsSegments, args.nb_sts_segments),
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
                        (App.EnableUwbDiagnostics, args.uwb_diagnostics_flags),
                    ]
                )
            if args.en_psdu_dump:
                app_configs.append((App.EnablePSDUDump, 1))
            if args.dis_encryption:
                app_configs.append((App.MacPayloadEncryption, 0))
            if "skey" in vars(args):
                app_configs.append((App.SessionKey, args.skey))
            if "sskey" in vars(args):
                app_configs.append((App.SubSessionKey, args.sskey))
            if "sts_length" in vars(args):
                app_configs.append((App.StsLength, args.sts_length))
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

            rts, rtv = client.session_set_app_config(session_handle, app_configs)
            if rts != Status.Ok:
                print(f"session_set_app_config failed: {rts.name} ({rts}).")
                print(f"{rtv}")
                client.session_deinit(session_handle)
                break
            if args.init_time > 0:
                # Retrieve the current UWB timestamp
                rts, timestamp = client.get_time()
                if rts != Status.Ok:
                    print(f"get_time failed: {rts.name} ({rts}).")
                    print(f"{timestamp}")
                    client.session_deinit(session_handle)
                    break
                time.sleep(0.01)
                print(
                    "# setting the proper UwbInitiationTime to:"
                    + str(timestamp + args.init_time + 13000)
                )
                # UwbInitiationTime is set to the timestamp + init_time + 13000,
                # where 13ms is the processing delay.
                rts, rtv = client.session_set_app_config(
                    session_handle,
                    [
                        (App.UwbInitiationTime, timestamp + args.init_time + 13000),
                    ],
                )
                if rts != Status.Ok:
                    print(f"session_set_app_config failed: {rts.name} ({rts}).")
                    print(f"{rtv}")
                    client.session_deinit(session_handle)
                    break

            if "controlees_with_sskey" in vars(args):
                try:
                    controlees_with_sskey = args.controlees_with_sskey
                    if len(controlees_with_sskey) % 3 != 0:
                        raise SyntaxError(
                            f"Syntax error in the parameter controlees-with-sskey. Got {args.controlees_with_sskey!r}"
                        )
                    for i in range(len(controlees_with_sskey) // 3):
                        k = controlees_with_sskey[3 * i + 2]
                        if isinstance(k, str):
                            controlees_with_sskey[3 * i + 2] = binascii.unhexlify(
                                k.replace(" ", "").replace(":", "").replace(".", "")
                            )
                            sskey_length = len(controlees_with_sskey[3 * i + 2])
                            if sskey_length == 16:
                                action = 0x02
                            elif sskey_length == 32:
                                action = 0x03
                            else:
                                raise SyntaxError(
                                    f"Syntax error in the parameter controlees-with-sskey. Got {args.controlees_with_sskey!r}"
                                )
                except Exception as e:
                    print(f"Error while handling user input: {e}")
                    sys.exit(uqt_errno(2))
                print("Updating the multicast list of controlees...")
                rts, rtv = client.session_update_multicast_list(
                    session_handle, action, controlees_with_sskey
                )
                print(f"session_update_multicast_list: {rts.name} ({rts}).")
                if rts != Status.Ok:
                    print(f"{rtv}")
                    client.session_deinit(session_handle)
                    break

            print("Starting ranging...")
            rts = client.ranging_start(session_handle)
            if rts != Status.Ok:
                print(f"ranging_start failed: {rts.name} ({rts})")
                client.session_deinit(session_handle)
                break

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

            print("Deinitializing session...")
            rts = client.session_deinit(session_handle)
            if rts != Status.Ok:
                print(f"session_deinit failed: {rts.name} ({rts})")
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

    if opts.stats:

        range_ntf = []
        diag_ntf = []

        try:
            while True:
                range_ntf.append(range_ntf_queue.get(timeout=1))
        except Exception:
            pass

        try:
            while True:
                diag_ntf.append(diag_ntf_queue.get(timeout=1))
        except Exception:
            pass

        if diag_ntf and opts.diag_dump:
            log_name = (
                "diagnostic_data_"
                f"{datetime.datetime.now().strftime('%y-%m-%d-%Hh%Mm%Ss')}.json"
            )
            with open(log_name, "x") as f:
                json.dump(diag_ntf, f, indent=4, cls=DataclassJSONEncoder)

        if range_ntf and opts.range_dump:
            log_name = (
                "ranging_data_"
                f"{datetime.datetime.now().strftime('%y-%m-%d-%Hh%Mm%Ss')}.json"
            )
            with open(log_name, "x") as f:
                json.dump(range_ntf, f, indent=4, cls=DataclassJSONEncoder)

        stats = RangingStats(range_ntf, diag_ntf)

        print(stats)

    sys.exit(uqt_errno(rts))


if __name__ == "__main__":
    main()
