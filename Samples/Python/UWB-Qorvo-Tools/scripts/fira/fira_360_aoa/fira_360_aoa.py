import argparse
import time
import datetime
import os
import sys
import json
import dataclasses
import math
import binascii
from queue import Queue

from uci import (
    Client,
    Gid,
    OidRanging,
    OidQorvo,
    App,
    SessionType,
    Status,
    UciComError,
    RangingData,
    RangingDiagData,
)
from uci.utils import S4_11
from uqt_utils.load_calibration import load_calibration
from . import json_to_np, display
from uqt_utils.utils import (
    uqt_errno,
    str2bytes,
)

epilog = """
Note:
    Multipurpose script for 2D 360 Aoa. Can be run with different options:
    -l, --load-calibration, load calibration file (-cf) and display current calibration values.
    -c, --calibrate, allow to perform mode 6 calibration to the QM35 device and set proper PDoA offsets,
    providing controller position (-np [-120, 0, 120]) is obligatory in this option. When calibration file
    not specified (-cf) default one will be used: scripts/device/load_cal/calib_files/QM35825DK/torus_360AoA.json.
    -r, --run-measurement, performs ranging measurement and saves results to a log file. This file can be
    later used with -d option to display the measured PDoA values and calculated 2D AoA of recorded measurement.
    -d, --display, show calculated 2D AoA based on the log file (-lf). LUT table can ba specified optionally (--lut)
"""

CH5_LUT_FILE = os.path.dirname(__file__) + "/torus_lut_ch5.npz"
CH9_LUT_FILE = os.path.dirname(__file__) + "/torus_lut_ch9.npz"
CALIBRATION_FILE = (
    os.path.dirname(__file__)
    + "/../../device/load_cal/calib_files/QM35825DK/torus_360AoA.json"
)

eng_ursk_prefix = "ed07a80d2beb00f785af2627"


class DataclassJSONEncoder(json.JSONEncoder):
    def default(self, o):
        if dataclasses.is_dataclass(o):
            return dataclasses.asdict(o)
        return super().default(o)


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


