#
# Copyright (C) 2018- DEEPX Ltd.
# All rights reserved.
#
# This software is the property of DEEPX and is provided exclusively to customers 
# who are supplied with DEEPX NPU (Neural Processing Unit). 
# Unauthorized sharing or usage is strictly prohibited by law.
#

import numpy as np
import subprocess
import re
import pathlib
import shutil
import time
import struct
import warnings
from typing import List, Dict, Any, Optional, Tuple
import os
import sys 

# Constants
RTOL = 1e-4
ATOL = 1e-2
TIME_THRESHOLD_HOURS = 1  # Hours for file age threshold
DEFAULT_TIMEOUT = 10  # Seconds for subprocess timeout
MAX_DIFF_INDICES_TO_LOG = 10  # Maximum number of different indices to log
FLOAT32_BYTES = 4  # Bytes per float32

#def int8_to_float32(arr: np.ndarray) -> np.ndarray:
#    return arr.view(np.float32)
def int8_to_float32(arr):
    """Convert int8 array to float32 array.
    
    Args:
        arr: Input int8 array
        
    Returns:
        Float32 array view of the input
        
    Raises:
        ValueError: If array size is not divisible by 4
    """
    if len(arr) % FLOAT32_BYTES != 0:
        raise ValueError(f"Array size {len(arr)} is not divisible by {FLOAT32_BYTES}")
    return arr.view(np.int8).reshape(-1, FLOAT32_BYTES).view(np.float32).squeeze()

def compare_int8_arrays_fast(arr1: np.ndarray, arr2: np.ndarray, rtol=RTOL, atol=ATOL):
    if len(arr1) != len(arr2):
        return False, 0

    float_arr1 = int8_to_float32(arr1)
    float_arr2 = int8_to_float32(arr2)

    mask = ~np.isclose(float_arr1, float_arr2, rtol=rtol, atol=atol)

    if np.any(mask):
        first_fail_index = np.where(mask)[0][0] * 4  
        warnings.warn(
            f"[FLOAT32 NOT CLOSE] gt != rt, first fail at index {first_fail_index}: "
            f"rt : {float_arr1[first_fail_index // 4]}, "
            f"gt : {float_arr2[first_fail_index // 4]}, "
            f"error(r) : {(float_arr2[first_fail_index // 4] - float_arr1[first_fail_index // 4]) / float_arr2[first_fail_index // 4] * 100}%\n"
            f"{arr1[first_fail_index:first_fail_index+4]}, {arr2[first_fail_index:first_fail_index+4]}"
        )
        return False, first_fail_index

    return True, 0

def _extract_numbers_from_text(text: str, context: str = "") -> List[int]:
    """Extract all numbers from text for fallback parsing.
    
    Args:
        text: Text to extract numbers from
        context: Context description for logging
        
    Returns:
        List of integers found in the text
    """
    try:
        numbers = re.findall(r'\d+', text)
        return [int(num) for num in numbers]
    except Exception as e:
        print(f"Warning: Failed to extract numbers from {context}: {e}", file=sys.stderr)
        return []

def _extract_numbers_from_text(text: str, context: str = "") -> List[int]:
    """Extract all numbers from text for fallback parsing.
    
    Args:
        text: Text to extract numbers from
        context: Context description for logging
        
    Returns:
        List of integers found in the text
    """
    try:
        numbers = re.findall(r'\d+', text)
        return [int(num) for num in numbers]
    except Exception as e:
        print(f"Warning: Failed to extract numbers from {context}: {e}", file=sys.stderr)
        return []

