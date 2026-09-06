#! /usr/bin/env python

from PyQt5 import QtCore, QtWidgets
from superqt import QLabeledRangeSlider
import numpy as np
import pyqtgraph as pg
import matplotlib.cm as cm
import argparse
import sys
import re
import textwrap

CIRS_LIVE_DISPLAY_COUNT = 300
TAPS_DISPLAY_COUNT = 64

class CirStream(QtCore.QObject):
    processed = QtCore.pyqtSignal(np.ndarray)

    def __init__(self):
        super().__init__()
        self.frames=[]
        self.data_range = (0, CIRS_LIVE_DISPLAY_COUNT)
        self.stop_event = False
        self.preprocessing = clutter_reduction
        self.display = magnitude
        self.cirs = np.zeros((CIRS_LIVE_DISPLAY_COUNT, TAPS_DISPLAY_COUNT))

    def update_preprocessing(self, preprocessing_func):
        self.preprocessing = preprocessing_func

    def update_display(self, display_func):
        self.display = display_func

    def update_data_range(self, data_range):
        self.data_range = data_range

    def process_data(self):
        previous_taps_count = 0
        while True:
            # sys.stdin.readlines() returns only on EOF. Since tail -f never sends EOF
            # sys.stdin.readlines() is not usable with tail -f, only with cat.
            # So, we use sys.stdin.readline() in a while True.
            line =sys.stdin.readline()
            if not line:
                break
            match = re.search("^.*(CIR:)(.*)", line)
            if match is not None and match.group(1) == "CIR:":
                cir = np.fromstring(match.group(2), dtype="complex", sep=',')
                # At the first match or taps count changes, catch the taps count an create an empty array for future CIRs.
                taps_count = cir.shape[0]
                if taps_count != previous_taps_count:
                    # The CIRs buffer is empty or should be change.
                    # Fill all the buffer with the first CIR to minimize the effect of preprocessing on 0 values.
                    self.cirs = np.full((CIRS_LIVE_DISPLAY_COUNT, taps_count), cir)
                    print(f"Detected taps count: {taps_count}")
                    previous_taps_count = taps_count
                # Add the new CIR at the end of the cirs array.
                self.cirs = np.vstack([self.cirs, cir])
                # Remove the oldest CIR from the cirs array.
                self.cirs = self.cirs[1:]
                self.refresh_data()
            if self.stop_event:
                break
        print('Stopping CIR stream')

    def refresh_data(self):
        # Select the CIRs specified by the CIR selection slider.
        self.frames= self.cirs[self.data_range[0]:self.data_range[1]]
        # Compute the preprocessing like mean removal declutter.
        output = self.preprocessing(self.frames)
        # Compute the displayed data, like magnitude.
        output = self.display(output)
        # Data are ready, emit a signal.
        self.processed.emit(output)

    def stop(self):
        print("stream stop")
        self.stop_event = True

class CirFileStream(CirStream):
    def __init__(self, file_path):
        super().__init__()
        self.cirs = get_CIRs(file_path)

    def process_data(self):
        self.refresh_data()

