#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

import argparse
import logging
import time
import os
import sys
import binascii
import queue
import numpy as np
import threading

import dataclasses
import datetime
import json
from queue import Queue

from uci import (
    Gid,
    OidRanging,
    OidQorvo,
    Status,
    SessionType,
    UciComError,
    App,
    RangingData,
    RangingPSDUSReport,
    RangingDiagData,
    NotImplementedData,
)
from uqt_utils.radar import (
    RadarClient,
    RadarDataEncoder,
)

from uqt_utils.ranging_stats import RangingStats
from uqt_utils.load_calibration import load_calibration
from uqt_utils.utils import uqt_errno, str2bytes
from uci.utils import Int16

eng_ursk_prefix = "ed07a80d2beb00f785af2627"


class DataclassJSONEncoder(json.JSONEncoder):
    def default(self, o):
        if dataclasses.is_dataclass(o):
            return dataclasses.asdict(o)
        return super().default(o)


class NpEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, np.ndarray):
            return obj.tolist()
        return json.JSONEncoder.default(self, obj)


# Below hack sometimes required when operating on windows git-bash/msys2
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8")

epilog = """
The goal of this script is to perform functional interleaving between a Radar session and FiRa TWR session.
Example of use:
    - Minimum use-case: Forever ranging (up to key pressed) on time-base scheduling, with radar done by the controller:
        shell 1: radar_fira_interleaving -p /dev/ttyUSB0 -t -1
        shell 2: run_fira_twr -p /dev/ttyUSB1 --controlee -t -1
"""


