import argparse
import os
import sys
import time
import numpy as np
from PySide6.QtWidgets import (
    QApplication,
    QMainWindow,
    QVBoxLayout,
    QHBoxLayout,
    QWidget,
    QLineEdit,
    QLabel,
)
from PySide6.QtCore import Qt, QTimer
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure

# Hack to being able to load this file or its parent as a module or as a script.
parent_module = sys.modules[".".join(__name__.split(".")[:-1]) or "__main__"]
if __name__ == "__main__" or parent_module.__name__ == "__main__":
    from aoa_calc import PDOAtoAOAConverter, MovingAveragePDOAFilter, PDOAs
    from json_to_np import JsonToNumpy
    from plots import plotLUT
else:
    from .aoa_calc import PDOAtoAOAConverter, MovingAveragePDOAFilter, PDOAs
    from .json_to_np import JsonToNumpy
    from .plots import plotLUT

DEFAULT_LUT_FILE = os.path.dirname(__file__) + "/torus_lut_ch9.npz"
PDOAS_TO_BE_DISPLAYED = 50


class PDOACanvas(FigureCanvas):
    def __init__(self, parent=None):
        self.fig = Figure()
        self.axes = self.fig.add_subplot(111)
        super().__init__(self.fig)
        self.setParent(parent)

        # Initialize three time series
        self.time_data = []
        self.pdoa_1 = []
        self.pdoa_2 = []
        self.pdoa_3 = []
        self.plot_pdoas()

    def plot_pdoas(self):
        self.axes.clear()
        self.axes.plot(self.time_data, self.pdoa_1, label="PDoA 1")
        self.axes.plot(self.time_data, self.pdoa_2, label="PDoA 2")
        self.axes.plot(self.time_data, self.pdoa_3, label="PDoA 3")
        self.axes.legend()
        self.draw()

    def update_plot_pdoas(self, time, pdoas):
        self.time_data.append(time)
        # Limit the data series to be plotted
        if len(self.time_data) > PDOAS_TO_BE_DISPLAYED:
            self.time_data.pop()
        self.pdoa_1.append(pdoas[0])
        if len(self.pdoa_1) > PDOAS_TO_BE_DISPLAYED:
            self.pdoa_1.pop(0)
        self.pdoa_2.append(pdoas[1])
        if len(self.pdoa_2) > PDOAS_TO_BE_DISPLAYED:
            self.pdoa_2.pop(0)
        self.pdoa_3.append(pdoas[2])
        if len(self.pdoa_3) > PDOAS_TO_BE_DISPLAYED:
            self.pdoa_3.pop(0)
        self.plot_pdoas()


class DOACanvas(FigureCanvas):
    def __init__(self, parent=None):
        self.fig = Figure()
        self.axes = self.fig.add_subplot(111, projection="polar")
        super().__init__(self.fig)
        self.plot_polar()

    def plot_polar(self, doa=None):
        self.axes.clear()
        if doa is not None:
            self.axes.plot(np.radians(doa), 1, "o")  # Plot a single point
        self.draw()

    def update_plot_polar(self, doa):
        self.plot_polar(doa)