class PreprocessingMenu(QtWidgets.QWidget):
    def __init__(self, parent=None, main_window=None, use_stdin=False):
        super().__init__(parent)
        self.main = main_window
        self.use_stdin = use_stdin
        self.range_time = True
        self.layout = QtWidgets.QFormLayout()

        # Taps double cursors slider.
        self.cir_tap_slider = QLabeledRangeSlider(QtCore.Qt.Horizontal)
        self.cir_tap_slider.setRange(0, TAPS_DISPLAY_COUNT)
        self.cir_tap_slider.setValue((0, TAPS_DISPLAY_COUNT))
        self.cir_tap_slider.valueChanged.connect(self.cir_tap_slider_updated)
        slider_label = QtWidgets.QLabel("Select taps:")
        self.layout.addRow(slider_label, self.cir_tap_slider)

        # CIRs double cursors slider.
        self.cir_range_slider = QLabeledRangeSlider(QtCore.Qt.Horizontal)
        self.cir_range_slider.setRange(1, 1000)
        self.cir_range_slider.setValue((1,1000))
        self.cir_range_slider.valueChanged.connect(self.range_update)
        max_data_label = QtWidgets.QLabel("Select CIRs:")
        self.layout.addRow(max_data_label, self.cir_range_slider)

        # Radio button for raw index or range / slow time display.
        self.radioButton_raw_index = QtWidgets.QRadioButton("Display raw data indexes")
        self.radioButton_raw_index.toggled.connect(self.raw_index_selected)
        self.radioButton_range_time = QtWidgets.QRadioButton("Display range and time")
        self.radioButton_range_time.setChecked(True)
        self.radioButton_range_time.toggled.connect(self.range_time_selected)
        index_hbox = QtWidgets.QHBoxLayout()
        index_hbox.addWidget(QtWidgets.QLabel("Axis annotation:"))
        index_hbox.addWidget(self.radioButton_raw_index)
        index_hbox.addWidget(self.radioButton_range_time)
        self.layout.addRow(index_hbox)

        # ComboBox for preprocessing options.
        self.processing_box = QtWidgets.QComboBox()
        for item in self.main.preprocessing_map.keys():
            self.processing_box.addItem(item)
        self.processing_box.setCurrentIndex(1)
        self.processing_box.currentIndexChanged.connect(self.processing_updated)
        processing_hbox = QtWidgets.QHBoxLayout()
        processing_hbox.addWidget(QtWidgets.QLabel("Processing:"))
        processing_hbox.addWidget(self.processing_box)
        self.layout.addRow(processing_hbox)

        # Text box to change rfri.
        self.rfri_box = QtWidgets.QLineEdit()
        self.rfri_box.setText(f'{self.main.rfri}')
        self.rfri_box.setFixedWidth(100)
        self.rfri_box.editingFinished.connect(self.rfri_updated)
        self.rfri_hbox = QtWidgets.QHBoxLayout()
        self.rfri_hbox.addWidget(QtWidgets.QLabel("RFRI(ms):"))
        self.rfri_hbox.addWidget(self.rfri_box)
        self.layout.addRow(self.rfri_hbox)

        if not self.use_stdin:
            # Create the menu for local file
            self.browse_file_button = QtWidgets.QPushButton("Load local file")
            self.browse_file_button.pressed.connect(self.choose_file)
            self.buttons_hbox = QtWidgets.QHBoxLayout()
            self.buttons_hbox.addWidget(self.browse_file_button)
            self.layout.addRow(self.buttons_hbox)

        self.setLayout(self.layout)

    def start(self):
        if not self.use_stdin:
            print(f'Starting, use local file: {self.file}')
        else:
            print(f'Starting, use stdin')
        self.stop()
        self.main.init_threads()

    def stop(self):
        if hasattr(self.main, 'data_stream'):
            # Stop the data stream and its thread
            self.main.data_stream.stop()
            self.main.data_thread.quit()
            self.main.data_thread.wait()
            self.main.data_stream.deleteLater()

    def range_update(self):
        value = self.cir_range_slider.value()
        if hasattr(self.main, 'data_stream'):
            self.main.data_stream.update_data_range(value)
            self.main.data_stream.refresh_data()
        self.main.cirs = value

    def rfri_updated(self):
        if self.rfri_box.text().isnumeric():
            self.main.rfri = int(self.rfri_box.text())
            if hasattr(self.main, 'data_stream'):
                self.main.data_stream.refresh_data()

    def cir_tap_slider_updated(self):
        self.main.taps = self.cir_tap_slider.value()
        if hasattr(self.main, 'data_stream') and self.main.data_refresh_enabled:
            self.main.data_stream.refresh_data()

    def processing_updated(self):
        self.main.data_stream.update_preprocessing(self.main.preprocessing_map[self.processing_box.currentText()])
        if hasattr(self.main, 'data_stream'):
            self.main.data_stream.refresh_data()

    def display_updated(self):
        self.main.data_stream.update_display(magnitude)

    def choose_file(self):
        # Create a file dialog to select a CIRs data file.
        dialog = QtWidgets.QFileDialog()
        # Show the dialog and wait for the user to select a folder
        if dialog.exec_() == QtWidgets.QFileDialog.Accepted:
            self.file = dialog.selectedFiles()[0]
            self.main.setWindowTitle(f"Radar Viewer: {self.file}")
            self.start()

    def raw_index_selected(self, selected):
        if selected:
            self.range_time = False
            if hasattr(self.main, 'data_stream'):
                self.main.data_stream.refresh_data()

    def range_time_selected(self, selected):
        if selected:
            self.range_time = True
            if hasattr(self.main, 'data_stream'):
                self.main.data_stream.refresh_data()

