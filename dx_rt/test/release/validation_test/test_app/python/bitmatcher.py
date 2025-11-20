"""
BitMatcher class for comparing inference outputs with ground truth
Ported from C++ version
"""

import os
import numpy as np
from typing import List, Optional, Any
from dataclasses import dataclass

@dataclass
class BitMatchResult:
    """Result of bit matching operation"""
    model_run: bool
    bit_match_run: bool
    is_fail: bool
    fail_count: int

class BitMatcher:
    """Compares inference outputs with ground truth data"""
    
    BYTE_BIT_COUNT = 8
    
    def __init__(self, input_path: str, version: int, ort: bool, output_size: int, mask: List[int]):
        self.input_path = input_path
        self.version = version
        self.ort = ort
        self.output_size = output_size
        self.mask = mask
        
        # State variables
        self.outputs = None
        self.batch_size = 0
        self.gt = bytearray()  # Ground truth buffer
        self.fail_count = 0
        self.is_run = False
        self.is_fail = False
        self.is_output_set = False
        self.is_gt_loaded = False

    def __del__(self):
        """Cleanup resources"""
        # Clear GT buffer
        if self.gt:
            self.gt.clear()
        
        # Reset output pointer reference
        self.outputs = None
        
        # Clear input path
        self.input_path = ""

    def bit_match(self):
        """Perform bit matching between output and ground truth"""
        # v6 ORT OFF 일때만 Mask가 필요하고, V7에 대해서는 Mask가 필요하지 않음.
        # 현재 생산 라인에서 검증하는 것은, V6 ORT OFF를 사용하고 있음.
        if not self.ort:
            self.fail_count = -1
            return

        if self.is_output_set and self.is_gt_loaded and self.outputs:
            for i in range(self.batch_size):
                # In Python, outputs[i] is directly a numpy array
                if self.outputs[i] is None:
                    print("Wrong output detected while bitmatching")
                    raise Exception("Empty output tensor")
                
                result = self._bit_match_single(self.outputs[i])
                if result > 0:
                    self.fail_count += result
            
            self.is_run = True
        else:
            raise Exception("Output tensors or GT buffer not set for BitMatcher")

        if self.fail_count > 0:
            self.is_fail = True

    def set_output(self, outputs):
        """Set output tensors for comparison"""
        self.outputs = outputs
        if hasattr(outputs, '__len__'):
            self.batch_size = len(outputs)
        else:
            self.batch_size = 1
        self.is_output_set = True

    def load_gt_buffer(self):
        """Load ground truth buffer from file"""
        gt_path = self._get_gt_file_path()
        
        try:
            with open(gt_path, 'rb') as f:
                self.gt = bytearray(f.read())
            
            if len(self.gt) != self.output_size:
                self.output_size = len(self.gt)  # Update output size to match GT file
            
            self.is_gt_loaded = True
            
        except FileNotFoundError:
            raise FileNotFoundError(f"Failed to open binary input file: {gt_path}")
        except Exception as e:
            raise Exception(f"Failed to read file: {gt_path}, error: {e}")

    def get_result(self) -> BitMatchResult:
        """Get bit matching result"""
        return BitMatchResult(
            model_run=True,
            bit_match_run=self.is_run,
            is_fail=self.is_fail,
            fail_count=self.fail_count
        )

    def _bit_match_single(self, output_data) -> int:
        """Compare single output with ground truth"""
        infer_fail_count = 0
        
        # Convert numpy array to bytes for comparison
        if hasattr(output_data, 'tobytes'):
            byte_output = output_data.tobytes()
        elif hasattr(output_data, 'data'):
            # Handle numpy array or similar
            byte_output = bytes(output_data.data)
        else:
            # Fallback - try to convert to bytes directly
            byte_output = bytes(output_data)
        
        if self.output_size < (len(self.mask) * self.BYTE_BIT_COUNT):
            print(f"Fail to compare buffer by mask. buffer-size={self.output_size} "
                  f"mask-count={len(self.mask) * self.BYTE_BIT_COUNT}")
            return 1

        if len(self.mask) > 0:
            # Use mask for comparison (following C++ logic)
            for i, mask_byte in enumerate(self.mask):
                index_i = i * self.BYTE_BIT_COUNT
                
                if mask_byte != 0xFF:
                    # Not all bits are masked - check individual bits
                    mm = 128  # 1000 0000
                    for j in range(self.BYTE_BIT_COUNT):
                        index = index_i + j
                        
                        if index >= len(byte_output) or index >= len(self.gt):
                            break
                            
                        if (mask_byte & (mm >> 1)) > 0:
                            if byte_output[index] != self.gt[index]:
                                infer_fail_count += 1
                                break
                else:
                    # All bits are masked (0xFF) - compare entire byte block
                    if (index_i + self.BYTE_BIT_COUNT <= len(byte_output) and 
                        index_i + self.BYTE_BIT_COUNT <= len(self.gt)):
                        
                        if byte_output[index_i:index_i + self.BYTE_BIT_COUNT] != self.gt[index_i:index_i + self.BYTE_BIT_COUNT]:
                            infer_fail_count += 1
                            break
        else:
            # Direct byte comparison - no mask
            if byte_output != self.gt:
                infer_fail_count = 1

        return infer_fail_count

    def _get_gt_file_path(self) -> str:
        """Get ground truth file path based on input path"""
        # inputPath에서 디렉토리 부분과 파일명 분리
        try:
            last_slash = self.input_path.rfind('/')
            if last_slash == -1:
                raise ValueError(f"Invalid input path format: {self.input_path}")

            gt_dir = self.input_path[:last_slash]  # /path/to/model_dir/gt
            input_file_name = self.input_path[last_slash + 1:]  # input_name.bin
            
            # input_name에서 npu_0가 있는지 확인
            has_npu0 = "npu_0" in input_file_name

            if has_npu0:
                # npu_0가 있는 경우: ort가 false여야 함
                if self.ort:
                    raise ValueError("Input file contains 'npu_0' but ort flag is true. Expected ort=false for npu_0 files.")
                
                # Based on version, set the output file name
                if self.version == 7:
                    output_file_name = "npu_0_decoder_output_0.bin"
                else:
                    output_file_name = "npu_0_output_0.bin"  # hardcoded one, should be modified
            else:
                # npu_0가 없는 경우: ort가 true여야 함
                if not self.ort:
                    raise ValueError("Input file does not contain 'npu_0' but ort flag is false. Expected ort=true for non-npu_0 files.")
                
                output_file_name = "output_0.bin"
            
            # GT 파일 경로 구성
            gt_file_path = os.path.join(gt_dir, output_file_name)
            
            # 파일 존재 여부 확인
            if not os.path.isfile(gt_file_path):
                raise FileNotFoundError(f"GT file does not exist: {gt_file_path}")

            return gt_file_path
            
        except Exception as e:
            raise Exception(f"Error generating GT file path: {e}")