#! python

# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

import argparse
import datetime
import json
import logging
import os
import queue
import sys
import threading
from enum import IntFlag
from time import sleep, time

import matplotlib.animation as animation
import matplotlib.pyplot as plt
import numpy as np
from matplotlib import cm
from matplotlib.lines import Line2D

from uci import UciComError, Status
from uqt_utils.radar import (
    RadarClient,
    RadarDataMessage,
    RadarDataEncoder,
    RadarConfigDecoder,
)
from uqt_utils.load_calibration import load_calibration

sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8")

DEFAULT_SETTINGS_FILE = os.path.dirname(__file__) + "/radar_parameters.json"
DEFAULT_CALIBRATIONS_FILE = (
    os.path.dirname(__file__)
    + "/../../device/load_cal/calib_files/QM35825DK/jolie_quad_radar_TWR_180AoA.json"
)

DEMO_RUN_TIME_S = 60


class FrameLogMode(IntFlag):
    WINDOW = 0
    FIRST_PATH_CENTRIC = 1


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
    def __init__(self, port, radar_config, calibration, qtraces_logfile):
        super().__init__()
        self._radar = RadarClient(port=port, qtraces_logfile=qtraces_logfile)

        self._radar.radar_set_max_measurement_nr_reached_clbk(self.stop)

        if calibration:
            load_calibration(self._radar._client, calibration)

        self.samples_per_sweep = int(str(radar_config["SamplesPerSweep"]))
        self.stopped = False  # Used when session is stopped already
        self.interrupted = False  # Used when session is interrupted from outside
        self.frames = {"Configuration": radar_config, "Frames": []}

        print(self.frames["Configuration"])

    def run(self) -> None:
        try:
            nb_frames = 0
            start = 0
            self.stopped = False
            self.interrupted = False
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
            start = time()
            while not self.stopped and not self.interrupted:
                try:
                    entry = self._radar.radar_get_data()
                except queue.Empty:
                    print("No CIR in queue \n")
                    continue
                if entry:
                    nb_frames += 1
                    self.frames["Frames"].append(entry)
                    q.put(entry)
        except UciComError as e:
            print_protocol_error(e)
            sys.exit(-1)
        finally:
            stop = time()
            try:
                if not self.stopped:
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
                if start:
                    print(
                        f"{nb_frames} frames in\
                    {stop - start}s ({nb_frames/(stop-start)} fps)"
                    )

    def stop(self):
        self.stopped = True

    def interrupt(self):
        self.interrupted = True

    def get_config(self):
        return self.frames["Configuration"]

    def set_config(self, cfg: dict):
        self.cfg = self.frames["Configuration"]


def save_cir_data_to_txt_file(path_to_file: str, data: dict):
    os.makedirs(os.path.dirname(path_to_file), exist_ok=True)
    data_str = "---Configuration---\n"
    for k, v in data["Configuration"].items():
        if k == "Radio Config":
            data_str += "---Radio Configuration---\n"
            for _k, _v in v.items():
                data_str += f"{_k}: {str(_v)}\n"
        else:
            data_str += f"{k}: {str(v)}\n"
    data_str += "\n"
    for i, d in enumerate(data["Frames"]):
        entry_str = f"FRAME {i} DATA:\n"
        entry_str += f"Frame status_code: {d.status_code}\n"
        entry_str += f"Frame radar_data_type: {d.radar_data_type}\n"
        entry_str += f"Frame number_of_sweeps: {d.number_of_sweeps}\n"
        entry_str += f"Frame samples_per_sweep: {d.samples_per_sweep}\n"
        entry_str += f"Frame bits_per_sample: {d.bits_per_sample}\n"
        entry_str += f"Frame sweep_offset: {d.sweep_offset}\n"
        entry_str += f"Frame sweep_data_size: {d.sweep_data_size}\n"
        for i_s, s in enumerate(d.sweep_data):
            entry_str += f"SWEEP {i_s} DATA:\n"
            entry_str += f"\tSweep sequence_number: {s.sequence_number}\n"
            entry_str += f"\tSweep timestamp: {s.timestamp}\n"
            entry_str += f"\tSweep vendor_specific_data_length: {s.vendor_specific_data_length}\n"
            if s.vendor_specific_data_length != 0:
                entry_str += f"\tSweep vendor_specific_data: {s.vendor_specific_data}\n"
            entry_str += "\tSAMPLES DATA:\n"
            entry_str += "\t\tRe = ["
            for sample in s.sample_data:
                entry_str += f"{sample.re}, "
            entry_str += "]\n"
            entry_str += "\t\tIe = ["
            for sample in s.sample_data:
                entry_str += f"{sample.im}, "
            entry_str += "]\n"
            entry_str += "\t\tZ = ["
            for sample in s.sample_data:
                entry_str += f"{sample.z:.2f}, "
            entry_str += "]\n"
        entry_str += "\n"
        data_str += entry_str

    with open(path_to_file, "x") as f:
        f.write(data_str)


