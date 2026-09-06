# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

"""
This file provide basic functions (loading, displaying) for Sweep data analyse
"""

import numpy as np
import logging
from scipy.ndimage import uniform_filter1d


def clutter_estimation_mean(sweep):
    """
    clutter_estimation_mean estimates clutter of a Sweep by computing the mean along the slowtime axis
    Parameters:
        sweep (numpy.ndarray): 2d complex numpy array containing the Sweep (slowtime, fasttime)
    Returns:
        dc (numpy.ndarray): estimated clutter
    """
    dc = np.mean(sweep, axis=0, keepdims=True)
    return dc


def clutter_estimation_svd(sweep, n_comp=1):
    """
    clutter_estimation_svd estimates clutter of a Sweep using the svd decomposition
    Parameters:
        sweep (numpy.ndarray): 2d complex numpy array containing the Sweep (slowtime, fasttime)
        n_comp (int): number of svd's components to estimate clutter (default=1)
    Returns:
        dc (numpy.ndarray): estimated clutter
    """
    u, s, vh = np.linalg.svd(sweep, full_matrices=False)
    s_ = np.diag(s[:n_comp])
    u_ = u[:, :n_comp]
    vh_ = vh[:n_comp, :]
    dc = u_ @ s_ @ vh_
    return dc


def clutter_estimation_ema(sweep, alpha=0.9):
    """
    Estimates the Exponential Moving Average of a Sweep, along slow time
    Parameters:
        sweep (numpy.ndarray): 2d complex numpy array containing the Sweep (slowtime, fasttime)
        alpha (float): learning rate of the filter (default=0.9)
    Returns:
        dc (numpy.ndarray) : estimated clutter
    """
    dc = np.zeros(sweep.shape, dtype=sweep.dtype)
    dc[0, :] = sweep[0, :]  # initialize the mean = first seen value
    for k in range(1, sweep.shape[0]):
        dc[k, :] = alpha * dc[k - 1, :] + (1 - alpha) * sweep[k, :]
    return dc


def clutter_estimation_ma(sweep_z, n):
    """
    Estimates the Moving Average of a Sweep, along slow time
    Parameters:
        sweep (numpy.ndarray): 2d complex numpy array containing the Sweep (slowtime, fasttime)
        n (int): size of the averaging window
    Returns:
        dc (numpy.ndarray) : estimated clutter
    """
    dcs = []
    for i in range(sweep_z.shape[1]):
        dc = uniform_filter1d(sweep_z[:, i], size=n, mode="reflect")
        dcs.append(dc)
    dcs = np.array(dcs).T
    return dcs


def clutter_reduction(
    sweep_z, algorithm, n_ma=100, n_svd=1, alpha_ema=0.99, row_wise=False
):
    """Remove the constant componenant from the Sweep to reveal the weak signals
    Parameters:
        sweep (numpy.ndarray): 2d complex numpy array containing the Sweep (slowtime, fasttime)
        algorithm: string, how to detemine the constant component to remove from Sweep,
                   choose from "SVD", "MEAN", "MA" (moving average), "EMA" (exponential moving
                   average), "MIN" (minimum of each column), "FIRST" (first sweep)
        row_wise: row wise mean removal and normalization
    Returns:
        dc (numpy.ndarray) : estimated clutter
    """
    # Remove time constant part of the signal (per column)
    if algorithm == "SVD":
        clutter_free_signal = sweep_z - clutter_estimation_svd(sweep_z, n_svd)
    elif algorithm == "MEAN":
        clutter_free_signal = sweep_z - clutter_estimation_mean(sweep_z)
    elif algorithm == "MA":
        clutter_free_signal = sweep_z - clutter_estimation_ma(sweep_z, n_ma)
    elif algorithm == "EMA":
        clutter_free_signal = sweep_z - clutter_estimation_ema(sweep_z, alpha_ema)
    elif algorithm == "MIN":
        clutter_free_signal = sweep_z - np.min(sweep_z, axis=0, keepdims=True)
    elif algorithm == "FIRST":  # take first sample as constant componenant
        clutter_free_signal = sweep_z - sweep_z[0, :]
    elif algorithm is None:
        clutter_free_signal = sweep_z
    else:
        logging.warning("No algorithm selected for clutter redution")

    if row_wise:
        # Remove distance constant part (per row)
        col_removal = clutter_free_signal
        row_removal = col_removal - np.mean(col_removal, axis=1, keepdims=True)
        # Previous step should not introudce more pixelwise variance compared to original coloumn
        prev_row = np.vstack([col_removal[0, :], col_removal[:-1, :]])
        diff_old = col_removal - prev_row
        prev_row_2 = np.vstack([row_removal[0, :], row_removal[:-1, :]])
        diff_new = row_removal - prev_row_2
        idx = abs(diff_new) > abs(diff_old)
        idx2 = np.sum(idx, axis=0) / idx.shape[0] > 0.5
        row_removal[:, idx2] = col_removal[:, idx2]
        # Row normalization
        normalized = np.abs(
            row_removal / (np.std(row_removal, axis=1, keepdims=True) + 1e-9)
        )
        return normalized
    else:
        return clutter_free_signal


