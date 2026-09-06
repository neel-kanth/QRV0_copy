#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

import numpy as np
import time
import argparse
import sys
import os
from os import getenv
import json
from uci import UciComError, Status
from uqt_utils.radar import (
    RadarClient,
    RadarDataMessage,
    RadarConfigDecoder,
)
from uqt_utils.load_calibration import load_calibration
import queue
import threading

# Hack to being able to load this file or its parent as a module or as a script
parent_module = sys.modules[".".join(__name__.split(".")[:-1]) or "__main__"]
if __name__ == "__main__" or parent_module.__name__ == "__main__":
    from presence_detection import PresenceDetection
else:
    from .presence_detection import PresenceDetection

DEFAULT_SETTINGS_FILE = os.path.dirname(__file__) + "/radar_parameters.json"
DEFAULT_CALIBRATIONS_FILE = (
    os.path.dirname(__file__)
    + "/../../device/load_cal/calib_files/QM35825DK/jolie_quad_radar_TWR_180AoA.json"
)

ALGORITHM_UPDATE_PERIOD_S = 0.1

q = queue.Queue()


def print_protocol_error(e: UciComError):
    print("\n*******************ERROR*********************")
    print("Command raised a Communication Error:")
    print(f"{e}")
    print("*********************************************")


def print_status_error(s: Status, error: str):
    print("\n*******************ERROR*********************")
    print(error)
    print(f"{str(s)}")
    print("*********************************************")


class RadarRunner(threading.Thread):
    def __init__(self, radar_client, radar_settings=None):
        super().__init__()
        self._radar = radar_client

        self.samples_per_sweep = int(str(radar_settings["SamplesPerSweep"]))
        self.stopped = False
        self.frames = {"Configuration": radar_settings, "Frames": []}

    def run(self) -> None:
        try:
            nb_frames = 0
            self.stopped = False
            ret, _ = self._radar.radar_enable()
            if ret != Status.Ok:
                print_status_error(ret, "Unable to Enable Radar:")
                return
            ret = self._radar.radar_set_config(self.frames["Configuration"])
            if ret != Status.Ok:
                print_status_error(ret, "Unable to Configure Radar:")
                return
            ret = self._radar.radar_start()
            if ret != Status.Ok:
                print_status_error(ret, "Unable to Start Radar:")
                return
            print("Running...")
            while not self.stopped:
                try:
                    entry = self._radar.radar_get_data()
                except Exception:
                    print("Fatal: Timeout in getting Sweep. \n")
                    raise
                if entry:
                    nb_frames += 1
                    self.frames["Frames"].append(entry)
                    q.put(entry)
        except UciComError as e:
            print_protocol_error(e)
            sys.exit(-1)
        finally:
            try:
                print("Stopping...")
                ret = self._radar.radar_stop()
                print(str(ret))
                ret = self._radar.radar_disable()
                print(str(ret))
            except UciComError as e:
                print_protocol_error(e)
                sys.exit(-1)
            finally:
                self.stopped = True
                print("Stopped...")

    def stop(self):
        self.stopped = True

    def get_config(self):
        return self.frames["Configuration"]

    def set_config(self, cfg: dict):
        self.cfg = self.frames["Configuration"]


class PresenceDetectionQt(PresenceDetection):
    def __init__(self, sweeps_history_size=50, thres_hi=500, thres_lo=200):
        super().__init__(
            sweeps_history_size=sweeps_history_size,
            thres_hi=thres_hi,
            thres_lo=thres_lo,
        )

    def update_n_sweep(self, n_sweep):
        chronological_buffer = self.sweep_buffer[self.i :] + self.sweep_buffer[: self.i]
        self.n_sweep = n_sweep

        if len(self.sweep_buffer) >= self.n_sweep:
            # keep only the last self.n_sweep
            self.sweep_buffer = chronological_buffer[-self.n_sweep :]
        else:
            self.sweep_buffer = chronological_buffer
        # as buffer is in chronological order reset i to zero
        # oldest element
        self.i = 0

    def reset(self):
        self.sweep_buffer = []
        self.i = 0  # pointer to next sweep storage location in the buffer
        self.last_result = False


