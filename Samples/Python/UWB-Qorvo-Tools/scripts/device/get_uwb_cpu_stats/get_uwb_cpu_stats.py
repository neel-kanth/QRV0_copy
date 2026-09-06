#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

import argparse
import logging
import os

import uci
from uqt_utils.utils import uqt_errno
import matplotlib.pyplot as plt
import numpy as np

# Below hack sometimes required when operating on windows git-bash/msys2
import sys

sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8")

epilog = """
Example of use:
    To get cpu stats:
      get_uwb_cpu_stats -p /dev/ttyUSB0
    To reset cpu stats after getting it:
      get_uwb_cpu_stats -p /dev/ttyUSB0 -r
    To get cpu stats and plot the data:
      get_uwb_cpu_stats -p /dev/ttyUSB0 -g
"""


def main():
    default_port = os.getenv("UQT_PORT", "/dev/ttyUSB0")
    parser = argparse.ArgumentParser(
        description="Get UWB CPU Stats.",
        formatter_class=argparse.RawTextHelpFormatter,
        epilog=epilog,
    )
    parser.add_argument(
        "--description",
        action="store_true",
        help="show short description of the script",
    )
    parser.add_argument(
        "-p",
        "--port",
        type=str,
        default=default_port,
        help="communication port to use.",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        default=False,
        help="use logging.DEBUG level (default: %(default)s)",
    )
    parser.add_argument(
        "-r",
        "--reset",
        action="store_true",
        default=False,
        help="reset cpu stats after getting it (default: %(default)s)",
    )
    parser.add_argument(
        "-g",
        "--graph",
        action="store_true",
        default=False,
        help="plot the cpu histograms (default: %(default)s)",
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

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    while True:
        try:
            client = None
            client = uci.Client(port=args.port, qtraces_logfile=args.qtraces_logfile)
            rts, timestamp_since_last_reset, type_stats, probe_count, probes = (
                client.get_uwb_cpu_stats()
            )
            if rts != uci.Status.Ok:
                print(
                    f"Get UWB CPU Stats failed: {rts.name} ({rts})\nIs the QPROBE module activated in the firmware ?"
                )
                break
            else:
                print(f"status: {rts.name} ({rts})")
                print(f"timestamp_since_last_reset: {timestamp_since_last_reset}")
                print(f"type_stats: {type_stats}")
                print(f"probe_count: {probe_count}")
                for probe in probes:
                    (
                        probe_name_length,
                        probe_name,
                        cumulative_time_us,
                        min_duration_us,
                        max_duration_us,
                        nb_histograms,
                        histo_range_seperators,
                        histo_counter,
                        error,
                    ) = probe
                    print("--------------------")
                    print(f" probe_name_length: {probe_name_length}")
                    print(f" probe_name: {probe_name}")
                    print(f" cumulative_time_us: {cumulative_time_us}")
                    print(f" min_duration_us: {min_duration_us}")
                    print(f" max_duration_us: {max_duration_us}")
                    print(f" nb_histograms: {nb_histograms}")
                    print(f" histo_range_seperators: {histo_range_seperators}")
                    print(f" histo_counter: {histo_counter}")
                    print(f" error: {error}")

                if args.graph:
                    print("\nPlotting cpu stats...")
                    data_sets = []
                    for probe in probes:
                        (
                            probe_name_length,
                            probe_name,
                            cumulative_time_us,
                            min_duration_us,
                            max_duration_us,
                            nb_histograms,
                            histo_range_seperators,
                            histo_counter,
                            error,
                        ) = probe
                        data_sets.append(
                            {
                                "name": probe_name,
                                "data": histo_counter,
                                "range": [0] + histo_range_seperators,
                                "error": error,
                            }
                        )

                    num_cols = 2
                    num_rows = (len(data_sets) + 1) // num_cols
                    fig, axs = plt.subplots(
                        num_rows, num_cols, figsize=(12, num_rows * 4)
                    )

                    # Flatten the axs array for easier indexing
                    axs = axs.flatten()
                    # Iterate over each probes
                    for i, data_set in enumerate(data_sets, start=1):
                        name = data_set["name"]
                        data = data_set["data"]
                        ranges = data_set["range"]
                        error = data_set["error"]
                        # Create labels based on ranges
                        labels = [
                            f"{start} - {end}"
                            for start, end in zip(ranges[:-1], ranges[1:])
                        ] + [f"> {ranges[-1]}"]

                        # Plot bar chart with bottom parameter to ensure bars are not stacked
                        bars = axs[i - 1].bar(
                            labels,
                            data,
                            label="Data",
                            color="blue",
                            bottom=np.zeros(len(data)),
                        )

                        # axs[i - 1].bar(labels, data, label="Data", color='blue', bottom=np.zeros(len(data)))
                        axs[i - 1].set_xticklabels(
                            labels, ha="center", fontsize="small"
                        )
                        axs[i - 1].set_title(
                            f"{name} - error {error}", fontsize="small", weight="bold"
                        )
                        axs[i - 1].set_ylabel("Count")
                        axs[i - 1].set_xlabel("Range in us")

                        # Add text labels on each bar for the number of elements
                        for bar, label in zip(bars, data):
                            if label != 0:
                                axs[i - 1].text(
                                    bar.get_x() + bar.get_width() / 2,
                                    bar.get_height() / 2,
                                    f"{label}",
                                    ha="center",
                                    va="bottom",
                                )

                    fig.suptitle("UWB CPU Stats", fontsize="large", weight="bold")
                    plt.subplots_adjust(
                        hspace=0.5
                    )  # Increase vertical gap between subplots
                    plt.show()

                if args.reset:
                    print("\nResetting cpu stats...")
                    rts = client.reset_uwb_cpu_stats()
                    print(f"status: {rts.name} ({rts})")
                break

        except uci.UciComError as e:
            rts = e.n
            print(f"{e}")
            break

    if client:
        client.close()
    sys.exit(uqt_errno(rts))


if __name__ == "__main__":
    main()