class MainWindow(QtWidgets.QMainWindow):
    def __init__(self, rfri=None, use_stdin=False):
        super().__init__()
        self.rfri = rfri
        self.use_stdin = use_stdin
        self.taps = (0, TAPS_DISPLAY_COUNT)
        self.previous_taps_count = TAPS_DISPLAY_COUNT
        self.cirs = (0, CIRS_LIVE_DISPLAY_COUNT)
        self.data_refresh_enabled = True
        self.preprocessing_map = {
            "None": none,
            "Mean removal": mean_removal,
        }
        self.init_ui()

    def init_ui(self):
        if self.use_stdin:
            self.setWindowTitle("Radar Viewer: CIRs from stdin")
        else:
            self.setWindowTitle("Radar Viewer")

        # Processed CIR Stream section
        self.data_group_box = QtWidgets.QGroupBox("Processed CIRs Stream")
        cmap = cm.jet
        self.data_image_view = pg.ImageItem(np.zeros((CIRS_LIVE_DISPLAY_COUNT, TAPS_DISPLAY_COUNT)), lut=(cmap(np.arange(cmap.N)) * 255).astype(np.ubyte))
        self.cir_plot_widget = pg.PlotWidget()
        self.cir_plot_widget.addItem(self.data_image_view)
        self.data_layout = QtWidgets.QVBoxLayout(self.data_group_box)
        self.data_layout.addWidget(self.cir_plot_widget)

        self.data_group_box.setLayout(self.data_layout)
        size_policy = QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Preferred)
        size_policy.setVerticalStretch(1)
        self.data_group_box.setSizePolicy(size_policy)

        # Control panel
        self.control_panel = PreprocessingMenu(main_window=self, use_stdin=self.use_stdin)
        # Set central widget as splitter
        central_widget = QtWidgets.QWidget()
        central_layout = QtWidgets.QVBoxLayout()
        central_layout.addWidget(self.data_group_box)
        central_layout.addWidget(self.control_panel)
        central_widget.setLayout(central_layout)
        self.setCentralWidget(central_widget)
        self.show()
        if self.use_stdin:
            self.control_panel.start()

    def init_threads(self):
        # CIR stream thread
        self.data_thread = QtCore.QThread()
        if not self.use_stdin:
            self.data_stream = CirFileStream(self.control_panel.file)
            self.control_panel.cir_range_slider.setRange(0, self.data_stream.cirs.shape[0])
            self.control_panel.cir_range_slider.setValue((0, min(1000, self.data_stream.cirs.shape[0])))
            self.control_panel.cir_tap_slider.setRange(0, self.data_stream.cirs.shape[1])
            self.control_panel.cir_tap_slider.setValue((0, self.data_stream.cirs.shape[1]))
            self.max_cirs_count = self.data_stream.cirs.shape[0]
        else:
            self.data_stream = CirStream()
            self.control_panel.cir_range_slider.setRange(0, CIRS_LIVE_DISPLAY_COUNT)
            self.control_panel.cir_range_slider.setValue((0, CIRS_LIVE_DISPLAY_COUNT))
            self.max_cirs_count = CIRS_LIVE_DISPLAY_COUNT
        self.data_stream.update_preprocessing(self.preprocessing_map[self.control_panel.processing_box.currentText()])
        self.data_stream.update_display(magnitude)
        self.data_stream.moveToThread(self.data_thread)
        self.data_thread.started.connect(self.data_stream.process_data)
        self.data_thread.start()
        self.data_stream.processed.connect(self.update_cir_image)

    def update_cir_image(self, data):
        # In live display from stdin, the taps count can change dynamically.
        # So change it if it was modify.
        if self.previous_taps_count != data.shape[1]:
            # Disable call to data_stream.refresh_data() to avoid infinite loop.
            self.data_refresh_enabled = False
            self.control_panel.cir_tap_slider.setRange(0, data.shape[1])
            self.control_panel.cir_tap_slider.setValue((0, data.shape[1]))
            self.previous_taps_count = data.shape[1]
            self.data_refresh_enabled = True
        # Keep only selected taps.
        data = data[:,self.taps[0]:self.taps[1]]
        # Set the taps Y axis labels: range.
        TAP_TICKS_COUNT = 10
        taps_count = data.shape[1]
        taps_ticks_interval = taps_count // (TAP_TICKS_COUNT - 1)
        range_index = get_range(self.taps[0], self.taps[1] + 1)
        ay = self.cir_plot_widget.getAxis('left')
        if (self.control_panel.range_time):
            taps_labels = [ (i*taps_ticks_interval, f"{range_index[i*taps_ticks_interval]:.1f}") for i in range(TAP_TICKS_COUNT)]
            ay.setLabel(text="Range", units="m")
        else:
            taps_labels = [ (i*taps_ticks_interval, f"{self.taps[0] + i*taps_ticks_interval:.0f}") for i in range(TAP_TICKS_COUNT)]
            ay.setLabel(text="Taps index")
        ay.setTicks([taps_labels])

        # Set the cirs X axis labels: slow time
        CIR_TICKS_COUNT = 10
        cirs_count = data.shape[0]
        cirs_ticks_interval = cirs_count // (CIR_TICKS_COUNT - 1)
        # data.shape[0] is updated more slowly than self.cirs, so when when the CIRs slider is decreasing quickly
        # self.cirs[1] - self.cirs[0] is lower than data.shape[0] causing out of bounds IndexError.
        # So using self.cirs[0] + cirs_count instead of self.cirs[1]
        slowtime_index = get_slowtime(self.cirs[0], self.cirs[0] + cirs_count + 1, self.max_cirs_count, self.rfri)
        ax = self.cir_plot_widget.getAxis('bottom')
        if (self.control_panel.range_time):
            cirs_labels = [ (i*cirs_ticks_interval, f"{slowtime_index[i*cirs_ticks_interval]:.1f}") for i in range(CIR_TICKS_COUNT)]
            if self.use_stdin:
                ax.setLabel(text="Time, 0 is now", units="s")
            else:
                ax.setLabel(text="Time, 0 is the most recent", units="s")
        else:
            cirs_labels = [ (i*cirs_ticks_interval, f"{self.cirs[0] + i*cirs_ticks_interval:.0f}") for i in range(CIR_TICKS_COUNT)]
            ax.setLabel(text="CIRs index")
        ax.setTicks([cirs_labels])
        # Display the data
        self.data_image_view.setImage(data)

    def closeEvent(self, event):
        # Stop the data stream and its thread.
        if hasattr(self, 'data_stream'):
            self.data_stream.stop()
            self.data_thread.quit()
            self.data_thread.wait(100)
            self.data_stream.deleteLater()
        event.accept()