def setup_fira_session(
    opts: argparse.Namespace, client: Client, session_id: int, controlee: bool
):
    print(f"Initializing FiRa session {session_id}...")
    rts, session_handle = client.session_init(session_id, SessionType.Ranging)
    if rts != Status.Ok:
        print(f"session_init failed: {rts.name} ({rts})")
        return None

    if session_handle is None:
        print(f"Using Fira 1.3 (session handle == session ID) is : {session_id}")
        session_handle = session_id
    else:
        print(f"Using Fira 2.0 session handle is : {session_handle}")

    print(f"Setting FiRa session {session_handle} config ...")

    if isinstance(opts.vendor_id, str):
        opts.vendor_id = eval(opts.vendor_id)
    if isinstance(opts.static_sts, str):
        opts.static_sts = eval(opts.static_sts)

    default_URSK = (
        eng_ursk_prefix
        + eval(str(opts.session_id)).to_bytes(4, "big", signed=False).hex()
    )

    default_config = dict(
        # Fira Mandatory:
        mac="00:01" if opts.controlee else "00:00",  # DEVICE_MAC_ADDRESS
        # Other:
        report="tof|azimuth|elevation|fom",  # RESULT_REPORT_CONFIG
        vendor_id=[0x7, 0x8],  # VENDOR_ID
        static_sts=[0x1, 0x2, 0x3, 0x4, 0x5, 0x6],  # STATIC_STS_IV
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

    args = type("args", (), {})()
    args.__dict__.update(default_config)
    args.__dict__.update({k: v for k, v in vars(opts).items() if v is not None})
    # Handle user inputs & conversions
    try:
        user_mapping = dict(
            unicast=0,
            onetomany=1,
            manytomany=2,  # node
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
            time=1,
            hybrid=2,  # schedule
            tof=1,
            azimuth=2,
            elevation=4,
            fom=8,  # report
            static=0,
            dyn=1,
            dyn_key=2,
            provisioned=3,
            provisioned_key=4,  # sts
            disabled=0,
            enabled=1,  # hopping_mode
        )

        args.mac = list(str2bytes(args.mac, "little"))

        # in contention based ranging the dest_mac should not be set
        if args.schedule == "time":
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
            "round_ctrl",
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

    # Fira Mandatory/minimal session config:
    app_configs = [
        (App.DeviceType, 0 if controlee else 1),
        (App.DeviceRole, 0 if controlee else 1),
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

    if not controlee:
        app_configs.append((App.EnableDiagnostics, 1))
        app_configs.append((App.DiagsFrameReportsFields, args.diag_fields))
        app_configs.append((App.EnableUwbDiagnostics, args.uwb_diagnostics_flags))
        app_configs.append((App.AoaResultReq, 1))

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
        return None
    if args.init_time > 0:
        # Retrieve the current UWB timestamp
        rts, timestamp = client.get_time()
        if rts != Status.Ok:
            print(f"get_time failed: {rts.name} ({rts}).")
            print(f"{timestamp}")
            client.session_deinit(session_handle)
            return None
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
            return None

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
            return None

    return session_handle


def read_calibration_offsets(cal_file: str, channel: int):
    with open(cal_file, "r") as f:
        data = json.load(f)

    calibrations = data.get("calibrations", {})
    return [
        int(calibrations.get(f"ant_pair0.ch{channel}.pdoa.offset", None)),
        int(calibrations.get(f"ant_pair1.ch{channel}.pdoa.offset", None)),
        int(calibrations.get(f"ant_pair2.ch{channel}.pdoa.offset", None)),
    ]


def write_calibration_offsets(cal_file: str, channel: int, offsets: list[int]):
    with open(cal_file, "r") as f:
        data = json.load(f)

    calibrations = data.setdefault("calibrations", {})
    calibrations[f"ant_pair0.ch{channel}.pdoa.offset"] = str(offsets[0])
    calibrations[f"ant_pair1.ch{channel}.pdoa.offset"] = str(offsets[1])
    calibrations[f"ant_pair2.ch{channel}.pdoa.offset"] = str(offsets[2])

    with open(cal_file, "w") as f:
        json.dump(data, f, indent=2)


def run_calibration(opts: argparse.Namespace, client: Client):
    print("\nFiRa 360 AoA Calibration...\n")

    cal_positions = [-120, 0, 120]
    log_file = "calibration_report.json"

    for position in cal_positions:
        input(
            f"\nPlease set your controller position to: {position} degree and click enter, when ready..."
        )

        # set ant pair offset to zero to perform calibration
        ant_pair = cal_positions.index(position)
        offsets = read_calibration_offsets(CALIBRATION_FILE, opts.channel)
        offsets[ant_pair] = 0
        write_calibration_offsets(CALIBRATION_FILE, opts.channel, offsets)
        load_calibration(client=client, calibration_filename=CALIBRATION_FILE)

        # run calibration measurement
        input(
            "\nRun `fira_360_aoa -r -p <controlee_port> -t -1 --controlee` on second terminal and click enter to start calibration..."
        )
        run_measurement(opts, client, log_file=log_file)

        # calculate mean offset
        mean_offsets = json_to_np.JsonToNumpy(log_file).calculate_mean()
        if os.path.exists(log_file):
            os.remove(log_file)

        # save calculated offset to file and load calibration
        mean_offsets[ant_pair] = (mean_offsets[ant_pair] + 360) % 360  # normalize angle
        offsets[ant_pair] = S4_11(math.radians(mean_offsets[ant_pair])).as_int()
        write_calibration_offsets(CALIBRATION_FILE, opts.channel, offsets)
        print(
            f"\nCalibration for controller position: {position} degree complete! (pdoa_offset: {offsets[ant_pair]})\n"
        )

    print(f"\nOffset calibration done!\n")


def run_measurement(
    opts: argparse.Namespace,
    client: Client,
    log_file: str = None,
):
    session_handle = setup_fira_session(opts, client, opts.session_id, opts.controlee)
    if session_handle is None:
        sys.exit("Failed to setup FiRa session")

    print("Starting ranging...")
    rts = client.ranging_start(session_handle)
    if rts != Status.Ok:
        client.session_deinit(session_handle)
        sys.exit(f"ranging_start failed: {rts.name} ({rts})")

    if opts.time == -1:
        input("Press <RETURN> to stop\n")
    else:
        time.sleep(opts.time)

    print("Stopping ranging...")
    rts = client.ranging_stop(session_handle)
    if rts != Status.Ok:
        client.session_deinit(session_handle)
        sys.exit(f"ranging_stop failed: {rts.name} ({rts})")

    print("Deinitializing session...")
    rts = client.session_deinit(session_handle)
    if rts != Status.Ok:
        sys.exit(f"session_deinit failed: {rts.name} ({rts})")

    if rts == Status.Ok:
        print("Ok")

    diag_ntf = []

    try:
        while True:
            diag_ntf.append(diag_ntf_queue.get(timeout=1))
    except Exception:
        pass

    if diag_ntf:
        if not log_file:
            log_file = (
                "diagnostic_report_"
                f"{datetime.datetime.now().strftime('%y-%m-%d-%Hh%Mm%Ss')}.json"
            )
        with open(log_file, "x") as f:
            json.dump(diag_ntf, f, indent=4, cls=DataclassJSONEncoder)


def main():
    default_port = os.getenv("UQT_PORT", "/dev/ttyUSB0")
    parser = argparse.ArgumentParser(
        description="Allow to configure, calibrate and run setup for 2D Aoa using PDoA mode 6 calibration",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=epilog,
    )
    parser.add_argument(
        "--description",
        action="store_true",
        default=False,
        help="Communication port to use. (default: %(default)s)",
    )
    parser.add_argument(
        "-p",
        "--port",
        type=str,
        default=default_port,
        help="Communication port to use",
    )
    parser.add_argument(
        "-t",
        "--time",
        type=int,
        default=100,
        help="duration of the ranging session. -1: forever. (default: %(default)s)",
    )
    parser.add_argument(
        "-s",
        "--session-id",
        type=int,
        default=42,
        help="session id of the ranging session. (default: %(default)s)",
    )
    parser.add_argument(
        "-ch",
        "--channel",
        type=int,
        default=9,
        help="set the CHANNEL_NUMBER value. Only 5 and 9 supported in this mode (default: %(default)s)",
    )
    parser.add_argument(
        "--controlee",
        action="store_true",
        default=False,
        help="set the DEVICE_TYPE value and related default profile. (default: %(default)s)",
    )
    parser.add_argument(
        "-l",
        "--load-calibration",
        action="store_true",
        help="Load calibration for PDoA 6 measurement.",
    )
    parser.add_argument(
        "-c",
        "--calibrate",
        action="store_true",
        help="Perform calibration procedure for PDoA.",
    )
    parser.add_argument(
        "-r",
        "--run-measurement",
        action="store_true",
        help="Perform AoA measurement.",
    )
    parser.add_argument(
        "-d",
        "--display",
        action="store_true",
        help="Presents raw PDoA and calculated 2D AoA in graphical form.",
    )
    parser.add_argument(
        "-lf",
        "--log_file",
        type=str,
        default=None,
        help="Path to JSON log file.",
    )
    parser.add_argument(
        "--lut",
        type=str,
        default=None,
        help="Path to LUT file.",
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

    if opts.display:
        print("\nFiRa 360 AoA data presentation...\n")
        if opts.log_file:
            if opts.lut:
                display.main(["-lf", opts.log_file] + ["--lut", opts.lut])
            else:
                if opts.channel == 5:
                    lut = CH5_LUT_FILE
                elif opts.channel == 9:
                    lut = CH9_LUT_FILE
                else:
                    raise ValueError(
                        f"Channel {opts.channel} is not supported in this mode."
                    )
                display.main(["-lf", opts.log_file] + ["--lut", lut])
        else:
            raise ValueError(
                "Please provide log file (-lf argument) for presenting PDoA and 2D AoA."
            )
        sys.exit(0)

    notif_handlers = {
        (Gid.Ranging, OidRanging.Start): show_range_data_ntf,
        (Gid.Qorvo, OidQorvo.TestDiag): process_range_diagnostic_ntf,
    }

    try:
        client = Client(port=opts.port, notif_handlers=notif_handlers)
    except UciComError as e:
        raise Exception(f"Failed to connect to {opts.port}: {e}")

    if opts.load_calibration:
        load_calibration(client=client, calibration_filename=CALIBRATION_FILE)
    elif opts.calibrate or opts.run_measurement:
        if opts.channel not in [5, 9]:
            raise ValueError(f"Channel {opts.channel} is not supported in this mode.")
        if opts.calibrate:
            run_calibration(opts, client)
        elif opts.run_measurement:
            run_measurement(opts, client)
    else:
        raise ValueError(
            "Please specify correct argument:\n --calibrate (-c)\n --run-measurement (-r)\n --display (-d)\n --load-calibration (-l)"
        )

    if client:
        client.close()


if __name__ == "__main__":
    main()