def get_dxrt_info():
    """Get DXRT system information with flexible parsing.
    
    This function is designed to be robust against format changes in dxrt-cli output.
    It uses multiple parsing strategies and fallback mechanisms.
    
    Returns:
        dict: System information including memory speed, PCIe info, and NPU details
        
    Raises:
        RuntimeError: If dxrt-cli command fails or returns invalid data
    """
    try:
        result = subprocess.run(
            "dxrt-cli -s",
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=DEFAULT_TIMEOUT
        )
        
        if result.returncode != 0:
            raise RuntimeError(f"dxrt-cli command failed: {result.stderr}")
            
        output = result.stdout
        if not output:
            raise RuntimeError("dxrt-cli returned empty output")
            
    except subprocess.TimeoutExpired:
        raise RuntimeError(f"dxrt-cli command timed out after {DEFAULT_TIMEOUT} seconds")
    except Exception as e:
        raise RuntimeError(f"Failed to execute dxrt-cli: {e}")

    # Initialize with default values
    info = {
        'memory_speed_MHz': 0,
        'pcie': "Unknown",
        'npus': []
    }

    try:
        # Parse memory information with multiple patterns
        memory_patterns = [
            # New format: "* Memory : LPDDR5 5400 MHz, 3.92GiB"
            r'\*\s*Memory\s*:\s*\S+\s+(\d+)\s*MHz',
            # Old format: "* Memory : SOMETHING 1234 MHz"
            r'\*\s*Memory\s*:\s*.*?(\d+)\s*(MHz|Mbps)',
            # Generic number extraction from memory line
            r'Memory.*?(\d+).*?(MHz|Mbps)'
        ]
        
        for pattern in memory_patterns:
            mem_match = re.search(pattern, output, re.IGNORECASE)
            if mem_match:
                try:
                    info['memory_speed_MHz'] = int(mem_match.group(1))
                    break
                except (ValueError, IndexError):
                    continue

        # Parse PCIe information with multiple patterns
        pcie_patterns = [
            # New format: "* PCIe   : Gen3 X4 [11:00:00]"
            r'\*\s*PCIe\s*:\s*([^\[\n]+?)(?:\s*\[|$)',
            # Old format: "* PCIe : Gen3 X4 [address]"
            r'\*\s*PCIe\s*:\s*([^\[\n]+)',
            # Generic PCIe line extraction
            r'PCIe.*?:\s*([^\[\n]+?)(?:\s*\[|$)'
        ]
        
        for pattern in pcie_patterns:
            pcie_match = re.search(pattern, output, re.IGNORECASE)
            if pcie_match:
                pcie_info = pcie_match.group(1).strip()
                if pcie_info and pcie_info != ':':
                    info['pcie'] = pcie_info
                    break

        # Parse NPU information with flexible patterns
        npus = []
        
        # Multiple NPU patterns to handle format variations
        npu_patterns = [
            # Current format: "NPU 0: voltage 750 mV, clock 1000 MHz, temperature 38'C"
            r'NPU\s+(\d+):\s+voltage\s+(\d+)\s*mV,\s*clock\s+(\d+)\s*MHz,\s*temperature\s+(-?\d+)',
            # Alternative format without units
            r'NPU\s+(\d+).*?voltage\s+(\d+).*?clock\s+(\d+).*?temperature\s+(-?\d+)',
            # More flexible pattern
            r'NPU\s+(\d+)[^\n]*?(\d+)\s*mV[^\n]*?(\d+)\s*MHz[^\n]*?(-?\d+)\s*[\'"]?C'
        ]
        
        for pattern in npu_patterns:
            npu_matches = re.finditer(pattern, output, re.IGNORECASE)
            temp_npus = []
            
            for match in npu_matches:
                try:
                    npu_id = int(match.group(1))
                    voltage = int(match.group(2))
                    clock = int(match.group(3))
                    temp = int(match.group(4))
                    
                    npu_info = {
                        'id': npu_id,
                        'voltage_mV': voltage,
                        'clock_MHz': clock,
                        'temperature_C': temp
                    }
                    temp_npus.append(npu_info)
                except (ValueError, IndexError) as e:
                    print(f"Warning: Failed to parse NPU info from match {match.group(0)}: {e}", file=sys.stderr)
                    continue
            
            if temp_npus:  # If we found NPUs with this pattern, use them
                npus = temp_npus
                break
        
        # Sort NPUs by ID if available
        if npus:
            npus.sort(key=lambda x: x.get('id', 0))
            # Remove ID from final output to maintain compatibility
            for npu in npus:
                npu.pop('id', None)
        
        # Ensure we have at least 3 NPUs with default values if needed
        while len(npus) < 3:
            npus.append({
                'voltage_mV': 0,
                'clock_MHz': 0,
                'temperature_C': 0
            })
        
        info['npus'] = npus
        
        # Log successful parsing for debugging (only in verbose mode)
        parsed_items = []
        if info['memory_speed_MHz'] > 0:
            parsed_items.append(f"Memory: {info['memory_speed_MHz']}MHz")
        if info['pcie'] != "Unknown":
            parsed_items.append(f"PCIe: {info['pcie']}")
        active_npus = len([n for n in npus if n['voltage_mV'] > 0])
        if active_npus > 0:
            parsed_items.append(f"NPUs: {active_npus}")
            
        if parsed_items and os.environ.get('DXRT_VERBOSE_PARSING', '0') == '1':
            print(f"Debug: Successfully parsed DXRT info - {', '.join(parsed_items)}", file=sys.stderr)
        
    except Exception as e:
        # If parsing fails, log the error but don't crash
        print(f"Warning: Failed to parse dxrt-cli output: {e}", file=sys.stderr)
        if os.environ.get('DXRT_DEBUG_PARSING', '0') == '1':
            print(f"Debug: dxrt-cli output was:\n{output}", file=sys.stderr)
        
        # Keep default values already set in info
        pass

    return info