class DOAPresentationWindow(QMainWindow):
    def __init__(self, model):
        super().__init__()
        self.setWindowTitle("360 2d AoA Demo")
        self.setGeometry(100, 100, 1000, 620)

        central_widget = QWidget()
        self.setCentralWidget(central_widget)

        main_layout = QHBoxLayout()
        central_widget.setLayout(main_layout)

        left_layout = QVBoxLayout()
        main_layout.addLayout(left_layout)

        right_layout = QVBoxLayout()
        main_layout.addLayout(right_layout)

        pdoa_plot_label = QLabel("Measured PDoAs")
        pdoa_plot_label.setAlignment(Qt.AlignCenter)
        left_layout.addWidget(pdoa_plot_label)
        self.pdoas_canvas = PDOACanvas(self)
        left_layout.addWidget(self.pdoas_canvas)

        # Add text fields to display the last pdoas of the series and their standard deviations
        last_pdoa1_layout = QHBoxLayout()
        self.last_pdoa1 = QLineEdit(self)
        self.last_pdoa1.setReadOnly(True)
        self.std_dev1 = QLineEdit(self)
        self.std_dev1.setReadOnly(True)
        last_pdoa1_layout.addWidget(QLabel("PDoA 1:"))
        last_pdoa1_layout.addWidget(self.last_pdoa1)
        last_pdoa1_layout.addWidget(QLabel("Std. Dev.:"))
        last_pdoa1_layout.addWidget(self.std_dev1)
        left_layout.addLayout(last_pdoa1_layout)

        last_pdoa2_layout = QHBoxLayout()
        self.last_pdoa2 = QLineEdit(self)
        self.last_pdoa2.setReadOnly(True)
        self.std_dev2 = QLineEdit(self)
        self.std_dev2.setReadOnly(True)
        last_pdoa2_layout.addWidget(QLabel("PDoA 2:"))
        last_pdoa2_layout.addWidget(self.last_pdoa2)
        last_pdoa2_layout.addWidget(QLabel("Std. Dev.:"))
        last_pdoa2_layout.addWidget(self.std_dev2)
        left_layout.addLayout(last_pdoa2_layout)

        last_pdoa3_layout = QHBoxLayout()
        self.last_pdoa3 = QLineEdit(self)
        self.last_pdoa3.setReadOnly(True)
        self.std_dev3 = QLineEdit(self)
        self.std_dev3.setReadOnly(True)
        last_pdoa3_layout.addWidget(QLabel("PDoA 3:"))
        last_pdoa3_layout.addWidget(self.last_pdoa3)
        last_pdoa3_layout.addWidget(QLabel("Std. Dev.:"))
        last_pdoa3_layout.addWidget(self.std_dev3)
        left_layout.addLayout(last_pdoa3_layout)

        self.doa_plot_canvas = DOACanvas(self)
        plot_label = QLabel("Calculated Direction of Arrival (DOA)")
        plot_label.setAlignment(Qt.AlignCenter)
        right_layout.addWidget(plot_label)
        right_layout.addWidget(self.doa_plot_canvas)

        self.last_doa_value = QLineEdit(self)
        self.last_doa_value.setReadOnly(True)
        self.last_doa_value.setAlignment(Qt.AlignCenter)
        angle_label = QLabel("Last DoA value:")
        angle_label.setAlignment(Qt.AlignCenter)
        right_layout.addWidget(angle_label)
        right_layout.addWidget(self.last_doa_value)

        left_layout.addStretch()
        right_layout.addStretch()

        self.model = model
        self.model.connect_data_updated_callback(self.update_plot)

        # Timer for automatic updates
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.model.update_data)
        self.timer.start(10)  # Update every 10 ms

    def update_plot(self, timestamp, doa, pdoas):
        self.doa_plot_canvas.update_plot_polar(doa)
        self.last_doa_value.setText(f"{doa:.2f}°")

        self.pdoas_canvas.update_plot_pdoas(timestamp, pdoas)

        # Update text fields with the last pdoas and their standard deviations
        self.last_pdoa1.setText(f"{pdoas[0]:.2f}")
        self.last_pdoa2.setText(f"{pdoas[1]:.2f}")
        self.last_pdoa3.setText(f"{pdoas[2]:.2f}")

        std_dev1 = np.std(self.pdoas_canvas.pdoa_1)
        std_dev2 = np.std(self.pdoas_canvas.pdoa_2)
        std_dev3 = np.std(self.pdoas_canvas.pdoa_3)

        self.std_dev1.setText(f"{std_dev1:.2f}")
        self.std_dev2.setText(f"{std_dev2:.2f}")
        self.std_dev3.setText(f"{std_dev3:.2f}")


class DOAModel:
    def __init__(self, data_file, lut_file):
        filter = MovingAveragePDOAFilter()
        self.aoa = PDOAtoAOAConverter(filter=filter, lut_file_name=lut_file)
        self.row_cnt = 0

        if data_file.rsplit(".", 1)[1] == "npz":
            self.input_data = np.load(data_file)["big_list"]
        elif data_file.rsplit(".", 1)[1] == "json":
            live_data = JsonToNumpy(data_file)
            self.input_data = live_data.get_pdoas_ndarray()
        else:
            raise TypeError("Not supported input file type.")

        plotLUT.plot_lut(self.input_data, "Raw data", no_time=True)

    def connect_data_updated_callback(self, callback):
        self.data_updated_callback = callback

    def update_data(self):
        row = self.input_data[self.row_cnt]
        self.row_cnt = (self.row_cnt + 1) % len(self.input_data)
        pdoas = row[1:]
        filtered_pdoas = self.aoa.pdoa_filter.filter(
            PDOAs(pdoas[0], pdoas[1], pdoas[2])
        )
        doa = self.aoa.get_aoa(filtered_pdoas)
        timestamp = time.time()
        if self.data_updated_callback:
            self.data_updated_callback(timestamp, doa, pdoas)


def main(raw_args=None):
    parser = argparse.ArgumentParser(
        description="Display and calculates 2D AoA value from JSON log file using PDoA LUT.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--description",
        action="store_true",
        help="Show short description of the script.",
        default=False,
    )
    parser.add_argument(
        "-lf",
        "--log_file",
        type=str,
        help="Path to JSON log file.",
    )
    parser.add_argument(
        "--lut",
        type=str,
        default=DEFAULT_LUT_FILE,
        help="Path to PDoA LUT file. (default: %(default)s)",
    )

    args = parser.parse_args(raw_args)
    app = QApplication(sys.argv)
    if args.log_file:
        print(f"Log file used: {args.log_file}")
        print(f"LUT file used: {args.lut}")
        main_window = DOAPresentationWindow(DOAModel(args.log_file, args.lut))
    else:
        raise ValueError("Please provide log file (-lf argument).")
    main_window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
