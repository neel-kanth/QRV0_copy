#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

from PySide6 import QtCore, QtWidgets, QtGui
from superqt import QLabeledRangeSlider, QLabeledSlider
import numpy as np
import pyqtgraph as pg
import time
import matplotlib.cm as cm
import argparse
import sys
import os
from os import getenv
import json
from datetime import datetime
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
    from sweep_utils import clutter_reduction
    from presence_detection import PresenceDetection
else:
    from .sweep_utils import clutter_reduction
    from .presence_detection import PresenceDetection

DEFAULT_SETTINGS_FILE = os.path.dirname(__file__) + "/radar_parameters.json"
DEFAULT_CALIBRATIONS_FILE = (
    os.path.dirname(__file__)
    + "/../../device/load_cal/calib_files/QM35825DK/jolie_quad_radar_TWR_180AoA.json"
)

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
                print("return CODE: " + str(ret))
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
                    print("Fatal: Timeout in getting sample. \n")
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


class SweepStream(QtCore.QObject):
    processed = QtCore.Signal(np.ndarray)

    def __init__(
        self,
        preprocessing_func,
        parent=None,
        algo=None,
        radar_client: RadarClient = None,
        radar_settings=None,
        samples_range=[0, 64],
        sweep_window_size=50,
    ):
        super().__init__(parent)
        self.frames = []
        self.window_size = sweep_window_size
        self.preprocessing = preprocessing_func
        self._stop_event = False
        self.last_time = time.time()
        self.algo = algo
        self.samples = samples_range
        self.radar = RadarRunner(
            radar_client=radar_client, radar_settings=radar_settings
        )
        self.radar.start()
        self.update_samples_range(samples_range)

    def update_preprocessing(self, preprocessing_func):
        self.preprocessing = preprocessing_func

    def update_data_window_size(self, window_size):
        self.window_size = window_size
        if self.algo is not None:
            self.algo.update_n_sweep(window_size)

    def update_samples_range(self, samples):
        self.samples = samples
        if self.algo is not None:
            self.algo.reset()

    def process_data(self):
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
                    self.frames = self.frames[-self.window_size :]
                    output = self.preprocessing(self.frames)
                    # Using magnitude of the output
                    output = np.abs(output)
                    self.processed.emit(output)
            time.sleep(1 / 1000)  # CRITICAL HERE
            if self._stop_event:
                return

    def stop(self):
        self._stop_event = True
        self.radar.stop()
        self.radar.join()