def move_to_rt_dir(rt_dir):
    """Move recent output files to RT directory.
    
    Args:
        rt_dir: Target directory path
        
    Raises:
        OSError: If directory creation or file operations fail
    """
    try:
        current_dir = pathlib.Path(".")
        rt_dir = pathlib.Path(rt_dir)

        rt_dir.mkdir(exist_ok=True, parents=True)

        time_threshold = time.time() - (TIME_THRESHOLD_HOURS * 3600)
        
        moved_count = 0
        for file in current_dir.glob("*put*.bin"):
            try:
                if file.is_file() and file.stat().st_ctime > time_threshold:
                    dest_file = rt_dir / file.name
                    
                    # Check if destination already exists
                    if dest_file.exists():
                        print(f"Warning: Destination file {dest_file} already exists, overwriting")
                    
                    shutil.copy2(str(file), str(dest_file))
                    file.unlink() 
                    print(f"Copied & Removed: {file} -> {dest_file}")
                    moved_count += 1
            except (OSError, IOError) as e:
                print(f"Error processing file {file}: {e}", file=sys.stderr)
                continue
                
        if moved_count == 0:
            print(f"No recent output files found to move to {rt_dir}")
        else:
            print(f"Successfully moved {moved_count} files to {rt_dir}")
            
    except Exception as e:
        raise OSError(f"Failed to move files to RT directory {rt_dir}: {e}")


def pcie_rescan():
    """Rescans PCIe devices."""
    print("Rescanning PCIe devices...")
    try:
        result = subprocess.run(
            "pcie_rescan",
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=True
        )
        print("PCIe rescan completed.")
    except subprocess.CalledProcessError as e:
        print(f"Error executing pcie_rescan command: {e}")
        print("stderr:", e.stderr)
    except FileNotFoundError:
        print("Error: 'pcie_rescan' command not found. Ensure it's in your system's PATH.")
    except Exception as e:
        print(f"An unexpected error occurred during pcie_rescan: {e}")


def process_paths(model_path, gt_dir, rt_dir, performance_mode, reg=0):
    path_parts = model_path.split("/")
    model_dir = ""
    model_name = path_parts[-1]
    
    if not model_name or model_name.endswith('.dxnn'):
        model_name = path_parts[-2]
        if model_name == ".":
            model_name = path_parts[-1].partition(".dxnn")[0]
    if os.path.isdir(model_path):
        model_dir = os.path.abspath(model_path)
        dxnn = None
        for filename in os.listdir(model_dir):
            if filename.lower().endswith('.dxnn'):
                dxnn = os.path.join(model_dir, filename)
                break
    elif os.path.isfile(model_path) and model_path.lower().endswith('.dxnn'):
        dxnn = os.path.abspath(model_path)
        model_dir = os.path.dirname(dxnn)
    else:
        print("model_path must be a model directory or a .dxnn file path.")
        #raise ValueError("model_path must be a model directory or a .dxnn file path.")
        return model_dir, model_name, None, None, None

    if not performance_mode:
        if gt_dir == "gt":
            gt_dir = os.path.join(model_dir, "gt")
            if not os.path.isdir(gt_dir):
                print(f"The 'gt' directory does not exist in the model directory '{model_dir}'.")
                return model_dir, model_name, None, None, None
                #raise FileNotFoundError(f"The 'gt' directory does not exist in the model directory '{model_dir}'.")
        else:
            gt_dir = os.path.abspath(gt_dir)

    if int(os.environ.get('DXRT_DEBUG_DATA', '0')) > 0:
        if rt_dir == "rt":
            rt_dir = os.path.join(model_dir, "rt")
            if not os.path.exists(rt_dir):
                os.makedirs(rt_dir)
        else:
            if reg:
                rt_dir = os.path.join(rt_dir, model_name)
                if not os.path.exists(rt_dir):
                    os.makedirs(rt_dir)
            else:
                rt_dir = os.path.abspath(rt_dir)
                if not os.path.exists(rt_dir):
                    os.makedirs(rt_dir)
                    
    return model_dir, model_name, dxnn, gt_dir, rt_dir