# Preprocessing options.
def none(x):
    return x

def mean_removal(x):
    x = np.array(x)
    return clutter_reduction(x)
# Display option.
def magnitude(x):
    return np.abs(x)

# Declutter.
def clutter_estimation_mean(cirs_z):
    dc = np.mean(cirs_z, axis=0, keepdims=True)
    return dc

def clutter_reduction(cirs_z):
    # Remove time constant part of the signal (per column)
    clutter_free_cirs_z = cirs_z - clutter_estimation_mean(cirs_z)
    return clutter_free_cirs_z

# Get CIRs from file.
def get_CIRs(file_path):
    data = []
    with open(file_path, "r") as f:
        for line in f:
            match = re.search("^.*(CIR:)(.*)", line)
            if match is not None and match.group(1) == "CIR:":
                cir = np.fromstring(match.group(2), dtype="complex", sep=',')
                data.append(cir)
    try:
        cirs_z = np.array(data)
    except:
        msg = QtWidgets.QMessageBox()
        msg.setIcon(QtWidgets.QMessageBox.Icon.Critical)
        msg.setText("The CIRs taps count is not the same on all CIRs, unable to load this data file!")
        msg.setWindowTitle("CIRs data error")
        msg.setStandardButtons(QtWidgets.QMessageBox.StandardButton.Close)
        msg.exec()
        return np.zeros((CIRS_LIVE_DISPLAY_COUNT, TAPS_DISPLAY_COUNT))
    print(f"{cirs_z.shape[0]} CIRs of {cirs_z.shape[1]} taps loaded from {file_path}.")
    return cirs_z