def save_cir_data_to_json_file(path_to_file: str, data: dict):
    os.makedirs(os.path.dirname(path_to_file), exist_ok=True)
    with open(path_to_file, "x") as f:
        json_data = json.dumps(data, indent=4, cls=RadarDataEncoder)
        f.write(json_data)


def save_plot_cir_data(
    path_to_file: str,
    data: dict,
):

    plt.title("Sweep samples over time")
    plt.xlabel("Sweep No.")
    plt.ylabel("Amplitude")
    if len(data["Frames"]) == 0:
        plt.clf()
        return
    nb_taps = data["Frames"][0].samples_per_sweep
    # color
    evenly_spaced_interval = np.linspace(0, 1, nb_taps)
    colors = [cm.rainbow(x) for x in evenly_spaced_interval]

    for i in range(nb_taps):
        # take only first sweep
        plt.plot(
            [
                data["Frames"][k].sweep_data[0].sample_data[i].z
                for k in range(len(data["Frames"]))
            ],
            color=colors[i],
        )

    # save figure before showing it, as show() clears the figure.
    plt.savefig(f"{path_to_file}_cirs.png")
    plt.show()

    plt.clf()


class RadarAnimation:
    def __init__(self, size, ylim):
        self.fig, (self.ax, self.CIRax) = plt.subplots(
            1, 2, sharey=True, figsize=(14, 6)
        )

        self.ax.set_title("Sweep samples over time")
        self.ax.set_xlabel("Sweep No.")
        self.ax.set_ylabel("Amplitude")
        self.CIRax.set_title("Sweep sample")
        self.CIRax.set_xlabel("Tap No.")
        self.CIRax.set_ylabel("Amplitude")
        self.ax.set_ylim(ylim)
        self.window = 100
        self.ax.set_xlim(0, self.window)
        self.i = 0
        self.x = []
        self.to_draw = [[] for _ in range(size)]

        # color
        evenly_spaced_interval = np.linspace(0, 1, size)
        colors = [cm.rainbow(x) for x in evenly_spaced_interval]
        self.lines = [Line2D([0], [0], color=colors[i]) for i in range(size)]

        for line in self.lines:
            self.ax.add_line(line)
        (self.CIRline,) = self.CIRax.plot([0.0] * size)
        self.x_points = [x for x in range(size)]
        y = [0.0] * size
        self.col_points = [colors[i] for i in range(size)]
        self.CIRpoint = self.CIRax.scatter(self.x_points, y, color=self.col_points)

    def animate(self, frame_number):
        while q.qsize():
            msg: RadarDataMessage = q.get()
            for sweep in msg.sweep_data:
                sweep_amps = sweep.get_amplitudes()
                self.CIRline.set_ydata(sweep_amps)
                self.CIRpoint = self.CIRax.scatter(
                    self.x_points, sweep_amps, color=self.col_points
                )
                self.x.append(self.i)

                for j, line in enumerate(self.lines):
                    self.to_draw[j].append(sweep_amps[j])
                    line.set_data(self.x, self.to_draw[j])

                self.i = self.i + 1
                if self.i > self.window:
                    for d in self.to_draw:
                        d.pop(0)
                    self.x.pop(0)
                    self.ax.set_xlim(self.i - self.window, self.i)
        return self.lines + [self.CIRline, self.CIRpoint]