class Runner(threading.Thread):
    def __init__(self, port, qtraces_logfile, logger, args):
        super().__init__()
        self.range_ntf_queue = Queue()
        self.diag_ntf_queue = Queue()
        self.frames = {"Configuration": [], "Frames": []}
        self.f_session_handle = None
        self.running = False
        self.args = args

        try:
            notif_handlers = {
                (Gid.Ranging, OidRanging.Start): self.show_range_data_ntf,
                (Gid.Qorvo, OidQorvo.TestDiag): self.process_range_diagnostic_ntf,
                (Gid.Qorvo, OidQorvo.PSDUSReport): lambda x: print(
                    RangingPSDUSReport(x)
                ),
                ("default", "default"): lambda gid, oid, x: print(
                    NotImplementedData(gid, oid, x)
                ),
            }
            self.radar_client = RadarClient(
                port=port,
                notif_handlers=notif_handlers,
                qtraces_logfile=qtraces_logfile,
            )

            # Because it is only possible to have one client at a time
            # and the radar client uses a fira client, it is possible
            # to access the fira client functions like so:
            self.fira_client = self.radar_client.get_client()
        except UciComError as e:
            logger.critical(f"{e}")

    def setup_calibration(self):
        print(f"Loading calibration from file {self.args.calibration}...")
        rts = load_calibration(self.fira_client, self.args.calibration)
        if rts != Status.Ok:
            raise Exception(f"Calibration failed: {rts.name} ({rts})")

        return Status.Ok

    def setup_fira_session(self):
        print(f"Initializing FiRa session with id: {self.args.fira_session}...")
        rts, self.f_session_handle = self.fira_client.session_init(
            self.args.fira_session, SessionType.Ranging
        )
        if rts != Status.Ok:
            raise Exception(f"FiRa session init failed: {rts.name} ({rts})")

        if self.f_session_handle is None:
            print(
                f"Using Fira 1.3 (session handle == session ID) is : {self.args.fira_session}"
            )
            self.f_session_handle = self.args.fira_session
        else:
            print(f"Using Fira 2.0 session handle is : {self.f_session_handle}")

        print(f"Setting FiRa session {self.f_session_handle} config ...")

        # Fira Mandatory/minimal session config:
        app_configs = [
            (App.DeviceType, self.args.device),
            (App.DeviceRole, 0 if self.args.controlee else 1),
            (App.MultiNodeMode, self.args.node),
            (App.RangingRoundUsage, self.args.round),
            (App.DeviceMacAddress, self.args.mac),
            # Additional config:
            (App.ChannelNumber, self.args.channel),
            (App.ScheduleMode, self.args.schedule),
            (App.CapSizeRange, self.args.cap_range),
            (App.StsConfig, self.args.sts),
            (App.RframeConfig, self.args.frame),
            (App.ResultReportConfig, self.args.report),
            (App.VendorId, self.args.vendor_id),
            (App.StaticStsIv, self.args.static_sts),
            (App.AoaResultReq, self.args.aoa_report),
            (App.UwbInitiationTime, self.args.init_time),
            (App.PreambleCodeIndex, self.args.preamble_idx),
            (App.SfdId, self.args.sfd),
            (App.SlotDuration, self.args.slot_span),
            (App.RangingDuration, self.args.ranging_span),
            (App.SlotsPerRr, self.args.slots_per_rr),
            (App.MaxNumberOfMeasurements, self.args.meas_max),
            (App.HoppingMode, self.args.hopping_mode),
            (App.RssiReporting, 1 if self.args.en_rssi else 0),
            (App.BlockStrideLength, self.args.block_stride_length),
            (App.PsduDataRate, self.args.psdu_data_rate),
            (App.PrfMode, self.args.prf_mode),
            (App.PreambleDuration, self.args.preamble_duration),
            (App.NumberOfStsSegments, self.args.nb_sts_segments),
        ]
        if "ssession" in vars(self.args):
            app_configs.append((App.SubSessionId, self.args.ssession))

        # in contention based ranging the n_controlees should not be set
        if "n_controlees" in vars(self.args) and self.args.schedule == "time":
            app_configs.append((App.NumberOfControlees, self.args.n_controlees))
        if "dest_mac" in vars(self.args):
            app_configs.append((App.DstMacAddress, self.args.dest_mac))
        if "round_ctrl" in vars(self.args):
            app_configs.append((App.RangingRoundControl, self.args.round_ctrl))
        if self.args.en_key_rot:
            app_configs.append((App.KeyRotation, 1))
        if "key_rot_rate" in vars(self.args):
            app_configs.append((App.KeyRotationRate, self.args.key_rot_rate))
        if self.args.en_diag:
            app_configs.extend(
                [
                    (App.EnableDiagnostics, 1),
                    (App.DiagsFrameReportsFields, self.args.diag_fields),
                    (App.EnableUwbDiagnostics, self.args.uwb_diagnostics_flags),
                ]
            )
        if self.args.en_psdu_dump:
            app_configs.append((App.EnablePSDUDump, 1))
        if self.args.dis_encryption:
            app_configs.append((App.MacPayloadEncryption, 0))
        if "skey" in vars(self.args):
            app_configs.append((App.SessionKey, self.args.skey))
        if "sskey" in vars(self.args):
            app_configs.append((App.SubSessionKey, self.args.sskey))
        if "sts_length" in vars(self.args):
            app_configs.append((App.StsLength, self.args.sts_length))
        if "antenna_set_id" in vars(self.args):
            app_configs.append((App.TxAntennaSelection, self.args.antenna_set_id))
            app_configs.append((App.RxAntennaSelection, self.args.antenna_set_id))

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

        rts, rtv = self.fira_client.session_set_app_config(
            self.f_session_handle, app_configs
        )
        if rts != Status.Ok:
            self.fira_client.session_deinit(self.f_session_handle)
            raise Exception(
                f"FiRa session_set_app_config failed: {rts.name} ({rts}). \n {rtv}"
            )

        if self.args.init_time > 0:
            # Retrieve the current UWB timestamp
            rts, timestamp = self.fira_client.get_time()
            if rts != Status.Ok:
                self.fira_client.session_deinit(self.f_session_handle)
                raise Exception(f"get_time failed: {rts.name} ({rts}). \n {timestamp}")

            time.sleep(0.01)
            print(
                "# setting the proper UwbInitiationTime to:"
                + str(timestamp + self.args.init_time + 13000)
            )
            # UwbInitiationTime is set to the timestamp + init_time + 13000,
            # where 13ms is the processing delay.
            rts, rtv = self.fira_client.session_set_app_config(
                self.f_session_handle,
                [
                    (App.UwbInitiationTime, timestamp + self.args.init_time + 13000),
                ],
            )
            if rts != Status.Ok:
                self.fira_client.session_deinit(self.f_session_handle)
                raise Exception(
                    f"session_set_app_config for UwbInitiationTime failed: {rts.name} ({rts}). \n {rtv}"
                )

        if "controlees_with_sskey" in vars(self.args):
            try:
                controlees_with_sskey = self.args.controlees_with_sskey
                if len(controlees_with_sskey) % 3 != 0:
                    raise SyntaxError(
                        f"Syntax error in the parameter controlees-with-sskey. Got {self.args.controlees_with_sskey!r}"
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
                                f"Syntax error in the parameter controlees-with-sskey. Got {self.args.controlees_with_sskey!r}"
                            )
            except Exception as e:
                print(f"Error while handling user input: {e}")
                sys.exit(uqt_errno(2))

            print("Updating the multicast list of controlees...")
            rts, rtv = self.fira_client.session_update_multicast_list(
                self.f_session_handle, action, controlees_with_sskey
            )
            print(f"session_update_multicast_list: {rts.name} ({rts}).")
            if rts != Status.Ok:
                self.fira_client.session_deinit(self.f_session_handle)
                raise Exception(f"session_update_multicast_list failed: {rtv}")

        return self.f_session_handle

    def setup_radar_session(self):
        print(f"Initializing Radar session with id: {self.args.radar_session}...")
        rts, r_session_handle = self.radar_client.radar_enable(self.args.radar_session)
        if rts != Status.Ok:
            raise Exception(f"Radar session init failed: {rts.name} ({rts})")

        timing_params = {
            "burst_period_ms": self.args.radar_burst_period_ms,
            "sweep_period_rstu": 0,
            "sweeps_per_burst": 1,
        }

        radar_settings = {
            "ChannelNumber": self.args.channel,
            "RframeConfig": self.args.radar_frame,
            "PreambleCodeIndex": self.args.radar_preamble_idx,
            "PreambleDuration": self.args.radar_preamble_duration,
            "SessionPriority": 70,
            "TimingParams": timing_params,
            "SamplesPerSweep": self.args.radar_samples_per_sweep,
            "SweepOffset": Int16(self.args.radar_sweep_offset),
            "BitsPerSample": 1,
            "NumberOfBursts": self.args.radar_number_of_bursts,
            "RadarDataType": 0,
            "AntennaSetId": self.args.radar_antenna_set_id,
            "TxProfileIdx": self.args.radar_tx_profile_idx,
        }

        self.frames["Configuration"] = radar_settings

        print(f"Setting Radar session {r_session_handle} config ...")
        for config, value in self.frames["Configuration"].items():
            print(f"{config} : {value}")

        rts = self.radar_client.radar_set_config(radar_settings)
        if rts != Status.Ok:
            self.radar_client.radar_disable(r_session_handle)
            raise Exception(f"Radar session_set_app_config failed: {rts.name} ({rts}).")

        return r_session_handle

    def start_fira_session(self):
        print("Starting FiRa session...")
        rts = self.fira_client.ranging_start(self.f_session_handle)
        if rts != Status.Ok:
            self.fira_client.session_deinit(self.f_session_handle)
            raise Exception(f"FiRa session_start failed: {rts.name} ({rts})")

    def start_radar_session(self):
        print("Starting Radar session...")
        rts = self.radar_client.radar_start()
        if rts != Status.Ok:
            self.radar_client.radar_disable()
            raise Exception(f"Radar start failed: {rts.name} ({rts})")

    def stop_fira_session(self):
        print("Stopping FiRa session...")
        rts = self.fira_client.ranging_stop(self.f_session_handle)
        if rts != Status.Ok:
            self.fira_client.session_deinit(self.f_session_handle)
            raise Exception(f"FiRa session_stop failed: {rts.name} ({rts})")

        print("Deinitializing FiRa session...")
        rts = self.fira_client.session_deinit(self.f_session_handle)
        if rts != Status.Ok:
            raise Exception(f"FiRa session_deinit failed: {rts.name} ({rts})")

    def stop_radar_session(self):
        print("Stopping time...")
        self.radar_stop_time = time.time()

        print("Stopping Radar session...")
        rts = self.radar_client.radar_stop()
        if rts != Status.Ok:
            raise Exception(f"Radar stop failed: {rts.name} ({rts})")

        rts = self.radar_client.radar_disable()
        if rts != Status.Ok:
            raise Exception(f"Radar disable failed: {rts.name} ({rts})")

    def run(self):
        try:
            self.nb_frames_lost = 0
            self.nb_frames = 0
            self.running = True
            self.start_fira_session()
            self.start_radar_session()
            self.radar_start_time = time.time()

            while self.running:
                try:
                    entry = self.radar_client.radar_get_data()
                except queue.Empty:
                    print("Radar: No CIR in queue \n")
                    continue

                if entry:
                    sweep = entry.sweep_data[0]
                    try:
                        self.nb_frames_lost += sweep.sequence_number - self.seq_n - 1
                        self.seq_n = sweep.sequence_number
                    except:
                        self.seq_n = sweep.sequence_number

                    self.frames["Frames"].append(entry)
                    self.nb_frames += 1
        except UciComError as e:
            print(f"Communication error: {e}")
            sys.exit(-1)

    def stop(self):
        try:
            print("Stopping Radar FiRa interleaving...")
            self.running = False

            self.stop_fira_session()
            self.stop_radar_session()

            print("Stopped...")
        except Exception as e:
            print(f"Error while trying to stop interleaving: {e}")
            sys.exit(uqt_errno(2))

    def close_client(self):
        print("Closing client...")
        self.fira_client.close()

    def show_range_data_ntf(self, payload):
        try:
            decoded_ntf = RangingData(payload)
            self.range_ntf_queue.put(decoded_ntf)
        except Exception:
            pass

    def process_range_diagnostic_ntf(self, payload):
        r = RangingDiagData(payload)
        try:
            # r = dataclasses.asdict(r)
            self.diag_ntf_queue.put(r)
            # print(r)
        except Exception:
            pass

    def save_twr_data_to_json_file(self, range_name, diag_name):
        range_ntf = []
        diag_ntf = []

        try:
            while True:
                range_ntf.append(self.range_ntf_queue.get(timeout=1))
        except Exception:
            pass

        try:
            while True:
                diag_ntf.append(self.diag_ntf_queue.get(timeout=1))
        except Exception:
            pass

        if len(range_ntf):
            os.makedirs(os.path.dirname(range_name), exist_ok=True)
            with open(range_name, "w+") as f:
                print(f"Saving range NTF data to json file: {range_name}")
                json.dump(range_ntf, f, indent=4, cls=DataclassJSONEncoder)

        if len(diag_ntf):
            os.makedirs(os.path.dirname(diag_name), exist_ok=True)
            with open(diag_name, "w+") as f:
                print(f"Saving diagnostics NTF data to json file: {diag_name}")
                json.dump(diag_ntf, f, indent=4, cls=DataclassJSONEncoder)
        try:
            stats = RangingStats(range_ntf, diag_ntf)
            print("\n*************TWR STATISTICS******************")
            print(stats)
            print("*********************************************\n")
        except Exception:
            pass

    def save_cir_data_to_json_file(self, path_to_file: str):
        print(f"Saving CIR data to json file: {path_to_file}")

        os.makedirs(os.path.dirname(path_to_file), exist_ok=True)
        with open(path_to_file, "w+") as f:
            json_data = json.dumps(self.frames, indent=4, cls=RadarDataEncoder)
            f.write(json_data)
        if self.radar_start_time:
            print("\n*************RADAR STATISTICS****************")
            print(
                f"{self.nb_frames} frames in\
            {self.radar_stop_time - self.radar_start_time}s ({self.nb_frames/(self.radar_stop_time-self.radar_start_time)} fps)"
            )
            print(f"{self.nb_frames_lost} frames lost")
            print("*********************************************\n")


