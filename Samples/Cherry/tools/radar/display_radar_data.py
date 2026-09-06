#! /usr/bin/env python

import argparse
import re
import matplotlib.pyplot as plt
import numpy as np
from matplotlib import cm


def show_taps(cirs_z, title):
    """
    show_taps display all the taps over time. One line is the same tap over all the CIRs.
    Parameters:
        cirs_z (numpy.ndarray): 2d complex numpy array containing the CIRs (slowtime, fasttime)
        title (str): title of the plot
    Returns:
        none
    """
    plt.title(f"Taps over time for {title}")
    plt.xlabel("CIRs index")
    plt.ylabel("Amplitude")
    cirs = np.abs(cirs_z)
    nb_taps = cirs_z.shape[1]
    evenly_spaced_interval = np.linspace(0, 1, nb_taps)
    colors = [cm.rainbow(x) for x in evenly_spaced_interval]
    for i in range(nb_taps):
        plt.plot(cirs[:,i], color=colors[i])
    plt.show()

def show_CIRs(cirs_z, index, title):
    """
    show_CIR display one CIR.
    Parameters:
        cirs_z (numpy.ndarray): 2d complex numpy array containing the CIRs (slowtime, fasttime)
        index (int): index od CIR to display in cirs_z
        title (str): title of the plot
    Returns:
        none
    """
    plt.title(f"CIR {index} of {title}")
    plt.xlabel("Tap index")
    plt.ylabel("Amplitude")
    cir = np.abs(cirs_z[index])
    plt.plot(cir)
    plt.show()

def get_CIRs(file_path):
    """
    get_CIRs loads the CIRs from a log file of the embedded version of cherry-radar
    or from a data file created with the option -f of the linux version of cherry-radar.
    The log file may contain anything. The start of the CIR data must be prefixed by 'CIR:'.
    After 'CIR:', there is a comma separated list of complex numbers.
    For example:
    [00:04:35.392,761] <inf> qlog: CIR:-1+12j,69+19j,3231+540j,3231+1665j,3231-221j,...
    or
    CIR:-38+7j,139+14j,2714+579j,3231+3142j,3231+3231j,...
    Parameters:
        file_path (str): file path of the log file
    Returns:
        cirs_z (numpy.ndarray) : 2d complex numpy array containing the CIRs (slowtime, fasttime)
    """
    data = []
    with open(file_path, "r") as f:
        for line in f:
            match = re.search("^.*(CIR:)(.*)", line)
            if match is not None and match.group(1) == "CIR:":
                cir = np.fromstring(match.group(2), dtype="complex", sep=',')
                data.append(cir)
    cirs_z = np.array(data)
    print(f"{cirs_z.shape[0]} CIRs of {cirs_z.shape[1]} taps loaded from {file_path}.")
    return cirs_z

def get_range(frame_length, sampling_rate=1e+9):
    """
    get_range gets the estimated range corresponding to the fasttime length
    Parameters:
        frame_length (int): length of one frame ie fasttime length
        sampling_rate (float): sampling rate of the cir signal (default=1GHz)
    Returns:
        range_index (numpy.ndarray): range index in meters
    """
    speed_of_light = 3e+8
    time_index = np.linspace(0, (1/sampling_rate) *
                             frame_length, num=frame_length)
    range_index = time_index * speed_of_light / 2
    return range_index

def get_slowtime(nb_frames, rfri):
    """
    get_slowtime gets the slow time indices in seconds according to the RFRI
    Parameters:
        nb_frames (int): number of frames ie slowtime length
        rfri (float): radar frame repetition interval in milliseconds
    Returns:
        time_index (numpy.ndarray): slowtime in seconds
    """
    time_index = np.linspace(0, rfri/1000 * nb_frames, num=nb_frames)
    return time_index

def plot_abs_cirs_2d(cirs_z, rfri, title):
    """
    plot_abs_cir_2d plots a 2d surface of the absolute value of the CIRs
    Parameters:
        cirs_z (numpy.ndarray): 2d complex numpy array containing the CIRs (slowtime, fasttime)
        title (str): title of the plot

    Returns: fig, ax (matplotlib.figure, matplotlib.axes.Axes) to show the plot, write
                plt.show() after calling the function
    """
    nb_frames = cirs_z.shape[0]
    frame_length = cirs_z.shape[1]
    range_index = get_range(frame_length)
    slowtime_index = get_slowtime(nb_frames, rfri)
    X, Y = np.meshgrid(range_index, slowtime_index)
    Z = np.abs(cirs_z)
    fig, ax = plt.subplots()
    ax.pcolormesh(X, Y, Z, cmap='jet', edgecolors=None)
    ax.set_xlabel('range [m]',  fontsize=16)
    ax.set_ylabel('slow time [s]',  fontsize=16)
    ax.set_title(f'Range map for {title}', fontsize=18)
    return fig, ax

def clutter_estimation_mean(cirs_z):
    """
    clutter_estimation_mean estimates clutter of a CIR by computing the mean along the slowtime axis
    Parameters:
        cirs_z (numpy.ndarray): 2d complex numpy array containing the CIRs (slowtime, fasttime)
    Returns:
        dc (numpy.ndarray): estimated clutter
    """
    dc = np.mean(cirs_z, axis=0, keepdims=True)
    return dc

def clutter_reduction(cirs_z):
    """
    Remove the constant component from the CIR to reveal the weak signals
    Parameters:
        cirs_z (numpy.ndarray): 2d complex numpy array containing the CIRs (slowtime, fasttime)
    Returns:
        clutter_free_cirs_z (numpy.ndarray) : uncluttered CIRs
    """
    # Remove time constant part of the signal (per column)
    clutter_free_cirs_z = cirs_z - clutter_estimation_mean(cirs_z)
    return clutter_free_cirs_z

def range_map(cirs_z, rfri, title):
    print(f"Range map visualization at rfri {rfri} ms")
    clutter_free_cirs_z = clutter_reduction(cirs_z)
    plot_abs_cirs_2d(clutter_free_cirs_z, rfri, title)
    plt.show()

def main():
    parser = argparse.ArgumentParser(description='Display radar data.')
    parser.add_argument('--range-map', '-r', action='store_true', help='display a range map')
    parser.add_argument('--cir', '-c', action='store_true', help='display one CIR')
    parser.add_argument('--taps', '-t', action='store_true', help='display all taps')
    parser.add_argument('--rfri', '-i', type=int, default=100,
                        help='radar frame repetition interval in milliseconds for range map display')
    parser.add_argument('--index', '-n', type=int, default=0,
                        help='index of the CIR to display for CIR display')
    parser.add_argument('filename', help='radar data file to plot')
    args = parser.parse_args()

    if not (args.range_map ^ args.cir ^ args.taps):
        print("One of the visualization options (--range-map, --cir or --taps) must be provided.")
        return 1

    cirs_z = get_CIRs(args.filename)

    if args.range_map:
        range_map(cirs_z, args.rfri, args.filename)
    elif args.cir:
        show_CIRs(cirs_z, args.index, args.filename)
    elif args.taps:
        show_taps(cirs_z, args.filename)
    return 0
if __name__ == "__main__":
    exit(main())