def parse_devices_option(devices_option_str: str) -> List[int]: 
    """Parse device specification string.
    
    Args:
        devices_option_str: Device specification string
        
    Returns:
        List of device IDs
        
    Raises:
        ValueError: If device specification is invalid
        SystemExit: For critical parsing errors
    """
    if not isinstance(devices_option_str, str):
        raise ValueError(f"devices_option_str must be a string, got {type(devices_option_str)}")
        
    devices_list_for_op: List[int] = []
    devices_spec_str = devices_option_str.strip().lower()

    if not devices_spec_str:
        devices_list_for_op = [] 
    elif devices_spec_str == "all":
        devices_list_for_op = [] 
        print("Device specification: 'all' (engine default)")
    elif devices_spec_str.startswith("count:"):
        try:
            count_str = devices_spec_str.split(":", 1)[1]
            count = int(count_str)
            if count > 0:
                devices_list_for_op = list(range(count))
                print(f"Device specification: First {count} NPU(s) {devices_list_for_op}")
            else:
                print(f"[ERR] Device count in '{devices_option_str}' must be positive.", file=sys.stderr)
                sys.exit(-1)
        except (IndexError, ValueError) as e:
            print(f"[ERR] Invalid format for 'count:N' in --devices '{devices_option_str}'. Expected e.g., 'count:2'. Error: {e}", file=sys.stderr)
            sys.exit(-1)
    else: 
        try:
            devices_list_for_op = [int(x.strip()) for x in devices_spec_str.split(',') if x.strip()]
            if not devices_list_for_op and devices_spec_str: 
                 print(f"[WARN] No valid device IDs parsed from --devices string: '{devices_option_str}'. Using engine default for devices.", file=sys.stderr)
                 devices_list_for_op = []
            elif devices_list_for_op:
                # Validate device IDs are non-negative
                if any(device_id < 0 for device_id in devices_list_for_op):
                    raise ValueError("Device IDs must be non-negative")
                print(f"Device specification: Specific NPU(s) {devices_list_for_op}")
        except ValueError as e:
            print(f"[ERR] Invalid device ID in --devices list '{devices_option_str}'. Expected comma-separated integers e.g., '0,1'. Error: {e}", file=sys.stderr)
            sys.exit(-1)
    return devices_list_for_op

# Multi-input support utilities

def split_input_data_for_multi_input(single_input_data: np.ndarray, 
                                   input_tensor_info: List[Dict[str, Any]]) -> Dict[str, np.ndarray]:
    """
    Split concatenated input data into separate tensors for multi-input models.
    
    Args:
        single_input_data: Concatenated input data as numpy array
        input_tensor_info: List of input tensor information dictionaries
                          Each dict should contain 'name', 'shape', 'dtype', 'elem_size'
    
    Returns:
        Dictionary mapping tensor names to their respective data
        
    Raises:
        ValueError: If input validation fails
        TypeError: If input types are incorrect
    """
    if not isinstance(single_input_data, np.ndarray):
        raise TypeError(f"single_input_data must be numpy array, got {type(single_input_data)}")
    
    if not isinstance(input_tensor_info, list):
        raise TypeError(f"input_tensor_info must be list, got {type(input_tensor_info)}")
    
    if len(input_tensor_info) <= 1:
        # Single input model, return as-is with first tensor name
        if input_tensor_info:
            required_keys = ['name']
            if not all(key in input_tensor_info[0] for key in required_keys):
                raise ValueError(f"input_tensor_info[0] missing required keys: {required_keys}")
            return {input_tensor_info[0]['name']: single_input_data}
        else:
            return {'input': single_input_data}
    
    # Multi-input model: split the data
    input_tensors = {}
    offset = 0
    total_expected_size = 0
    
    # First pass: calculate total expected size
    for tensor_info in input_tensor_info:
        required_keys = ['name', 'shape', 'elem_size']
        if not all(key in tensor_info for key in required_keys):
            raise ValueError(f"tensor_info missing required keys: {required_keys}")
            
        tensor_shape = tensor_info['shape']
        if not isinstance(tensor_shape, (list, tuple)) or not tensor_shape:
            raise ValueError(f"Invalid tensor_shape: {tensor_shape}")
            
        tensor_size = int(np.prod(tensor_shape)) * tensor_info['elem_size']
        if tensor_size <= 0:
            raise ValueError(f"Invalid tensor size: {tensor_size}")
            
        total_expected_size += tensor_size
    
    # Validate input data size
    if single_input_data.size < total_expected_size:
        raise ValueError(f"Input data size {single_input_data.size} is smaller than expected {total_expected_size}")
    
    # Second pass: split the data
    for tensor_info in input_tensor_info:
        tensor_name = tensor_info['name']
        tensor_shape = tensor_info['shape']
        tensor_size = int(np.prod(tensor_shape)) * tensor_info['elem_size']
        
        # Extract data for this tensor
        if offset + tensor_size > single_input_data.size:
            raise ValueError(f"Not enough data for tensor {tensor_name}: need {tensor_size}, available {single_input_data.size - offset}")
            
        tensor_data = single_input_data[offset:offset + tensor_size]
        input_tensors[tensor_name] = tensor_data
        
        offset += tensor_size
        
        #print(f"Split input tensor '{tensor_name}': shape={tensor_shape}, size={tensor_size} bytes")
    
    return input_tensors

