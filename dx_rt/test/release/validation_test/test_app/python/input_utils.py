"""
InputUtils class for creating input buffers
Ported from C++ version
"""

import os
import random
import numpy as np
from typing import Optional

from generator import TestCase, ExecutionOption

from dx_engine import InferenceEngine

class InputUtils:
    """Utility class for creating and managing input buffers"""
    
    def __init__(self, exec_option: ExecutionOption, test_case: TestCase, ie: InferenceEngine):
        """
        Initialize InputUtils
        
        Args:
            exec_option: Execution configuration
            test_case: Test case configuration
            ie: InferenceEngine instance (TODO: replace with actual DXRT Python API)
        """
        self.exec_option = exec_option
        self.test_case = test_case
        self.ie = ie
        
        self.input_buffer: np.ndarray = None
        self.file_path: str = ""
        self.input_size: int = 0
        self.version: int = 6  # Default version
        
    def __del__(self):
        """Cleanup resources"""
        self.input_buffer = None
        self.file_path = ""
        
    def create_input_buffer(self):
        """Create input buffer based on execution options"""
        self.file_path = self._get_input_file_path()
        
        # Check if file exists and get size
        if not os.path.exists(self.file_path):
            raise RuntimeError(f"Failed to open binary input file: {self.file_path}")
        
        self.input_size = os.path.getsize(self.file_path)
        
        # Debug output (commented in C++ version)
        # print(f"Input file path: {self.file_path}")
        # print(f"version: {self.version}")
        # print(f"Actual file size: {self.input_size}")
        # print(f"Expected file size: {self.ie.get_input_size()}")
        
        if self.exec_option.bitmatch:
            self._read_input_file()
        else:
            self._generate_dummy_input()
    
    def get_input_buffer(self) -> np.ndarray:
        """Get the input buffer as a numpy array"""
        return self.input_buffer
    
    def get_version(self) -> int:
        """Get the model version"""
        return self.version
    
    def get_file_path(self) -> str:
        """Get the input file path"""
        return self.file_path
        
    def _get_input_file_path(self) -> str:
        """
        Get input file path based on model path and configuration
        
        Returns:
            Path to the input file
        """
        model_path = self.test_case.ie_option.model_path
        
        # Extract directory path from model_path and append "gt/"
        last_slash = model_path.rfind("/")
        if last_slash == -1:
            last_slash = model_path.rfind("\\")
            
        base_input = "input_0.bin"
        npu_input = "npu_0_input_0.bin"
        cpu_input = "cpu_0_input_0.bin"
        encoder_input = "npu_0_encoder_input_0.bin"
        
        gt_path = ""
        if last_slash != -1:
            gt_path = model_path[:last_slash] + "/gt/"
        else:
            print(f"Error: Directory should be in the format of /path/to/model/model.dxnn: {model_path}")
            exit(-1)
            
        # Test whether it's v6 or v7
        test_v7 = gt_path + encoder_input
        
        if os.path.exists(test_v7):
            self.version = 7
        else:
            self.version = 6
        
        if self.ie.get_compile_type() == "debug":
            result = gt_path + npu_input
            return result
        
        if self.test_case.ie_option.ort:
            result = gt_path + base_input
            return result
        else:
            # Valid for both v6 and v7
            result = gt_path + npu_input
            
            if self.version == 7:
                return test_v7
            else:
                return result
    
    def _read_input_file(self):
        """Read input data from file and convert to numpy array"""
        try:
            with open(self.file_path, 'rb') as file:
                data = file.read()
                # Convert binary data to numpy array of uint8
                self.input_buffer = np.frombuffer(data, dtype=np.uint8)
                
        except IOError as e:
            raise RuntimeError(f"Failed to read file: {self.file_path}") from e
    
    def _generate_dummy_input(self):
        """Generate dummy input data as numpy array"""
        input_size = self.ie.get_input_size()
        
        # Create numpy array with random data (0-255)
        self.input_buffer = np.random.randint(0, 256, size=input_size, dtype=np.uint8)