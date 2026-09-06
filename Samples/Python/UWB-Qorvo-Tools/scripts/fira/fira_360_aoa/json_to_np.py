import argparse
import numpy as np
from statistics import mean
import json


class JsonToNumpy:
    def __init__(self, json_file_name: str):
        self.__load_data(json_file_name)
        # Using plain list as ndarray is not efficient for resizing
        self.pdoa_table = []
        self.data_converted = False

    def __load_data(self, json_file_name):
        with open(json_file_name, "r") as json_file:
            self.json_data = json.load(json_file)
        self.data_converted = False

    def load(self, json_file_name):
        self.__load_data(json_file_name)

    def convert_json_data(self):
        for ranging_ntf in self.json_data:
            for report in ranging_ntf["reports"]:
                fields = report.get("fields")
                if fields:
                    for filed in fields:
                        aoas = filed.get("aoa")
                        if aoas:
                            pdoas = [0, 0, 0, 0]
                            for aoa in aoas:
                                # +1 because index 0 reserved for target value
                                pdoas[aoa["idx"] + 1] = aoa["pdoa"]
                            self.pdoa_table.append(pdoas)

    def calculate_mean(self):
        if not self.data_converted:
            self.convert_json_data()
            self.data_converted = True
        return [
            mean(k[1] for k in self.pdoa_table),
            mean(k[2] for k in self.pdoa_table),
            mean(k[3] for k in self.pdoa_table),
        ]

    def get_pdoas_list(self):
        if not self.data_converted:
            self.convert_json_data()
            self.data_converted = True
        return self.pdoa_table

    def get_pdoas_ndarray(self):
        if not self.data_converted:
            self.convert_json_data()
            self.data_converted = True
        return np.array(self.pdoa_table)


epilog = """
Note:
    - Import JsonToNumpy class to do the JSON -> ndarray conversion.
    - Run as script to display mean values from the PDoA measurements.
"""


def main(raw_args=None):
    parser = argparse.ArgumentParser(
        description="Converts JSON log file to ndarray",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=epilog,
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

    args = parser.parse_args(raw_args)
    JsonToNumpy(args.log_file).calculate_mean()


if __name__ == "__main__":
    main()