def prepare_multi_input_data_list(input_data_list: List[List[np.ndarray]], 
                                ie, 
                                performance_mode: bool = False) -> List[Dict[str, np.ndarray]]:
    """
    Prepare input data list for multi-input models.
    
    Args:
        input_data_list: List of input data (each item is [concatenated_data])
        ie: InferenceEngine instance
        performance_mode: Whether in performance mode
        
    Returns:
        List of dictionaries mapping tensor names to their data
    """
    multi_input_data_list = []
    
    # Check if multi-input model
    if not ie.is_multi_input_model():
        # Single input model - return as-is but in dict format
        for input_data in input_data_list:
            multi_input_data_list.append({'input': input_data[0]})
        return multi_input_data_list
    
    # Multi-input model
    if performance_mode:
        # Performance mode: create dummy data for each input tensor
        input_tensor_names = ie.get_input_tensor_names()
        input_tensors_info = ie.get_input_tensors_info()  
        
        dummy_inputs = {}
        for i, tensor_name in enumerate(input_tensor_names):
            tensor_info = input_tensors_info[i]
            tensor_shape = tensor_info['shape']
            tensor_size = int(np.prod(tensor_shape)) * tensor_info['elem_size']
            dummy_inputs[tensor_name] = np.zeros(tensor_size, dtype=np.int8)
            
        multi_input_data_list.append(dummy_inputs)
    else:
        # Regular mode: split concatenated input data
        input_tensors_info = ie.get_input_tensors_info() 
        
        for input_data in input_data_list:
            concatenated_data = input_data[0]
            split_inputs = split_input_data_for_multi_input(concatenated_data, input_tensors_info)
            multi_input_data_list.append(split_inputs)
    
    return multi_input_data_list

def convert_multi_input_to_api_format(multi_input_data: Dict[str, np.ndarray],
                                    api_type: str = 'vector') -> Any:
    """
    Convert multi-input data to appropriate API format.
    
    Args:
        multi_input_data: Dictionary mapping tensor names to data
        api_type: 'vector' for vector<void*> API, 'dict' for dictionary API
        
    Returns:
        List of data pointers for vector API, or dict for dictionary API
    """
    if api_type == 'dict':
        return multi_input_data
    elif api_type == 'vector':
        # Return list of numpy arrays for vector API
        return [data for data in multi_input_data.values()]
    else:
        raise ValueError(f"Unsupported api_type: {api_type}")

def log_multi_input_info(ie, logger_func=print):
    """
    Log multi-input model information.
    
    Args:
        ie: InferenceEngine instance
        logger_func: Function to use for logging (default: print)
    """
    if ie.is_multi_input_model():
        input_count = ie.get_input_tensor_count()
        input_names = ie.get_input_tensor_names()
        input_mapping = ie.get_input_tensor_to_task_mapping()
        
        logger_func(f"Multi-input model detected:")
        logger_func(f"  - Input tensor count: {input_count}")
        logger_func(f"  - Input tensor names: {input_names}")
        logger_func(f"  - Input tensor to task mapping: {input_mapping}")