def main():
    parser = argparse.ArgumentParser(
        description="""Capture, save and optionally plot CIR data."""
    )
    parser.add_argument(
        "--description",
        action="store_true",
        default=False,
        help="Show short description of the script.",
    )
    parser.add_argument(
        "-p",
        "--port",
        default=os.getenv("UQT_PORT", "ftdi://FT4222"),
        help="Path to port interface (HSSPI). Default is taken from UQT_PORT "
        "environment variable or ftdi://FT4222 if UQT_PORT is not set.",
    )
    parser.add_argument(
        "-o",
        "--output",
        default="./",
        help="Path to saved data.",
    )
    parser.add_argument(
        "-P",
        "--plot",
        choices=["live", "static"],
        default="live",
        help="CIR plotting mode the CIR. Default: %(default)s.",
    )
    parser.add_argument(
        "-s",
        "--settings",
        default=DEFAULT_SETTINGS_FILE,
        help="Radar settings JSON object file.",
    )
    parser.add_argument(
        "-c",
        "--calibrations",
        default=DEFAULT_CALIBRATIONS_FILE,
        help="Radar calibrations JSON object file.",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        default=False,
        help="Use logging.DEBUG level. Default: %(default)s.",
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

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    if args.description:
        print(parser.description)
        sys.exit(0)

    frames = None
    log_name = (
        "Accumulator_log_" f"{datetime.datetime.now().strftime('%y-%m-%d-%Hh%Mm%Ss')}"
    )

    try:
        with open(args.settings, "r") as f:
            demo_config = json.load(f, object_hook=RadarConfigDecoder)

        if args.plot == "live":
            # override the burst_period_ms to force realtime plot without loosing frames.
            demo_config["TimingParams"]["burst_period_ms"] = 50
            print(
                "Live plot enabled: forcing burst period to "
                f"{demo_config['TimingParams']['burst_period_ms']} to guarantee synchronization "
                "between frame acquisition and display."
            )

        print(demo_config)

    except Exception as e:
        print(f"Invalid Radar Settings.\n{e}")
        exit(-1)

    assert demo_config

    try:
        rad = RadarRunner(
            port=args.port,
            radar_config=demo_config,
            calibration=args.calibrations,
            qtraces_logfile=args.qtraces_logfile,
        )

        if args.plot == "live":
            rad_ani = RadarAnimation(rad.samples_per_sweep, [0, 100000])
            ani = animation.FuncAnimation(
                rad_ani.fig,
                rad_ani.animate,
                interval=1,
                save_count=1,
                blit=True,
                cache_frame_data=False,
            )
            rad.start()
            plt.show()
        else:
            rad.start()
            ani = None
            sleeping = 0
            while (
                sleeping < DEMO_RUN_TIME_S and not rad.stopped and not rad.interrupted
            ):
                sleep(1)
                remaining = DEMO_RUN_TIME_S - sleeping
                print(f"Running demo... remaining: {str(remaining)} s", end="\r")
                sleeping = sleeping + 1
        rad.interrupt()
        rad.join()
        frames = rad.frames
    except KeyboardInterrupt:
        print("Interrupted")
        if rad:
            rad.interrupt()
            rad.join()
            frames = rad.frames
            # Save CIR data
            save_cir_data_to_json_file(f"{args.output}/{log_name}.json", frames)
            save_cir_data_to_txt_file(f"{args.output}/{log_name}.txt", frames)
        if ani:
            ani.event_source.stop()
        try:
            sys.exit(0)
        except Exception:
            os._exit(0)
    if frames:
        # Save CIR data
        save_cir_data_to_json_file(f"{args.output}/{log_name}.json", frames)
        save_cir_data_to_txt_file(f"{args.output}/{log_name}.txt", frames)
        # Optionally plot CIR data
        if args.plot == "static":
            save_plot_cir_data(f"{args.output}/{log_name}", frames)
            pass


if __name__ == "__main__":
    main()