def declutter_and_decomplexify(
    data, declutter="EMA", decomplex="abs", row_wise=False, **args_clutter_reduction
):
    if decomplex == "amplitude":
        data = np.abs(data)

    # Remove mean
    if declutter or row_wise:
        data = clutter_reduction(
            data, algorithm=declutter, row_wise=row_wise, **args_clutter_reduction
        )

    if decomplex == "concat":
        data = np.hstack([data.real, data.imag])
    elif (
        decomplex == "abs"
    ):  # Reconstruct the amplitude AFTER zero mean, better performance on ML training compared to zero mean of raw amplitude
        data = np.abs(data)
    elif decomplex == "phase":
        data = np.angle(data)
    elif decomplex != "amplitude" and decomplex is not None:
        print(f"Unrecognized argument: decomplex={decomplex}")

    return data


def range_doppler_fft(sweep):
    """
    range_doppler_fft computes the range-doppler map of Sweep using the FFT along slowtime axis
    Parameters:
        sweep (numpy.ndarray): 2d complex numpy array containing the Sweep (slowtime, fasttime) after clutter removal

    Returns: rd_map (numpy.ndarray) : range doppler map (frequency, fasttime)
    """
    frame_length = sweep.shape[1]
    rd_map = np.zeros(sweep.shape, dtype=sweep.dtype)
    for i in range(frame_length):
        # change n to add more resolution
        rd_map[:, i] = np.fft.fft(sweep[:, i], n=sweep.shape[0])
    return rd_map


def time_range_doppler_fft(sweep, fft_size=32, hop_size=16):
    """
    range_doppler_fft computes the time-range-doppler map of Sweep ie applies a STFT on the slowtime axis
    Parameters:
        sweep (numpy.ndarray): 2d complex numpy array containing the Sweep (slowtime, fasttime) after clutter removal
        fft_size (int): window size used for the STFT (default=32)
        hop_size (int): hop size used for the STFT (default=16)

    Returns: trd_map (numpy.ndarray) : time range doppler map
    """
    frame_length = sweep.shape[1]
    nb_frames = sweep.shape[0]
    total_segments = np.floor(nb_frames / hop_size).astype(int) - 1
    window = np.hanning(fft_size)

    trd_map = np.empty((total_segments, fft_size, frame_length), dtype=np.complex128)

    for i in range(frame_length):
        signal = sweep[:, i]
        for k in range(total_segments):
            cursor = k * hop_size
            signal_segment = signal[
                cursor : np.min((cursor + fft_size, signal.shape[0]))
            ]
            signal_segment_windowed = signal_segment * window
            spectrum = np.fft.fft(signal_segment_windowed)
            trd_map[k, :, i] = spectrum

    return trd_map


def get_slowtime(nb_frames, rfri):
    """
    get_slowtime gets the slow time indices in seconds according to the RFRI
    Parameters:
        nb_frames (int): number of frames ie slowtime length
        rfri (float): radar frame repetition interval in seconds (default=(1/180))
    Returns:
        time_index (numpy.ndarray): slowtime in seconds
    """
    time_index = np.linspace(0, rfri * nb_frames, num=nb_frames)
    return time_index


def get_range(frame_length, sampling_rate=1e9):  # A vérifier sampling rate
    """
    get_range gets the estimated range corresponding to the fasttime length
    Parameters:
        frame_length (int): length of one frame ie fasttime length
        sampling_rate (float) : sampling rate of the sweep signal (default=1GHz)
    Returns:
        range_index (numpy.ndarray) : range index in meters
    """
    speed_of_light = 3e8
    time_index = np.linspace(0, (1 / sampling_rate) * frame_length, num=frame_length)
    range_index = time_index * speed_of_light / 2
    return range_index


def get_frequencies(win_len, d):
    """
    get_frequencies gets the Discrete Fourier Transform sample frequencies.
    Parameters:
        win_len (int): window length
        d (float) : Sample spacing ie inverse of the sampling rate. (default=(1/180))
    Returns:
        freq_index (numpy.ndarray) : sample frequencies in Hz
    """
    freq_index = np.fft.fftfreq(win_len, d=d)
    return freq_index