# Convert taps indexes to distance.
def get_range(start, stop, sampling_rate=1e+9):
    speed_of_light = 3e+8
    range_index = np.linspace(start, stop, num=int(stop - start))
    range_index = range_index * (1/sampling_rate)
    range_index = range_index * speed_of_light / 2
    return range_index

# Convert CIRs indexes to time.
def get_slowtime(start, stop, max, rfri):
    count = stop - start
    stop = stop - max
    start = start - max
    time_index = np.linspace(start, stop, count)
    time_index = time_index * rfri/1000
    return time_index

def main():
    description="GUI for viewing of CIRs radar data stream."
    epilog = textwrap.dedent('''\
    This tool displays radar data produced by the cherry-radar-app.
    The input data may be:
        - A file containing CIRs generated by the -f option of cherry-radar-app.
        - The raw output of cherry-radar-app.
    It displays a range map of the collected CIRs with time in seconds on abscissa
    and range in meters on ordinate.
    It has two modes of operation:
        - Offline visualization by loading a data file with the "Load local file" button.
        - Live visualization when a log file or a data file is piped in the tool.

    Example of live visualization with a QM35 embedded system kit
    =============================================================
    Open a serial terminal to the embedded system with data logging:
        $ minicom -b 1000000 -D /dev/ttyACM0 -C radar.log
    minicom will log all the outputed data in the radar.log file.

    In that terminal, run a radar session that collects for example 300 CIRs of 80 taps at a RFRI of 50 ms:
        uart:~$ cherry-radar-app -n 300 -b 50 -S 80

    In another terminal, run this tool by piping the live log file on its standard input:
        tail -f ~/radar.log | ./display_radar_data_live.py

    The tool will scroll the range map from right (most recent time) to left (oldest time).

    Example of live visualization with a QM35 Linux RPi kit
    =======================================================
    Open a terminal on the host system:
        $ ssh root@rpi cherry-radar-app -n 0 -S 150 -b 50 2>&1 | ./display_radar_data_live.py
    ''')
    parser = argparse.ArgumentParser(description=description, epilog=epilog, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--rfri', '-r', type=int, default=100, help='Desired CIR frame interval in ms.')
    args = parser.parse_args()

    if not sys.stdin.isatty():
        print("Pipe detected, read CIRs data from stdin")
        use_stdin = True
    else:
        use_stdin = False

    app = QtWidgets.QApplication([])
    # MainWindow() must be assigned to a variable (window) else the garbage collector
    # frees it randomly and 'QThread: Destroyed while thread is still running' error occurs.
    window = MainWindow(rfri=args.rfri, use_stdin=use_stdin)
    app.exec_()

if __name__ == "__main__":
    main()
