import sys
import numpy as np
import pickle
INT_MAX = 2 ** 31
INT_MIN = -2 ** 31
MIN_GAMMA = -23

def calculate_gamma_actual(tensor_data, pc_lower=0, pc_upper=100):
        """This method calculates gamma value for a given tensor. Gamma is the minimum required
        scaling for a tensor data to be in range of [-1,1)

        Args:
            tensor_data (np.array): input data
            pc_lower (int, optional): lower bound for inclusion percentile. Defaults to 0.
            pc_upper (int, optional): higher bound for inclusion percentile. Defaults to 100.
        Returns:
            int: gamma
        """
        _tensor_data = tensor_data
        _tensor_data = np.sort(_tensor_data.flatten())
        num_elems = _tensor_data.size
        index_lower = max(np.round(pc_lower * num_elems / 100.0).astype(np.int32), 0)
        index_upper = min(np.round(pc_upper * (num_elems - 1) / 100.0).astype(np.int32), num_elems - 1)
        max_val = max(abs(_tensor_data[index_lower]), abs(_tensor_data[index_upper]))
        if max_val < 2 ** MIN_GAMMA:
            gamma = MIN_GAMMA
        if(max_val > 1):
            gamma = np.ceil(np.log2(max_val))
            gamma = int(gamma)
        else:
            gamma = 0
        return gamma

def convert_pickle_to_raw(input_pickle_file, output_bin_file, batch_size=None):
    '''
    It reads pickle file and write it into raw file
    '''
    with open(input_pickle_file, "rb") as f:
        input_data = pickle.load(f)
    input_data = input_data["inputs"]
    
    # quantize
    for name, data in input_data.items():
        scale_factor = calculate_gamma_actual(data,0,100)
        data = data / (2**scale_factor)
        data = (data).astype(np.float32) * INT_MAX
        data = np.clip(data, INT_MIN, INT_MAX - 1)
        data = np.round(data).astype(np.int32).byteswap()
        input_data[name] = data
    
    serial_input = bytearray()
    batch_size_total = len(list(input_data.values())[0])
    if (batch_size is None) or (batch_size > batch_size_total) or (batch_size <= 0):
        batch_size = batch_size_total
    for j in range(batch_size):
        for i in input_data:
            input_s = input_data[i]
            serial_input.extend(input_s[j].tobytes())
            
    with open(output_bin_file, "wb") as f:
        f.write(serial_input)