class SweepStream(threading.Thread):

    def __init__(self, user_callback, threshold, sweep_range, samples_range):
        super().__init__()
        self.frames = []
        self.data_range = sweep_range
        self._stop_event = False
        self.last_time = time.time()
        self.algo = PresenceDetectionQt(sweep_range[1], threshold[1], threshold[0])
        self.samples = samples_range
        self.user_callback = user_callback
        self.algo.thres_lo = threshold[0]
        self.algo.thres_hi = threshold[1]
        print(
            f"Algorithm Parameters: Threshold low/high {threshold}, samples range {samples_range}, Sweeps range {sweep_range}"
        )

    def run(self):
        while True:
            while q.qsize():
                msg: RadarDataMessage = q.get()
                for sweep in msg.sweep_data:
                    if self._stop_event:
                        break
                    re = np.array(sweep.get_real_values(), dtype=np.float32)
                    im = np.array(sweep.get_imaginary_values(), dtype=np.float32)
                    sweep = re + 1j * im
                    if self.algo is not None:
                        self.algo.feed_sweep(sweep[self.samples[0] : self.samples[1]])
                    self.frames.append(sweep)
                    self.frames = self.frames[-self.data_range[1] :]
            time.sleep(ALGORITHM_UPDATE_PERIOD_S)

            self.user_callback(
                ready=self.algo.is_ready(), detected=self.algo.get_result()
            )

            if self._stop_event:
                return

    def stop(self):
        self._stop_event = True


# Example of user callback
def update_user_interface(ready, detected):
    sys.stdout.flush()
    # Wipe the line
    print(" " * 60, end="\r")
    if not ready:
        print("Presence Detection status: waiting for measurements", end="\r")
    else:
        if detected:
            print("Presence Detection status: PRESENCE DETECTED", end="\r")
        else:
            print("Presence Detection status: no presence detected", end="\r")


def main():
    parser = argparse.ArgumentParser(
        description="Demonstration of the UWB Radar presence detection without GUI."
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
        help="Path to hsspi interface. Default: FT4222",
        default=getenv("UQT_PORT", "ftdi://FT4222"),
    )
    parser.add_argument(
        "-s",
        "--settings",
        help="Radar parameters JSON object file",
        default=DEFAULT_SETTINGS_FILE,
    )
    parser.add_argument(
        "-c",
        "--calibrations",
        help="Radar calibrations JSON object file",
        default=DEFAULT_CALIBRATIONS_FILE,
    )
    parser.add_argument(
        "-th",
        "--threshold",
        help="Low and high threshold for presence detection algorithm. Default: -th 3000 5000",
        default=[3000, 5000],
        type=int,
        nargs=2,
    )
    parser.add_argument(
        "-sample",
        "--samples_range",
        help="Samples range for presence detection algorithm, limit 0-64. Default: -sample 0 64",
        default=[0, 64],
        type=int,
        nargs=2,
    )
    parser.add_argument(
        "-sweep",
        "--sweep_range",
        help="Sweeps range for presence detection algorithm, limit 0-1000. Default: -sweep 0 50",
        default=[0, 50],
        type=int,
        nargs=2,
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

    radar_client = RadarClient(port=args.port, qtraces_logfile=args.qtraces_logfile)

    try:
        with open(args.settings, "r") as f:
            radar_settings = json.load(f, object_hook=RadarConfigDecoder)
        print(radar_settings)

    except Exception as e:
        print(f"Invalid Radar Settings.\n{e}")
        exit(-1)

    time.sleep(0.2)

    # Setting calibrations
    if args.calibrations:
        load_calibration(radar_client._client, args.calibrations)

    # Starting the application
    radar = RadarRunner(radar_client=radar_client, radar_settings=radar_settings)
    radar.start()

    print("Press Ctrl+C to stop.")

    sweep_stream = SweepStream(
        user_callback=update_user_interface,
        threshold=args.threshold,
        sweep_range=args.sweep_range,
        samples_range=args.samples_range,
    )
    sweep_stream.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("Interrupted, closing app...")
        sweep_stream.stop()
        sweep_stream.join()
        radar.stop()
        radar.join()


if __name__ == "__main__":
    main()
