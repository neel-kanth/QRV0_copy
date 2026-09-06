import numpy as np
from collections import deque
from scipy.stats import circmean
from typing import NamedTuple
from abc import ABC, abstractmethod


class PDOAs(NamedTuple):
    pdoa1: float
    pdoa2: float
    pdoa3: float


class PDOAFilter(ABC):
    @abstractmethod
    def filter(self, pdoas: PDOAs):
        pass


class MovingAveragePDOAFilter(PDOAFilter):
    def __init__(self, roll_average_buf_len: int = 50):
        self.roll_buffer = deque(maxlen=roll_average_buf_len)

    def filter(self, pdoas: PDOAs):
        self.roll_buffer.append(pdoas)
        rad_pdoas = np.radians(self.roll_buffer)
        return np.degrees(circmean(rad_pdoas, axis=0))


class PDOAtoAOAConverter:
    def __init__(
        self,
        filter: PDOAFilter,
        lut_file_name: str,
        pdoa1_weight: float = 0.45,
        pdoa2_weight: float = 0.45,
        pdoa3_weight: float = 0.1,
    ):
        self.lookup_table = np.load(lut_file_name)["arr_0"]
        self.weights = np.array([pdoa1_weight, pdoa2_weight, pdoa3_weight])
        self.pdoa_filter = filter

    def get_aoa(self, pdoas: PDOAs) -> int:
        lookup_pdoas = self.lookup_table[:, 1:]
        diff_array = lookup_pdoas - self.pdoa_filter.filter(pdoas)
        normalized_angles = np.mod(diff_array, 360)
        translated_angles = np.where(
            normalized_angles > 180, 360 - normalized_angles, normalized_angles
        )
        weighted_angles = np.multiply(translated_angles, self.weights)
        rms_values = np.sqrt(np.mean(np.array(weighted_angles) ** 2, axis=1))
        idx = np.argmin(rms_values)

        return self.lookup_table[idx][0]
