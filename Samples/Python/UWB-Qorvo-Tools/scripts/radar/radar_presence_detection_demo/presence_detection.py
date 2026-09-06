# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

import numpy as np


class PresenceDetection:
    """
    Implements the baseline presence detection algorithm.
    The algorithm simply calculates the std-deviation along each sample (colunm)
    of the sweep image and apply a threshold on it.
    """

    def __init__(self, sweeps_history_size=50, thres_hi=500, thres_lo=200):
        """
        Args:
            sweeps_history_size: how many sweeps to keep in history, it's recommended to keep
                                 sweeps in the last 5 to 10 seconds, with burst period=100ms
            thres_hi: threshold on sample std-dev for passing from no detection to detection
            thres_lo: threshold on sample std-dev for passing from detection to no detection

        """
        self.n_sweep = sweeps_history_size
        self.sweep_buffer = []
        self.i = 0  # pointer to next sweep storage location in the buffer

        self.last_result = False
        self.thres_hi = thres_hi
        self.thres_lo = thres_lo

    def detect(self, sweep, get_result=True):
        """A shortcut function provided to aggregate the call to feed_sweep() and get_result(),
        to be called in a loop, at each sweep aquirement
        Provide the sweep to this function, and optionaly get the detection result

        Args
            sweep: numpy array of shape (n_samples,), to focus on specific samples,
            slice the original sweep and only provide the needed samples

        Returns:
            True if presence is detected, False if no presence is detected
            Always False if buffer is not full or if get_result=False.

        """
        self.feed_sweep(sweep)
        if get_result:
            return self.get_result()

    def feed_sweep(self, sweep):
        """Feed one sweep measurement to this object

        The instance will store the sweep in a buffer

        Args
            sweep: numpy array of complex values, with shape (n_samples,),
            to focus on specific samples, provide only the needed samples slice

        """
        # Initialization: fill the buffer
        if len(self.sweep_buffer) < self.n_sweep:
            self.sweep_buffer.append(sweep)
            return

        # Use circular buffer once enough sweeps are collected
        self.sweep_buffer[self.i] = sweep
        self.i = (self.i + 1) % self.n_sweep

    def get_result(self):
        """Run the detection algo and return result

            Call this funcion only when result is needed, less frequent than the sweep feed,
            this can reduce calculation load.

        Returns:
            True if presence is detected, False if no presence is detected
            Always False if buffer is not full or if get_result=False.

        """
        if self.is_ready():
            # Make presence detection
            sweep_complex = np.array(self.sweep_buffer)
            sweep_mean_removed = sweep_complex - np.mean(
                sweep_complex, axis=0, keepdims=True
            )
            sweep = abs(sweep_mean_removed)
            std_dev_per_sample = np.std(sweep, axis=0)
            score = max(std_dev_per_sample)

            presence = self.__hysteresis(score)
            return presence
        else:
            return False

    def is_ready(self):
        """Check if the instance is ready to make detections

        Mainly checks if there are enough sweep samples available to make the detection.
        If the instance is not ready, calling get_result() will always return False

        Returns:
            True if the instance is ready to make detection and return result
            False if the instance is not ready
        """
        return False if len(self.sweep_buffer) < self.n_sweep else True

    def __hysteresis(self, score):
        """Get stable detection result given the std-dev score

        Apply higher threshold for "no presence" to "presence" transision,
        and lower threshold for the opposite transition. This avoid the detction
        result from quickly oscillating between True and False under some
        tricky conditions.

        """
        thres = self.thres_lo if self.last_result else self.thres_hi
        presence = bool(score > thres)
        # print(score, thres, presence)
        self.last_result = presence
        return presence
