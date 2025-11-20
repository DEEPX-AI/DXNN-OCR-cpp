"""
Generator class for creating test options with IE reuse optimization
Ported from C++ version
"""

import json
import os
import random
from pathlib import Path
from typing import List, Dict, Any, Tuple
from dataclasses import dataclass
import sys

from dx_engine import DeviceStatus
from dx_engine import InferenceOption

@dataclass
class IEOption:
    """Options for IE creation (model~ieOption from JSON)"""
    model_path: str
    dynamic_cpu_offloading: str  # "on" or "off"
    thread_type: str            # "single-ie" or "multi-ie"
    thread_count: int           # thread count
    ort: bool                   # true or false
    bound: str                  # "NPU_ALL", "NPU_0", "NPU_12"
    device: str                 # "all", "1", "0,1"

@dataclass
class ExecutionOption:
    """Options for execution (inferenceFunction, inoutOption from JSON)"""
    inference_function: str     # "sync", "async", "batch"
    input_style: str           # "single" or "multi"
    output_buffer: str         # "user" or "internal"
    async_method: str          # "callback" or "wait"
    callback_delay: int        # delay value
    loop: int                  # loop count
    time: int                  # time value
    bitmatch: bool             # bitmatch enabled

@dataclass
class TestCase:
    """Test case that combines one IE option with multiple execution options"""
    ie_option: IEOption
    exec_options: List[ExecutionOption]

def is_valid_device_option(device_option: str, num_devices: int) -> bool:
    """Helper function to validate device option"""
    if device_option == "all":
        return True
    
    # Parse comma-separated device IDs
    try:
        segments = device_option.split(',')
        for segment in segments:
            segment = segment.strip()
            if not segment.isdigit():
                return False
            device_id = int(segment)
            if device_id < 0 or device_id >= num_devices:
                return False
        return True
    except:
        return False