class PreprocessingMenu(QtWidgets.QWidget):
    def __init__(self, parent=None, main_window=None):
        super().__init__(parent)
        self.main = main_window
        self.layout = QtWidgets.QFormLayout()

        # Create the range slider, horizontal
        self.sweep_sample_slider = QLabeledRangeSlider(QtCore.Qt.Horizontal)
        self.sweep_sample_slider.setRange(0, 64)
        self.sweep_sample_slider.setValue((self.main.samples[0], self.main.samples[1]))
        self.sweep_sample_slider.valueChanged.connect(self.sweep_sample_slider_updated)

        # Add the range slider to the layout with a label
        self.layout.addRow("Select samlpes:", self.sweep_sample_slider)

        # Create a slider for the max data
        self.sweep_range_slider = QLabeledSlider(QtCore.Qt.Horizontal)
        self.sweep_range_slider.setTickPosition(QtWidgets.QSlider.TicksBelow)
        # Display the labe of the slider
        self.sweep_range_slider.setTickInterval(90)
        self.sweep_range_slider.setRange(1, 1000)
        self.sweep_range_slider.setValue(self.main.calib["algo"]["sweeps_history_size"])
        self.sweep_range_slider.valueChanged.connect(self.range_update)
        # Add the range slider to the layout with a label
        self.layout.addRow("Processed sweeps window size:", self.sweep_range_slider)

        if self.main.algo is not None:
            algo_hbox = QtWidgets.QHBoxLayout()
            self.algo_thr_low = QtWidgets.QLineEdit()
            self.algo_thr_low.setText(f"{self.main.algo.thres_lo}")
            self.algo_thr_low.textChanged.connect(self.update_threshold_low)
            self.algo_thr_low.setFixedWidth(100)
            self.algo_thr_high = QtWidgets.QLineEdit()
            self.algo_thr_high.setText(f"{self.main.algo.thres_hi}")
            self.algo_thr_high.textChanged.connect(self.update_threshold_high)
            self.algo_thr_high.setFixedWidth(100)
            algo_hbox.addWidget(QtWidgets.QLabel("threshold low:"))
            algo_hbox.addWidget(self.algo_thr_low)
            algo_hbox.addWidget(QtWidgets.QLabel("threshold high:"))
            algo_hbox.addWidget(self.algo_thr_high)
            self.layout.addRow(algo_hbox)

        self.setLayout(self.layout)

    def start(self):
        self.main.init_threads()

    def stop(self):
        """Stop event handling threads
        force: force stop all threads as we want to create new ones
        """
        if hasattr(self.main, "data_stream"):
            # Stop the data stream and its thread
            self.main.data_stream.stop()
            self.main.data_thread.quit()
            self.main.data_thread.wait()
            self.main.data_stream.deleteLater()

    def range_update(self):
        if hasattr(self.main, "data_stream"):
            self.main.data_stream.update_data_window_size(
                self.sweep_range_slider.value()
            )

    def sweep_sample_slider_updated(self):
        self.main.samples = self.sweep_sample_slider.value()
        rect = self.main.rect_of_detection_zone.rect()
        rect_2 = self.main.rect_of_detection_zone_2.rect()
        width = self.main.samples[1] - self.main.samples[0] - 1
        rect.setX(self.main.samples[0])
        rect.setWidth(width)
        rect_2.setX(self.main.samples[0])
        rect_2.setWidth(width)
        self.main.rect_of_detection_zone.setRect(rect)
        self.main.rect_of_detection_zone_2.setRect(rect_2)
        if hasattr(self.main, "data_stream"):
            self.main.data_stream.update_samples_range(self.main.samples)

    def update_threshold_low(self, text):
        if self.main.algo is not None:
            try:
                self.main.algo.thres_lo = float(text)
                self.main.algo.last_res = False
                rect = self.main.rect_of_detection_zone_2.rect()
                rect.setHeight(self.main.algo.thres_lo)
                self.main.rect_of_detection_zone_2.setRect(rect)
            except ValueError:
                print("thres_lo must be a numerical value")

    def update_threshold_high(self, text):
        if self.main.algo is not None:
            try:
                self.main.algo.thres_hi = float(text)
                self.main.algo.last_res = False
                rect = self.main.rect_of_detection_zone.rect()
                rect.setHeight(self.main.algo.thres_hi)
                self.main.rect_of_detection_zone.setRect(rect)
            except ValueError:
                print("thres_hi must be a numerical value")


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self, radar_settings, radar_client):
        super().__init__()
        self.calib = self.load_calib()
        self.algo = PresenceDetectionQt(**self.calib["algo"])
        self.samples = (self.calib["range_samples"][0], self.calib["range_samples"][1])
        self.radar_settings = radar_settings
        self.radar_client = radar_client
        self.init_ui()

    def init_ui(self):
        self.setWindowTitle("UWB Radar presence detection demo")

        self.detector_group_box = QtWidgets.QGroupBox("Presence Detection indicator")
        self.detector_label = QtWidgets.QLabel(self.detector_group_box)
        self.detector_label.setAlignment(QtCore.Qt.AlignCenter)
        self.detector_layout = QtWidgets.QVBoxLayout(self.detector_group_box)
        self.detector_layout.addWidget(self.detector_label)
        self.detector_group_box.setLayout(self.detector_layout)
        self.detector_group_box.setSizePolicy(
            QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Preferred
        )
        self.detector_label.setFont(QtGui.QFont("Arial", 20, QtGui.QFont.Weight.Bold))
        self.detector_label.setStyleSheet("background-color:yellow;")
        self.detector_label.setText("Waiting for measurements...")

        # Processed Sweeps Stream section
        self.data_group_box = QtWidgets.QGroupBox("Processed sweeps window")
        cmap = cm.viridis
        self.data_image_view = pg.ImageItem(
            np.zeros((300, 64)), lut=(cmap(np.arange(cmap.N)) * 255).astype(np.ubyte)
        )
        self.sweep_plot_widget = pg.PlotWidget()
        self.sweep_plot_widget.addItem(self.data_image_view)
        self.sweep_plot_widget.getAxis("bottom").setLabel("Sweep No.")
        self.sweep_plot_widget.getAxis("left").setLabel("Sample No.")

        self.data_layout = QtWidgets.QVBoxLayout(self.data_group_box)
        self.data_layout.addWidget(self.sweep_plot_widget)

        self.data_group_box.setLayout(self.data_layout)
        size_policy = QtWidgets.QSizePolicy(
            QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Preferred
        )
        size_policy.setVerticalStretch(1)
        self.data_group_box.setSizePolicy(size_policy)

        # Sweeps Stream stat section
        # stat 1: std dev of each sample
        self.sweep_std_dev_widget = pg.PlotWidget()
        self.sweep_std_dev_widget.addLegend(
            brush=pg.mkBrush(0, 0, 0, 70), offset=(0, 1)
        )
        self.sweep_std_dev_curve = self.sweep_std_dev_widget.plot(name="std dev")
        self.sweep_std_dev_widget.showGrid(x=True, y=True)

        thres_hi = self.calib["algo"]["thres_hi"]
        thres_lo = self.calib["algo"]["thres_lo"]
        # rectangle of detection zone
        len_rect = self.samples[1] - self.samples[0] - 1
        if self.algo is not None:
            thres_hi = self.algo.thres_hi
        self.rect_of_detection_zone = QtWidgets.QGraphicsRectItem(
            QtCore.QRectF(self.samples[0], 0, len_rect, thres_hi)
        )
        self.rect_of_detection_zone.setPen(pg.mkPen(255, 255, 0))
        self.sweep_std_dev_widget.addItem(self.rect_of_detection_zone)

        if self.algo is not None:
            thres_lo = self.algo.thres_lo
        # rectangle of detection zone
        self.rect_of_detection_zone_2 = QtWidgets.QGraphicsRectItem(
            QtCore.QRectF(self.samples[0], 0, len_rect, thres_lo)
        )
        self.rect_of_detection_zone_2.setPen(pg.mkPen(0, 0, 255))
        self.sweep_std_dev_widget.addItem(self.rect_of_detection_zone_2)
        # stat 2: ema of each sample
        self.sweep_realtime_widget = pg.PlotWidget()
        self.sweep_realtime_widget.addLegend(
            brush=pg.mkBrush(0, 0, 0, 70), offset=(0, 1)
        )
        self.sweep_realtime_curve = self.sweep_realtime_widget.plot(
            name="real time sweep"
        )
        self.sweep_realtime_widget.showGrid(x=True, y=True)
        # Put 2 plots into a splitter / Groupbox
        self.sweep_stat_splitter = QtWidgets.QGroupBox("Sweep statistics")
        self.sweep_stat_splitter.setMinimumSize(480, 300)
        self.sweep_stat_layout = QtWidgets.QHBoxLayout(self.sweep_stat_splitter)
        self.sweep_stat_layout.addWidget(self.sweep_std_dev_widget)
        self.sweep_stat_layout.addWidget(self.sweep_realtime_widget)

        # Combine sqwwp stat and detector widget
        sweep_stat_and_detector = QtWidgets.QWidget()
        combined_sweep_layout = QtWidgets.QHBoxLayout(sweep_stat_and_detector)
        combined_sweep_layout.addWidget(self.sweep_stat_splitter)
        combined_sweep_layout.addWidget(self.detector_group_box)
        size_policy = QtWidgets.QSizePolicy()

        # Control panel
        self.control_panel = PreprocessingMenu(main_window=self)

        # Set central widget as splitter
        central_widget = QtWidgets.QWidget()
        central_layout = QtWidgets.QVBoxLayout()
        central_layout.addWidget(self.data_group_box)
        central_layout.addWidget(sweep_stat_and_detector)
        central_layout.addWidget(self.control_panel)

        central_widget.setLayout(central_layout)
        self.setCentralWidget(central_widget)

        self.showMaximized()
        # Needed to give some time for calibration
        time.sleep(0.5)
        # Automatically start capture
        self.control_panel.start()

    def init_threads(self):
        # Sweep stream thread
        self.data_thread = QtCore.QThread()

        self.data_stream = SweepStream(
            None,
            algo=self.algo,
            radar_client=self.radar_client,
            radar_settings=self.radar_settings,
            samples_range=self.calib["range_samples"],
            sweep_window_size=self.calib["algo"]["sweeps_history_size"],
        )
        self.control_panel.sweep_range_slider.setRange(0, 1000)
        self.control_panel.sweep_range_slider.setValue(
            self.calib["algo"]["sweeps_history_size"]
        )
        self.data_stream.update_preprocessing(mean_removal)
        self.data_stream.moveToThread(self.data_thread)
        self.data_thread.started.connect(self.data_stream.process_data)
        self.data_thread.start()
        self.data_stream.processed.connect(self.update_sweep_image)

    def update_sweep_image(self, data):
        padding = 5
        # Keep only selected samples
        padding_left = np.max([0, self.samples[0] - padding])
        padding_right = np.min([64, self.samples[1] + padding])
        data_sweeo_image = data[:, self.samples[0] : self.samples[1]]
        data_plot = data[:, padding_left:padding_right]
        # Update statistic plots (before Sweep image, which may rotate the data)
        col_std = np.std(data_plot, axis=0)
        x_data = range(padding_left, padding_right)
        self.sweep_std_dev_curve.setData(x_data, col_std)
        self.sweep_realtime_curve.setData(x_data, data_plot[-1, :])
        # update Sweep image
        self.data_image_view.setImage(data_sweeo_image)
        # Update presence indication
        if self.algo:
            if not self.algo.is_ready():
                self.detector_label.setStyleSheet("background-color:yellow;")
                self.detector_label.setText("Waiting for measurements...")
            else:
                if self.algo.get_result():
                    self.detector_label.setStyleSheet("background-color:green;")
                    self.detector_label.setText("Presence Detected!")
                else:
                    self.detector_label.setStyleSheet("background-color:gray;")
                    self.detector_label.setText("No presence detected.")

    def load_calib(self):
        if os.path.exists("calib"):
            list_calib = sorted(os.listdir("calib"))
            if list_calib:
                path_calib = os.path.join("calib", list_calib[-1])
                with open(path_calib, "r") as file:
                    calib = json.load(file)
                return calib

        calib = dict()
        calib["range_samples"] = [0, 64]
        calib["algo"] = dict()
        calib["algo"]["sweeps_history_size"] = 50
        calib["algo"]["thres_hi"] = 5000
        calib["algo"]["thres_lo"] = 3000
        return calib

    def closeEvent(self, event):
        # Stop the data stream and its thread
        if hasattr(self, "data_stream"):
            self.data_stream.stop()
            self.data_thread.quit()
            self.data_thread.wait()
            self.data_stream.deleteLater()

        # save the algo-parameter
        if self.algo is not None:
            date = datetime.now().strftime("%Y-%m-%d-%H%M%S")
            calib = dict()
            calib["range_samples"] = list(self.samples)
            calib["algo"] = dict()
            calib["algo"][
                "sweeps_history_size"
            ] = self.control_panel.sweep_range_slider.value()
            calib["algo"]["thres_hi"] = self.algo.thres_hi
            calib["algo"]["thres_lo"] = self.algo.thres_lo
            if not os.path.exists("calib"):
                os.makedirs("calib")
                filename = date + "_calib.json"
                with open(os.path.join("calib", filename), "w") as file:
                    json.dump(calib, file)
            elif calib != self.calib:
                filename = date + "_calib.json"
                with open(os.path.join("calib", filename), "w") as file:
                    json.dump(calib, file)

        # Close the main window
        event.accept()


def mean_removal(x):
    x = np.array(x)
    return clutter_reduction(x, algorithm="MEAN")


def main():
    parser = argparse.ArgumentParser(
        description="Demonstration of the UWB Radar presence detection."
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

    # Setting calibrations
    if args.calibrations:
        load_calibration(radar_client._client, args.calibrations)

    # Starting the application
    app = QtWidgets.QApplication([])
    window = MainWindow(  # noqa: F841
        radar_settings=radar_settings, radar_client=radar_client
    )
    app.exec_()


if __name__ == "__main__":
    main()
