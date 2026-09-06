#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

import argparse
import logging
import sys
import json


# Below hack sometimes required when operating on windows git-bash/msys2
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8")


# List of Config keys
# xtal_trim
# ant<K>.ch<C>.ref_frame0.tx_power_index
# ant<K>.ch<C>.ant_delay
# ant<K>.ch<C>.pa_gain_offset
# ant<K>.ch<C>.tx_bypass_delay_offset
# ant<K>.transceiver
# ant<K>.port
# ant<K>.lna
# ant_pair<P>.axis
# ant_pair<P>.ant_paths
# ant_pair<P>.ch<C>.pdoa.offset
# ant_set<X>.rx_ants
# ant_set<X>.tx_ant_paths
# ant_set<X>.nb_rx_ants
# ant_set<X>.rx_ants_are_pairs
# ref_frame0.phy_cfg
# ref_frame0.payload_size

# create a list of strings that are the keys of the default values
ANT_NUM = 7
ANT_PAIR_NUM = 2
ANT_SET_NUM = 3


def generate_reference_list():
    """
    Generate a reference list containing strings like ant<K>.ch<C>.ant_delay
    with K from 0 to 3 and C equal 5 or 9.
    """
    reference_list = ["xtal_trim", "ref_frame0.phy_cfg", "ref_frame0.payload_size"]
    for k in range(ANT_NUM):
        reference_list.append(f"ant{k}.transceiver")
        reference_list.append(f"ant{k}.port")
        reference_list.append(f"ant{k}.lna")
        for c in [5, 9]:
            reference_list.append(f"ant{k}.ch{c}.ref_frame0.tx_power_index")
            reference_list.append(f"ant{k}.ch{c}.ant_delay")
            reference_list.append(f"ant{k}.ch{c}.pa_gain_offset")
            reference_list.append(f"ant{k}.ch{c}.tx_bypass_delay_offset")
    for p in range(ANT_PAIR_NUM):
        reference_list.append(f"ant_pair{p}.axis")
        reference_list.append(f"ant_pair{p}.ant_paths")
        for c in [5, 9]:
            reference_list.append(f"ant_pair{p}.ch{c}.pdoa.offset")
    for x in range(ANT_SET_NUM):
        reference_list.append(f"ant_set{x}.rx_ants")
        reference_list.append(f"ant_set{x}.tx_ant_paths")
        reference_list.append(f"ant_set{x}.nb_rx_ants")
        reference_list.append(f"ant_set{x}.rx_ants_are_pairs")
    return reference_list


def filter_default_values(input_filename, output_filename):
    """
    Read input filename json file, and write on the output filename json file
    only the keys corresponding to the reference list.
    """
    with open(input_filename, "r") as f:
        values = json.load(f)

    reference_list = generate_reference_list()
    filtered_values = {}

    if "values" in values:
        main_entry = "values"
    elif "calibrations" in values:
        main_entry = "calibrations"
    else:
        raise KeyError("No 'values' or 'calibrations' entry found in input file")

    filtered_values["values"] = {
        k: f"{v}" for k, v in values[main_entry].items() if k in reference_list
    }
    for k, v in values[main_entry].items():
        if k in [f"ant_set{x}.tx_ant_path" for x in range(ANT_SET_NUM)]:
            filtered_values["values"][k + "s"] = "0xFF" + v[2:]

    # In case a Tx Power Index is present, add the default values for phy_cfg and payload_size.
    # Usually, ref_frame0 is not explicitly present in the calibration file because there is a
    # default value for it in the FW. In our case, we don't rely on FW's default values (because
    # they can change).
    has_tx_power_index = any(
        f"ant{x}.ch{c}.ref_frame0.tx_power_index" in filtered_values["values"]
        for x in range(8)  # Checking antennas 0 through 7
        for c in [5, 9]  # Checking channels 5 and 9
    )

    if has_tx_power_index:
        if "ref_frame0.phy_cfg" not in filtered_values["values"]:
            filtered_values["values"]["ref_frame0.phy_cfg"] = "0x072144"
        if "ref_frame0.payload_size" not in filtered_values["values"]:
            filtered_values["values"]["ref_frame0.payload_size"] = "127"

    with open(output_filename, "w") as f:
        json.dump(filtered_values, f, indent=4)


def main():
    parser = argparse.ArgumentParser(
        description="Convert a configuration & calibration Json file into a default calibration Json file.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="",
    )
    parser.add_argument(
        "--description",
        action="store_true",
        help="show short description of the script",
        default=False,
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="use logging.DEBUG level. (default: %(default)s)",
        default=False,
    )
    parser.add_argument(
        "-i",
        "--input-file",
        type=str,
        help="Path to JSON file containing the original calibration file.",
    )
    parser.add_argument(
        "-o",
        "--output-file",
        type=str,
        help="Path to the output JSON file containing the filtered and updated configuration keys.",
    )

    args = parser.parse_args()

    if args.description:
        print(parser.description)
        sys.exit(0)

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
    log = logging.getLogger()

    if not args.input_file:
        log.error("Please provide a input JSON file.")
        sys.exit(1)
    if not args.output_file:
        log.error("Please provide an output JSON file.")
        sys.exit(1)

    filter_default_values(
        input_filename=args.input_file, output_filename=args.output_file
    )

    sys.exit(0)


if __name__ == "__main__":
    main()