class Generator:
    """Generator class for creating test options with IE reuse optimization"""
    
    def __init__(self, base_path: str, json_path: str, random_mode: bool):
        self.base_path = base_path
        self.json_path = json_path
        self.random_mode = random_mode
        self.model_paths: List[str] = []
        self.test_cases: List[TestCase] = []
        self.json_document: Dict[str, Any] = {}

    def load_json(self) -> bool:
        """Load and parse JSON file"""
        try:
            with open(self.json_path, 'r') as f:
                self.json_document = json.load(f)
        except Exception as e:
            print(f"Error loading JSON file: {e}")
            return False

        # Extract model directory paths
        self.model_paths.clear()
        
        if "model" in self.json_document and isinstance(self.json_document["model"], list):
            for model_dir in self.json_document["model"]:
                if isinstance(model_dir, str):
                    full_path = os.path.join(self.base_path, model_dir)
                    dxnn_file = self._find_dxnn_file_in_directory(full_path)
                    if dxnn_file:
                        self.model_paths.append(dxnn_file)
                    else:
                        print(f"Warning: No .dxnn file found in {full_path}")
        else:
            print("Error: 'model' array not found in JSON")
            return False

        return True

    def _find_dxnn_file_in_directory(self, directory_path: str) -> str:
        """Find .dxnn file in the given directory"""
        try:
            for filename in os.listdir(directory_path):
                if filename.endswith('.dxnn'):
                    file_path = os.path.join(directory_path, filename)
                    if os.path.isfile(file_path):
                        return file_path
        except Exception as e:
            print(f"Error accessing directory {directory_path}: {e}")
        
        return ""

    def generate_test_cases(self):
        """Generate combined test cases (IE options + execution options)"""
        ie_options = self._generate_ie_options(self.json_document, self.model_paths)
        exec_options = self._generate_execution_options(self.json_document)

        self.test_cases = []
        for ie_option in ie_options:
            test_case = TestCase(ie_option=ie_option, exec_options=exec_options)
            self.test_cases.append(test_case)

        print(f"Generated {len(self.test_cases)} test cases")

    def _generate_ie_options(self, config: Dict[str, Any], model_paths: List[str]) -> List[IEOption]:
        """Generate IE options (model~ieOption combinations)"""
        ie_options = []
        
        # Get available device count for validation
        num_devices = 0
        try:
            num_devices = DeviceStatus.get_device_count()
        except Exception as e:
            print(f"Warning: Could not get device count: {e}")
            num_devices = -1 

        # Extract options using helper functions
        dyn_cpu_options = self._extract_string_array(config, ["configuration", "dynamic-cpu-offloading"])
        ort_options = self._extract_bool_array(config, ["ieOption", "ort"])
        bound_options = self._extract_string_array(config, ["ieOption", "bound"])
        device_options = self._extract_string_array(config, ["ieOption", "device"])

        # Extract thread style options
        thread_styles = []
        if "threadStyle" in config:
            for thread_style in config["threadStyle"]:
                if isinstance(thread_style, dict):
                    thread_type = thread_style.get("type", "single-ie")
                    thread_count = thread_style.get("count", 1)
                    thread_styles.append((thread_type, thread_count))
        else:
            # Default thread style if not specified
            thread_styles.append(("single-ie", 1))

        # Generate all combinations or random selection
        for model_path in model_paths:
            for dyn_cpu in dyn_cpu_options:
                for ort in ort_options:
                    for bound in bound_options:
                        for device in device_options:
                            # Validate device option
                            if not is_valid_device_option(device, num_devices):
                                print(f"Warning: Invalid device option '{device}' for {num_devices} devices")
                                continue

                            for thread_type, thread_count in thread_styles:
                                ie_option = IEOption(
                                    model_path=model_path,
                                    dynamic_cpu_offloading=dyn_cpu,
                                    thread_type=thread_type,
                                    thread_count=thread_count,
                                    ort=ort,
                                    bound=bound,
                                    device=device
                                )
                                ie_options.append(ie_option)

        # Randomize if requested
        if self.random_mode and ie_options:
            random.shuffle(ie_options)
            # Take a random subset (e.g., 50% of total)
            subset_size = max(1, len(ie_options) // 2)
            ie_options = ie_options[:subset_size]

        return ie_options

    def _generate_execution_options(self, config: Dict[str, Any]) -> List[ExecutionOption]:
        """Generate execution options from JSON"""
        exec_options = []
        
        # Extract execution options
        infer_funcs = self._extract_string_array(config, ["inferenceFunction"])
        input_styles = self._extract_string_array(config, ["inoutOption", "inputStyle"])
        output_buffers = self._extract_string_array(config, ["inoutOption", "outputBuffer"])
        async_methods = self._extract_string_array(config, ["inoutOption", "asyncMethod"])
        callback_delays = self._extract_int_array(config, ["inoutOption", "callbackDelay"])
        loops = self._extract_int_array(config, ["inoutOption", "loop"])
        times = self._extract_int_array(config, ["inoutOption", "time"])
        bitmatches = self._extract_bool_array(config, ["inoutOption", "bitmatch"])

        # Generate combinations
        for infer_func in infer_funcs:
            for input_style in input_styles:
                for output_buffer in output_buffers:
                    for async_method in async_methods:
                        for callback_delay in callback_delays:
                            for loop in loops:
                                for time in times:
                                    for bitmatch in bitmatches:
                                        exec_option = ExecutionOption(
                                            inference_function=infer_func,
                                            input_style=input_style,
                                            output_buffer=output_buffer,
                                            async_method=async_method,
                                            callback_delay=callback_delay,
                                            loop=loop,
                                            time=time,
                                            bitmatch=bitmatch
                                        )
                                        exec_options.append(exec_option)

        # Randomize if requested
        if self.random_mode and exec_options:
            random.shuffle(exec_options)
            # Take a random subset
            subset_size = max(1, len(exec_options) // 2)
            exec_options = exec_options[:subset_size]

        return exec_options

    def _extract_string_array(self, config: Dict[str, Any], keys: List[str]) -> List[str]:
        """Helper function to extract string array from nested JSON"""
        current = config
        try:
            for key in keys:
                current = current[key]
            if isinstance(current, list):
                return [str(item) for item in current if isinstance(item, (str, int, float))]
            else:
                return [str(current)]
        except (KeyError, TypeError):
            return []

    def _extract_bool_array(self, config: Dict[str, Any], keys: List[str]) -> List[bool]:
        """Helper function to extract bool array from nested JSON"""
        current = config
        try:
            for key in keys:
                current = current[key]
            if isinstance(current, list):
                return [bool(item) for item in current]
            else:
                return [bool(current)]
        except (KeyError, TypeError):
            return []

    def _extract_int_array(self, config: Dict[str, Any], keys: List[str]) -> List[int]:
        """Helper function to extract int array from nested JSON"""
        current = config
        try:
            for key in keys:
                current = current[key]
            if isinstance(current, list):
                return [int(item) for item in current if isinstance(item, (int, float))]
            else:
                return [int(current)]
        except (KeyError, TypeError):
            return []

    def get_test_cases(self) -> List[TestCase]:
        """Get generated test cases"""
        return self.test_cases

    def print_test_cases(self):
        """Print test cases for debugging"""
        if not self.test_cases:
            print("No test cases generated")
            return

        print("=== Generated Test Cases Summary ===")
        print(f"Total Test Cases: {len(self.test_cases)}")
        
        for i, test_case in enumerate(self.test_cases):
            print(f"\n--- Test Case {i+1} ---")
            ie_opt = test_case.ie_option
            print(f"Model: {ie_opt.model_path}")
            print(f"Dynamic CPU Offloading: {ie_opt.dynamic_cpu_offloading}")
            print(f"Thread Type: {ie_opt.thread_type}")
            print(f"Thread Count: {ie_opt.thread_count}")
            print(f"ORT: {ie_opt.ort}")
            print(f"Bound: {ie_opt.bound}")
            print(f"Device: {ie_opt.device}")
            print(f"Execution Options: {len(test_case.exec_options)}")

    def check_for_duplicates(self):
        """Check for duplicate test cases and execution options"""
        print("========== DUPLICATE CHECK ==========")
        
        # Check for duplicate IE options
        ie_option_groups = {}
        for i, test_case in enumerate(self.test_cases):
            ie_key = (
                test_case.ie_option.model_path,
                test_case.ie_option.dynamic_cpu_offloading,
                test_case.ie_option.thread_type,
                test_case.ie_option.thread_count,
                test_case.ie_option.ort,
                test_case.ie_option.bound,
                test_case.ie_option.device
            )
            
            if ie_key not in ie_option_groups:
                ie_option_groups[ie_key] = []
            ie_option_groups[ie_key].append(i)
        
        # Report duplicate IE options
        duplicate_ie_groups = 0
        for ie_key, indices in ie_option_groups.items():
            if len(indices) > 1:
                duplicate_ie_groups += 1
                print(f"[DUPLICATE IE] Group {duplicate_ie_groups}: TestCases {indices}")
                print(f"  Model: {ie_key[0]}")
                print(f"  Dynamic CPU Offloading: {ie_key[1]}")
                print(f"  Thread: {ie_key[2]} ({ie_key[3]})")
                print(f"  ORT: {ie_key[4]}")
                print(f"  Bound: {ie_key[5]}")
                print(f"  Device: {ie_key[6]}")
        
        # Check for duplicate execution options within each test case
        duplicate_exec_groups = 0
        for i, test_case in enumerate(self.test_cases):
            exec_option_counts = {}
            for j, exec_option in enumerate(test_case.exec_options):
                exec_key = (
                    exec_option.inference_function,
                    exec_option.input_style,
                    exec_option.output_buffer,
                    exec_option.async_method,
                    exec_option.callback_delay,
                    exec_option.loop,
                    exec_option.time,
                    exec_option.bitmatch
                )
                
                if exec_key not in exec_option_counts:
                    exec_option_counts[exec_key] = []
                exec_option_counts[exec_key].append(j)
            
            # Report duplicates in this test case
            for exec_key, indices in exec_option_counts.items():
                if len(indices) > 1:
                    duplicate_exec_groups += 1
                    print(f"[DUPLICATE EXEC] TestCase {i}, ExecOptions {indices}")
                    print(f"  Function: {exec_key[0]}")
                    print(f"  Input: {exec_key[1]}")
                    print(f"  Output: {exec_key[2]}")
                    print(f"  Async: {exec_key[3]}")
                    print(f"  Loop/Time: {exec_key[5]}/{exec_key[6]}")
                    print(f"  Bitmatch: {exec_key[7]}")
        
        # Summary
        print(f"========== DUPLICATE SUMMARY ==========")
        print(f"Duplicate IE Option Groups: {duplicate_ie_groups}")
        print(f"Duplicate Execution Option Groups: {duplicate_exec_groups}")
        print("=========================================")

def set_inference_configuration_from_ie_option(inference_option, ie_option: IEOption):
    """Set inference configuration from IE option
    This function needs to be implemented with actual DXRT Python API
    """

    # Set ORT option (now boolean)
    inference_option.useORT = ie_option.ort
    
    if ie_option.bound == "NPU_ALL":
        inference_option.boundOption = InferenceOption.BOUND_OPTION.NPU_ALL
    
    elif ie_option.bound == "NPU_0":
        inference_option.boundOption = InferenceOption.BOUND_OPTION.NPU_0
    
    elif ie_option.bound == "NPU_1":

        inference_option.boundOption = InferenceOption.BOUND_OPTION.NPU_1

    elif ie_option.bound == "NPU_2":
        inference_option.boundOption = InferenceOption.BOUND_OPTION.NPU_2

    elif ie_option.bound == "NPU_01" or ie_option.bound == "NPU_0/1":
        inference_option.boundOption = InferenceOption.BOUND_OPTION.NPU_01;  # NPU_0/1
    
    elif ie_option.bound == "NPU_12" or ie_option.bound == "NPU_1/2":
        inference_option.boundOption = InferenceOption.BOUND_OPTION.NPU_12;  # NPU_1/2

    elif ie_option.bound == "NPU_02" or ie_option.bound == "NPU_0/2":
        inference_option.boundOption = InferenceOption.BOUND_OPTION.NPU_02;  # NPU_0/2

    else:
        print(f"Error: Invalid bound option: {ie_option.bound}")
        sys.exit(-1)
    

    # Set device options
    inference_option.devices = [];  # Clear any existing devices
    
    if ie_option.device == "all":
        # Leave devices empty for "all" - engine will use all available devices
        pass
    
    else:
        device_options = ie_option.device.split(',')

        for device_str in device_options:
            device_str = device_str.strip()

            if device_str.isdigit():
                device_id = int(device_str)
                inference_option.devices.append(device_id)
                has_valid_device = True

            else:
                print(f"Error: Invalid device option: {ie_option.device} (contains non-numeric value: '{device_str}')")
                sys.exit(-1)
        
        # If no valid devices were parsed, it's an error
        if not has_valid_device:
            print(f"Error: Invalid device option: {ie_option.device} (no valid device IDs found)")
            sys.exit(-1)
        
    