def main():
    default_port = os.getenv("UQT_PORT", "/dev/ttyUSB0")
    parser = argparse.ArgumentParser(
        description="run a Fira TWR and Radar Interleaving session.",
        formatter_class=argparse.RawTextHelpFormatter,
        epilog=epilog,
    )
    parser.add_argument(
        "--description",
        action="store_true",
        help="Show short description of the script",
        default=False,
    )
    parser.add_argument(
        "-p",
        "--port",
        type=str,
        default=default_port,
        help="Communication port to use. (default: %(default)s)",
    )
    parser.add_argument(
        "-t",
        "--time",
        type=int,
        default=10,
        help="Duration of the FiRa and Radar sessions (in seconds). -1: forever. (default: %(default)s)",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        default=False,
        help="Use logging. DEBUG level. (default: %(default)s)",
    )
    parser.add_argument(
        "-s",
        "--fira-session",
        type=str,
        default="42",
        help="Set the FiRa session id to use. (default: %(default)s)",
    )
    parser.add_argument(
        "-c",
        "--channel",
        type=int,
        default=9,
        help="Set the CHANNEL_NUMBER value of the FiRa session. (default: %(default)s)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=str,
        default=os.path.dirname(__file__) + "/data",
    )
    parser.add_argument(
        "--calibration",
        type=str,
        default=os.path.dirname(__file__)
        + "/../../device/load_cal/calib_files/QM35825DK/jolie_quad_radar_TWR_180AoA.json",
        help="Calibration file. (default: %(default)s)",
    )
    parser.add_argument(
        "--controlee",
        action="store_true",
        default=False,
        help="Set the DEVICE_TYPE value of the FiRa session. (default: %(default)s)\n"
        "use -h to review the default profiles.)",
    )
    parser.add_argument(
        "--round",
        choices=["ss-deferred", "ds-deferred", "ss-non-deferred", "ds-non-deferred"],
        default="ds-deferred",
        help="Set the RANGING_ROUND_USAGE value of the FiRa session. (default: %(default)s)",
    )
    parser.add_argument(
        "--round-ctrl",
        type=str,
        help="Set the RANGING_ROUND_CONTROL values of the FiRa session.\n"
        "OR flags: rrrm, cm, rcp, mrp, mrm (default: %(default)s)",
    )
    parser.add_argument(
        "--en-key-rot",
        action="store_true",
        default=False,
        help="Set KEY_ROTATION to 1 of the FiRa session. (default: %(default)s)",
    )
    parser.add_argument(
        "--key-rot-rate",
        type=int,
        default=0,
        help="Set KEY_ROTATION_RATE of the FiRa session",
    )
    parser.add_argument(
        "--sts",
        choices=["static", "dyn", "dyn-key", "provisioned", "provisioned-key"],
        default="static",
        help="Set the STS_CONFIG value of the FiRa session. (default: %(default)s)\n"
        "Note: choosing provisioned-key for this parameter also requires setting:\n"
        "--skey and --controlees-with-sskey for Controller/Initiator\n"
        "--skey, --ssession and --sskey for Controlee/Responder",
    )
    parser.add_argument(
        "--slot-span",
        type=int,
        default=2400,
        help="Set the SLOT_DURATION value of the FiRa session. (default: %(default)s)",
    )
    parser.add_argument(
        "--node",
        choices=dict(unicast=0, onetomany=1, manytomany=2),
        default="unicast",
        help="Set the MULTI_NODE_MODE value of the FiRa session. (default: %(default)s)",
    )
    parser.add_argument(
        "--ranging-span",
        type=int,
        default=200,
        help="Set the RANGING_DURATION param of the FiRa session. (default: %(default)s)\n"
        "(previously RANGING_INTERVAL)",
    )
    parser.add_argument(
        "--en-diag",
        action="store_true",
        default=False,
        help="Set the Qorvo ENABLE_DIAGNOSTIC parameter to 1. (default: %(default)s)",
    )
    parser.add_argument(
        "--diag-fields",
        type=str,
        default="metrics|aoa|cir",
        help="Set the Qorvo DIAGNOSTIC_FRAME_REPORTS_FIELD value.\n"
        "OR flags: metrics, aoa, cir, cfo, conf_raw_metrics. (default: %(default)s)",
    )
    parser.add_argument(
        "--uwb-diagnostics-flags",
        type=str,
        default="conf_metrics",
        help="Set the Qorvo ENABLE_UWB_DIAGNOSTICS_FIELD value.\n"
        "OR flags: conf_metrics. (default: %(default)s)",
    )
    parser.add_argument(
        "--meas-max",
        type=int,
        default=0,
        help="Set the MAX_NUMBER_OF_MEASUREMENTS value (0: unlimited).\n"
        "(default: %(default)s)",
    )
    parser.add_argument(
        "--skey",
        type=str,
        help="Set the SESSION_KEY 16 or 32 bytes value.\n"
        '"default" is an accepted value (see help)',
    )
    parser.add_argument(
        "--en-psdu-dump",
        action="store_true",
        default=False,
        help="Set the Qorvo PSDU_DUMP value to 1. (default: %(default)s)",
    )
    parser.add_argument(
        "--schedule",
        choices=["contention", "time"],
        default="time",
        help="Set the SCHEDULE_MODE value of the FiRa session. (default: %(default)s)",
    )
    parser.add_argument(
        "--cap-range",
        type=str,
        default="0x0510",
        help="Set the CAP_SIZE_RANGE value of the FiRa session, which is a 2 bytes word. (default: %(default)s)\n"
        "MSB: minimum cap size (default:5).\n"
        "LSB: maximum (Default: SLOTS_PER_RR-1). Default: 0x10",
    )
    parser.add_argument(
        "--mac",
        type=str,
        help="Set the DEVICE_MAC_ADDRESS value.\n"
        "default: 00:00 if controlee else 00:01.",
    )
    parser.add_argument(
        "--dest-mac",
        type=str,
        help="Set the DST_MAC_ADDRESS value of the FiRa session, which is a list.\n"
        'default: ["00:01"] if controlee or ["00:01"].',
    )
    parser.add_argument(
        "--controlees-with-sskey",
        type=str,
        help="This parameter is used to trigger SESSION_UPDATE_CONTROLLER_MULTICAST_LIST_CMD\n"
        "needed for Provisioned STS for Responder specific Sub-Session Key of the FiRa session.\n"
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
        help="Set the RFRAME_CONFIG value of the FiRa session. Frame Config. (default: %(default)s)",
    )
    parser.add_argument("--ssession", type=str, help="Set the SUB_SESSION_ID value.")
    parser.add_argument(
        "--sskey",
        type=str,
        help="Set the SUB_SESSION_KEY 16 or 32 bytes value.\n"
        '"default" is an accepted value (see help)',
    )
    parser.add_argument(
        "--en-rssi",
        action="store_true",
        default=False,
        help="Set the RSSI_REPORTING value to 1 of the FiRa session. (default: %(default)s)",
    )
    parser.add_argument(
        "--dis-encryption",
        action="store_true",
        help="Set MAC_PAYLOAD_ENCRYPTION to 0. Default: 1. \n"
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
        help="Set the number of controlee in case of onetomany ranging of the FiRa session. (default: %(default)s)",
    )
    parser.add_argument(
        "--block_stride_length",
        type=int,
        default=0,
        help="Set the BLOCK_STRIDE_LENGTH value of the FiRa session.\n"
        "(default: %(default)s)",
    )
    parser.add_argument(
        "--sts-length",
        type=int,
        choices=[0, 1, 2],
        default=1,
        help="Number of symbols in a STS segment of the FiRa session. 0 = 32 symbols; 1 = 64 symbols; 2 = 128 symbols. (default: %(default)s)",
    )
    parser.add_argument(
        "--vendor-id",
        type=str,
        default="[0x07, 0x08]",
        help="Unique ID for a specific Vendor used to generate static STS (vUpper64[15:0]) of the FiRa session. (default: %(default)s)",
    )
    parser.add_argument(
        "--static-sts",
        type=str,
        default="[0x01, 0x02, 0x03, 0x04, 0x05, 0x06]",
        help="Static sts value of the FiRa session. (default: %(default)s)",
    )
    parser.add_argument(
        "--aoa-report",
        choices=["all-disabled", "all-enabled", "azimuth-only", "elevation-only"],
        default="all-enabled",
        help="Aoa report value of the FiRa session. (default: %(default)s)",
    )
    parser.add_argument(
        "--init-time",
        type=int,
        default=0,
        help="Init time value in us of the FiRa session. (default: %(default)s)",
    )
    parser.add_argument(
        "--preamble-idx",
        type=int,
        default=10,
        help="Preamble value of the FiRa session. (default: %(default)s)",
    )
    parser.add_argument(
        "--preamble-duration",
        type=int,
        choices=[0, 1],
        default=1,
        help="Preamble duration of the FiRa session. 0 = 32 symbols; 1 = 64 symbols. (default: %(default)s)",
    )
    parser.add_argument(
        "--nb-sts-segments",
        type=int,
        choices=[0, 1, 2, 3, 4],
        default=1,
        help="Number of STS segments in the frame of the FiRa session. (default: %(default)s)",
    )
    parser.add_argument(
        "--sfd",
        type=int,
        default=2,
        help="Sfd value of the FiRa session. (default: %(default)s)",
    )
    parser.add_argument(
        "--slots-per-rr",
        type=int,
        default=25,
        help="Number of slots in a ranging round of the FiRa session. (default: %(default)s)",
    )
    parser.add_argument(
        "--hopping-mode",
        choices=["disabled", "enabled"],
        default="disabled",
        help="Hopping mode value of the FiRa session. (default: %(default)s)",
    )
    parser.add_argument(
        "--antenna-set-id",
        type=int,
        choices=[0, 1, 2, 3],
        default=0,
        help="Set the antenna set to use for the FiRa session. (default: %(default)s)",
    )
    parser.add_argument(
        "--psdu-data-rate",
        type=int,
        choices=[0, 1, 2, 3, 4],
        default=0,
        help="This value configures the data rate for PHY service Data Unit (PSDU) of the FiRa session:"
        " 0 6.81Mbps / 1 7.80Mbps / 2 27.2Mbps / 3 31.2Mbps / 4 850Kbps, (default: %(default)s)",
    )
    parser.add_argument(
        "--prf-mode",
        type=int,
        choices=[0, 1, 2],
        default=0,
        help="This parameter is used to configure the mean Pulse Repetition Frequency (PRF) of the FiRa session:"
        " 0 BPRF / 1 HPRF / 2 HPRF High Rate, (default: %(default)s)",
    )
    parser.add_argument(
        "--radar-session",
        type=str,
        default="43",
        help="Set the Radar session id to use. (default: %(default)s)",
    )
    parser.add_argument(
        "--radar-frame",
        choices=["sp0", "sp1", "sp3"],
        default="sp0",
        help="Set the RFRAME_CONFIG value of the Radar session. (default: %(default)s)",
    )
    parser.add_argument(
        "--radar-preamble-idx",
        type=int,
        default=9,
        help="Preamble code index of the Radar session. (default: %(default)s)",
    )
    parser.add_argument(
        "--radar-preamble-duration",
        type=int,
        choices=[0, 1, 2, 3, 4, 5, 6, 7],
        default=5,
        help="Preamble duration of the Radar session. 0 = 32 symbols; 1 = 64 symbols; 2 = 128 symbols; 3 = 256 symbols; \
         4 = 512 symbols; 5 = 1024 symbols; 6 = 2048 symbols; 7 = 4096 symbols. (default: %(default)s)",
    )
    parser.add_argument(
        "--radar-burst-period-ms",
        type=int,
        default=200,
        help="Duration between the start of two consecutive Radar bursts in ms. (default: %(default)s)",
    )
    parser.add_argument(
        "--radar-samples-per-sweep",
        type=int,
        default=64,
        help="Number of Radar samples per sweep. Possible values are (1 to 255) (Default = 64). (default: %(default)s)",
    )
    parser.add_argument(
        "--radar-sweep-offset",
        type=int,
        default=-10,
        help="Number of Radar samples offset before First Path. Possible values are (-32768 to 32767) (Default = -10). (default: %(default)s)",
    )
    parser.add_argument(
        "--radar-number-of-bursts",
        type=int,
        default=0,
        help="Configuration parameter to set maximum number of Radar bursts to be executed in a session. The session is stopped when configured Radar bursts are elapsed. 0x00 = Unlimited. (default: %(default)s)",
    )
    parser.add_argument(
        "--radar-antenna-set-id",
        type=int,
        default=3,
        help="Antenna set ID used for Radar. Possible values are (0 to 3). (default: %(default)s)",
    )
    parser.add_argument(
        "--radar-tx-profile-idx",
        type=int,
        default=0,
        help="Radar Tx profile index. 0x00 = TX_PROFILE_HIGH; 0x01 = TX_PROFILE_LOW. (default: %(default)s)",
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

    opts.vendor_id = eval(opts.vendor_id)
    opts.static_sts = eval(opts.static_sts)

    if opts.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
    log = logging.getLogger()

    default_URSK = (
        eng_ursk_prefix + eval(opts.fira_session).to_bytes(4, "big", signed=False).hex()
    )

    controller_default_mac = "00:00"
    controlee_default_mac = "00:01"

    # Fira Mandatory:
    default_device = "controller"  # DEVICE_TYPE
    default_mac = controller_default_mac  # DEVICE_MAC_ADDRESS
    # Other:
    default_report = "tof|azimuth|elevation|fom"  # RESULT_REPORT_CONFIG

    if opts.controlee:
        default_device = "controlee"
        default_mac = controlee_default_mac

    default_config = dict(device=default_device, mac=default_mac, report=default_report)

    if opts.sts in ["dyn-key", "provisioned", "provisioned-key"]:
        default_config["skey"] = default_URSK
    if opts.schedule == "time":
        default_config["n_controlees"] = 1  # NUMBER_OF_CONTROLEES
        default_config["dest_mac"] = (
            f"['{controller_default_mac}']"
            if opts.controlee
            else f"['{controlee_default_mac}']"
        )  # DEST_MAC-ADDRESS

    # Enable en-diag and stats parameters when diag_dump is enabled
    if opts.diag_dump:
        opts.en_diag = True
        opts.stats = True

    args = type("args", (), {})()
    args.__dict__.update(default_config)
    args.__dict__.update({k: v for k, v in vars(opts).items() if v is not None})

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

        # in contention based ranging the dest_mac should not be set
        if args.schedule == "time":
            args.dest_mac = list(
                int.from_bytes(str2bytes(v), "big") for v in eval(args.dest_mac)
            )

        for p in (
            "fira_session",
            "controlees_with_sskey",
            "ssession",
            "cap_range",
            "diag_fields",
            "uwb_diagnostics_flags",
            "round",
            "radar_frame",
            "frame",
            "radar_session",
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

    log_name = f"{datetime.datetime.now().strftime('%y-%m-%d-%Hh%Mm%Ss')}"
    twr_range_name = opts.output + "/range_data_" + log_name + ".json"
    twr_diag_name = opts.output + "/diag_data_" + log_name + ".json"
    radar_log_name = opts.output + "/Accumulator_log_" + log_name + ".json"

    try:
        runner = Runner(
            port=opts.port, qtraces_logfile=opts.qtraces_logfile, logger=log, args=args
        )
        runner.setup_calibration()
        runner.setup_fira_session()
        runner.setup_radar_session()
        runner.start()

        if opts.time == -1:
            input("Press <RETURN> to stop\n")
        else:
            time.sleep(opts.time)

        runner.stop()
        runner.join()
        runner.close_client()

        # Save CIR data
        runner.save_twr_data_to_json_file(twr_range_name, twr_diag_name)
        runner.save_cir_data_to_json_file(radar_log_name)

    except KeyboardInterrupt:
        print("Interrupted")
        if runner:
            runner.stop()
            runner.join()
            runner.close_client()

            # Save CIR data
            runner.save_twr_data_to_json_file(twr_range_name, twr_diag_name)
            runner.save_cir_data_to_json_file(radar_log_name)

        try:
            sys.exit(0)
        except Exception:
            os._exit(0)
    except Exception as e:
        print(f'Error while running the script":\n{e}')
        sys.exit(0)


if __name__ == "__main__":
    main()
