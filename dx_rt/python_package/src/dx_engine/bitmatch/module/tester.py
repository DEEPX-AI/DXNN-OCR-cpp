#
# Copyright (C) 2018- DEEPX Ltd.
# All rights reserved.
#
# This software is the property of DEEPX and is provided exclusively to customers 
# who are supplied with DEEPX NPU (Neural Processing Unit). 
# Unauthorized sharing or usage is strictly prohibited by law.
#

import os
import glob
import time
import queue
import logging
import warnings
import threading
from collections import defaultdict
from typing import List, Dict, Any, Optional, Tuple, Union
from concurrent.futures import ThreadPoolExecutor
import copy
import re

try:
    from tqdm import tqdm
    TQDM_AVAILABLE = True
except ImportError:
    TQDM_AVAILABLE = False

import numpy as np

from dx_engine import InferenceEngine
from dx_engine import InferenceOption

from .config import TestConfig
from .statistics import TestStatistics
from .debug_analyzer import DebugAnalyzer
from .utils import (int8_to_float32, compare_int8_arrays_fast, get_dxrt_info, move_to_rt_dir, RTOL, ATOL,
                   prepare_multi_input_data_list, convert_multi_input_to_api_format, log_multi_input_info,
                   MAX_DIFF_INDICES_TO_LOG)

class BitMatchTester:
    def __init__(self, config: TestConfig):
        self.config = config
        self.stats = TestStatistics()
        self.threading_lock = threading.Lock()
        self.model_result_messages = []
        self.compile_type = "RELEASE"
        self.model_file_format_version = "7"
        self.performance_mode = config.performance_mode
        self.callback_queue = queue.Queue()
        self.current_input_idx = 0  
        if not self.performance_mode:
            self.result_queue = queue.Queue()
            self.task_queue = queue.Queue()
            self.num_threads = 4
            self.threads = []
            for _ in range(self.num_threads):
                thread = threading.Thread(target=self._worker_loop, daemon=False)
                thread.start()
                self.threads.append(thread)
            self.float_compare_caches = set()

        self._setup_logging()
        self.rt_dir="rt"
        self.is_ppu = False
        
        # Multi-input support
        self.is_multi_input_model = False
        self.multi_input_data_list = []
        self.input_tensor_info = []

    def shutdown(self):
        if hasattr(self, "threads") and self.threads:
            for _ in self.threads:
                self.task_queue.put(None)
            for thread in self.threads:
                thread.join()
            self.threads.clear()

    def __del__(self):
        self.shutdown()

    def _setup_logging(self):
        if self.config.log_enabled:
            np.set_printoptions(linewidth=500)
            log_format = "%(asctime)s - %(levelname)s - %(message)s"
            date_format = "%Y-%m-%d %H:%M:%S"
            logging.basicConfig(
                filename='BITMATCH_RESULTS.log',
                level=logging.INFO,
                format=log_format,
                datefmt=date_format
            )

    def _sort_files_by_number(self, file_list: List[str]) -> List[str]:
        def extract_number(filename: str) -> int:
            basename = os.path.basename(filename)
            match = re.search(r'_(\d+)\.', basename)
            if match:
                return int(match.group(1))
            else:
                return 0
        
        return sorted(file_list, key=extract_number)

    def _get_next_input_idx(self) -> int:
        """Get next input index based on input order configuration.
        
        Returns:
            int: Next input index to use
            
        Raises:
            ValueError: If no input data is available
        """
        # Comprehensive validation
        if not hasattr(self, 'input_data_list') or not self.input_data_list:
            error_msg = "No input data available for index selection"
            self.log(error_msg, 'error')
            raise ValueError(error_msg)
        
        # Validate current_input_idx bounds
        if not hasattr(self, 'current_input_idx'):
            self.current_input_idx = 0
        elif self.current_input_idx >= len(self.input_data_list):
            self.log(f"current_input_idx {self.current_input_idx} exceeds input_data_list length {len(self.input_data_list)}, resetting to 0", 'warning')
            self.current_input_idx = 0
            
        try:
            if self.config.input_order == "sequential":
                idx = self.current_input_idx
                self.current_input_idx = (self.current_input_idx + 1) % len(self.input_data_list)
                return idx
            elif self.config.input_order == "random":
                return np.random.randint(len(self.input_data_list))
            else:
                self.log(f"Unknown input_order: {self.config.input_order}, defaulting to random", 'warning')
                return np.random.randint(len(self.input_data_list))
        except Exception as e:
            self.log(f"Error in _get_next_input_idx: {e}, returning 0", 'error')
            return 0

    def log(self, message: str, level: str = 'info', logskip: bool = False):
        if self.config.verbose or level == 'error':
            print(f"[{level}] {message}")
        if self.config.log_enabled and not logskip:
            getattr(logging, level)(message)

    def bitmatch_logic(self, ie_output: np.ndarray, gt_output: np.ndarray, mask: Optional[np.ndarray] = None) -> Tuple[str, np.ndarray, np.ndarray]:
        # For dynamic shape models, check if sizes differ and handle appropriately
        if ie_output.shape != gt_output.shape:
            # If this is a dynamic shape model, compare only the valid portion
            min_size = min(len(ie_output), len(gt_output))
            if min_size > 0:
                self.log(f"Dynamic shape detected: IE output size {ie_output.shape} vs GT size {gt_output.shape}. Comparing first {min_size} bytes.", 'info')
                ie_output_trimmed = ie_output[:min_size]
                gt_output_trimmed = gt_output[:min_size]
                return self._compare_outputs(ie_output_trimmed, gt_output_trimmed, mask)
            else:
                self.log(f"Size mismatch with zero valid data: IE {ie_output.shape} vs GT {gt_output.shape}", 'error')
                return "FAIL", ie_output, gt_output
        
        return self._compare_outputs(ie_output, gt_output, mask)
    
    def _compare_outputs(self, ie_output: np.ndarray, gt_output: np.ndarray, mask: Optional[np.ndarray] = None) -> Tuple[str, np.ndarray, np.ndarray]:
        if mask is not None and mask.nbytes > 0:
            if mask.shape != ie_output.shape:
                self.log(f"Size mismatch Runtime output: {ie_output.shape} vs mask: {mask.shape}, (gt : {gt_output.shape}), ({ie_output[0].flatten()[0]})")
            ie_output = np.where(mask, ie_output[:mask.shape[0]], 0)
            gt_output = np.where(mask, gt_output[:mask.shape[0]], 0)
        if np.array_equal(ie_output, gt_output):
            return "PASS", ie_output, gt_output

        if len(gt_output) >= 4 and len(gt_output) % 4 == 0 and self.config.use_ort:
            rtol, atol = RTOL, ATOL
            if len(self.float_compare_caches) > 0:
                fail_indices = np.array(list(self.float_compare_caches))
                fail_indices = fail_indices - (fail_indices % 4)  
                fail_indices = fail_indices[fail_indices + 4 <= gt_output.shape[0]]
                fail_indices = np.unique(fail_indices)  
                ie_float_cache = np.array([int8_to_float32(ie_output[i:i+4]) for i in fail_indices])
                gt_float_cache = np.array([int8_to_float32(gt_output[i:i+4]) for i in fail_indices])

                mask_cache_fail = ~np.isclose(ie_float_cache, gt_float_cache, rtol=rtol, atol=atol)
                if np.any(mask_cache_fail):
                    first_fail_cache_index = np.where(mask_cache_fail)[0][0]
                    first_fail_index = sorted(self.float_compare_caches)[first_fail_cache_index]
                    warnings.warn(
                        f"[FLOAT32 NOT CLOSE] gt != rt, first fail at index {first_fail_index}: "
                        f"rt : {ie_float_cache[first_fail_cache_index]}, "
                        f"gt : {gt_float_cache[first_fail_cache_index]}, "
                        f"error(r) : {(gt_float_cache[first_fail_cache_index] - ie_float_cache[first_fail_cache_index]) / gt_float_cache[first_fail_cache_index] * 100}%\n"
                        f"{ie_output[first_fail_index:first_fail_index+4]}, {gt_output[first_fail_index:first_fail_index+4]}"
                    )
                    return "FAIL", ie_output, gt_output
            compare_result, first_fail_index = compare_int8_arrays_fast(ie_output, gt_output, rtol=rtol, atol=atol)
            if compare_result:
                warnings.warn("[FLOAT32 CLOSE] gt ~ rt, float32 values of rt and gt have discrepancies within the acceptable range.")
                return "PASS(CLOSE)", ie_output, gt_output
            else:
                self.float_compare_caches.add(first_fail_index)  
                return "FAIL", ie_output, gt_output
        return "FAIL", ie_output, gt_output

    def _handle_mismatch(self, ie_outputs_masked: np.ndarray, gt_output_masked: np.ndarray, loop_id: int): 
        """Handle bitmatch mismatch with improved memory management.
        
        Args:
            ie_outputs_masked: Inference engine outputs
            gt_output_masked: Ground truth outputs 
            loop_id: Loop identifier
            
        Returns:
            int: 1 for mismatch, 0 for success
        """
        try:
            if gt_output_masked.shape != ie_outputs_masked.shape:
                self.log(f"Size mismatch ie_outputs: {ie_outputs_masked.shape} vs gt_output_masked: {gt_output_masked.shape}")
                self.stats.failed_jobs.append(loop_id)
                return 1
                
            # Limit detailed logging to prevent memory issues with large arrays
            if len(self.stats.failed_jobs) < MAX_DIFF_INDICES_TO_LOG:
                try:
                    different_indices = np.where(ie_outputs_masked != gt_output_masked)[0]
                    
                    # Limit the number of indices to process for memory efficiency
                    max_indices_to_check = min(len(different_indices), MAX_DIFF_INDICES_TO_LOG)
                    first_diff_indices = different_indices[:max_indices_to_check]
                    
                    # Use list comprehension with bounds checking
                    first_diff_values = []
                    for i in first_diff_indices:
                        if i < len(ie_outputs_masked) and i < len(gt_output_masked):
                            first_diff_values.append((ie_outputs_masked[i], gt_output_masked[i]))

                    self.log(f"Indices of differences (first {max_indices_to_check}): {first_diff_indices}")
                    self.log(f"Total number of differences: {len(different_indices)}")
                    self.log(f"Different values (first {len(first_diff_values)}): {first_diff_values}")
                    
                    # Clean up large arrays early
                    del different_indices
                    del first_diff_indices
                    del first_diff_values
                    
                except MemoryError:
                    self.log(f"Memory error during mismatch analysis for loop {loop_id}, skipping detailed analysis", 'warning')
                except Exception as e:
                    self.log(f"Error during mismatch analysis for loop {loop_id}: {e}", 'warning')
                    
            self.stats.failed_jobs.append(loop_id)
            return 1
            
        except Exception as e:
            self.log(f"Unexpected error in _handle_mismatch for loop {loop_id}: {e}", 'error')
            self.stats.failed_jobs.append(loop_id)
            return 1

    def callback_handler_without_bitmatch(self, outputs: List[np.ndarray], user_arg: Any):
        loop_id, input_idx = user_arg
        #self.log(f"loop_id: {loop_id}, input_idx: {input_idx} , Callback called ({hex(id(outputs))} - {hex(outputs[0].ctypes.data)})", logskip=True)
        self.log(f"(Callback) loop_id: {loop_id}, input_idx: {input_idx}", logskip=True)
        self.stats.total_count += 1
        self.callback_queue.get(timeout=1)
        self.callback_queue.task_done()
        return 0

    def callback_handler(self, outputs: List[np.ndarray], user_arg: Any):
        loop_id, input_idx = user_arg
        #self.log(f"loop_id: {loop_id}, input_idx: {input_idx} , Callback called ({hex(id(outputs))} - {hex(outputs[0].ctypes.data)})", logskip=True)
        self.log(f"(Callback) loop_id: {loop_id}, input_idx: {input_idx}", logskip=True)
        self.task_queue.put((copy.deepcopy(outputs), loop_id, input_idx))
        self.callback_queue.get(timeout=1)
        self.callback_queue.task_done()
        return 0
    
    def callback_handler_sync_bitmatch(self, outputs: List[np.ndarray], user_arg: Any):
        loop_id, input_idx = user_arg
        #self.log(f"loop_id: {loop_id}, input_idx: {input_idx} , Callback called ({hex(id(outputs))} - {hex(outputs[0].ctypes.data)})", logskip=True)
        self.log(f"(Callback) loop_id: {loop_id}, input_idx: {input_idx}", logskip=True)
        try:
            self.log(f"(Callback) Processed bitmatch for loop_id: {loop_id}, input_idx: {input_idx}, outputs : {len(outputs)},{outputs[0].shape} ", logskip=True)
            ie_outputs = np.frombuffer(
                b''.join(output.flatten(order='C').tobytes() for output in outputs),
                dtype=np.uint8
            )
            #print(f"{len(ie_outputs)}, {ie_outputs[0].shape}, {ie_outputs[0].dtype}")
            if self.gt_outputs is None:
                self.log(f"GT outputs not available for input_idx: {input_idx}", 'error')
                return 0
            gt_output = self.gt_outputs[input_idx]
            result, ie_outputs_masked, gt_output_masked = self.bitmatch_logic(
                ie_outputs, gt_output["LAST"], self.masks[0]
            )
            self.stats.total_count += 1
            if result == "PASS" or result == "PASS(CLOSE)":
                self.log(f"Processed bitmatch for loop_id: {loop_id}, input_idx: {input_idx} , Result: {result}", logskip=True)
                self.stats.pass_count += 1
            else:
                #with open(f"loop_{loop_id}_output_{input_idx}.bin", "wb") as f:
                #    f.write(ie_outputs_masked.tobytes())
                self.log(f"Processed bitmatch for loop_id: {loop_id}, input_idx: {input_idx} , Result: {result}", level="error", logskip=True)
                self._handle_mismatch(ie_outputs_masked, gt_output_masked, loop_id)
            try:
                self.result_queue.get(timeout=10)
                self.result_queue.task_done()
            except queue.Empty:
                self.log(f"Timeout waiting on result_queue for loop_id: {loop_id}", level="error")

        except Exception as e:
            print(f"[Worker Error] {e}")
        return 0
    
    def _worker_loop(self):
        while True:
            try:
                task = self.task_queue.get()  
                if task is None:
                    break 
                outputs, loop_id, input_idx = task
                #self.log(f"Processed bitmatch for loop_id: {loop_id}, input_idx: {input_idx} ({hex(id(outputs))} - {hex(outputs[0].ctypes.data)})", logskip=True)
                self.log(f"(BM) Processed bitmatch for loop_id: {loop_id}, input_idx: {input_idx}, outputs count : {len(outputs)} - shape : {outputs[0].shape}", logskip=True)
                ie_outputs = np.frombuffer(
                    b''.join(output.flatten(order='C').tobytes() for output in outputs),
                    dtype=np.uint8
                )

                #print(f"{len(ie_outputs)}, {ie_outputs[0].shape}, {ie_outputs[0].dtype}")
                if self.gt_outputs is None:
                    self.log(f"GT outputs not available for input_idx: {input_idx}", 'error')
                    return 0
                gt_output = self.gt_outputs[input_idx]
                result, ie_outputs_masked, gt_output_masked = self.bitmatch_logic(
                    ie_outputs, gt_output["LAST"], self.masks[0]
                )
                self.stats.total_count += 1
                if result == "PASS" or result == "PASS(CLOSE)":
                    self.log(f"Processed bitmatch for loop_id: {loop_id}, input_idx: {input_idx} , Result: {result}", logskip=True)
                    self.stats.pass_count += 1
                else:
                    #with open(f"loop_{loop_id}_output_{input_idx}.bin", "wb") as f:
                    #    f.write(ie_outputs_masked.tobytes())
                    self.log(f"Processed bitmatch for loop_id: {loop_id}, input_idx: {input_idx} , Result: {result}", level="error", logskip=True)
                    self._handle_mismatch(ie_outputs_masked, gt_output_masked, loop_id)
                try:
                    self.result_queue.get(timeout=10)
                    self.result_queue.task_done()
                except queue.Empty:
                    self.log(f"Timeout waiting on result_queue for loop_id: {loop_id}", level="error")

            except Exception as e:
                print(f"[Worker Error] {e}")
            finally:
                self.task_queue.task_done() 
    '''
    def _process_output(self, outputs: List[np.ndarray], user_arg: Any):
        loop_id, input_idx = user_arg
        #self.log(f"Processe bitmatch for loop_id: {loop_id}, input_idx: {input_idx} ({hex(id(outputs))} - {hex(outputs[0].ctypes.data)})", logskip=True)
        self.log(f"Processe bitmatch for loop_id: {loop_id}, input_idx: {input_idx}", logskip=True)

        ie_outputs = np.frombuffer(b''.join(output.tobytes() for output in outputs), dtype=np.uint8)

        gt_output = self.gt_outputs[input_idx]
        result, ie_outputs_masked, gt_output_masked = self.bitmatch_logic(
            ie_outputs, gt_output["LAST"], self.masks[0]
        )
        self.log(f"Processed bitmatch for loop_id: {loop_id}, input_idx: {input_idx} , Result: {result}", logskip=True)
        self.stats.total_count += 1
        if result == "PASS" or result == "PASS(CLOSE)":
            self.stats.pass_count += 1
        else:
            self._handle_mismatch(ie_outputs_masked, gt_output_masked, loop_id)
        try:
            self.result_queue.get(timeout=10)
            self.result_queue.task_done()
        except queue.Empty:
            self.log(f"Timeout waiting on result_queue for loop_id: {loop_id}", level="error")
    '''
    def process_model(self, model_path: str, model_name: str, dxnn: str, gt_dir: str, rt_dir: str, loops: int, compile_type: str) -> bool:
        # Reset all model-specific state variables
        self.stats = TestStatistics()  
        self.rt_dir = rt_dir
        self.current_input_idx = 0 
        
        # Reset model-specific data structures
        self.input_data_list = []
        self.gt_outputs = None
        self.masks = []
        self.input_files = []
        
        # Reset multi-input related state
        self.is_multi_input_model = False
        self.multi_input_data_list = []
        self.input_tensor_info = []
        
        # Reset float comparison cache for new model
        if not self.performance_mode:
            self.float_compare_caches = set()
        
        # Reset PPU state
        self.is_ppu = False
        
        if dxnn is None:
            self.log(f"No .dxnn files found in '{model_path}'", 'error')
            dxrt_data = get_dxrt_info()
            message = (f"{model_path} , {model_name} , NOTFOUND , {0} , "
                    f"{0} , , {0} , "
                    f" {0} , "
                    f" {0} , "
                    f" {0} , "
                    f" {0} , "
                    f" {0} , "
                    f" {0} , "
                    f" {0} , " 
                    f" {0} , "
                    f"{dxrt_data['memory_speed_MHz']:.1f} , "
                    f"{dxrt_data['pcie']} , "
                    f"{dxrt_data['npus'][0]['voltage_mV']:.1f} , "
                    f"{dxrt_data['npus'][0]['clock_MHz']:.1f} , "
                    f"{dxrt_data['npus'][0]['temperature_C']:.1f} , "
                    f"{dxrt_data['npus'][1]['voltage_mV']:.1f} , "
                    f"{dxrt_data['npus'][1]['clock_MHz']:.1f} , "
                    f"{dxrt_data['npus'][1]['temperature_C']:.1f} , "
                    f"{dxrt_data['npus'][2]['voltage_mV']:.1f} , "
                    f"{dxrt_data['npus'][2]['clock_MHz']:.1f} , "
                    f"{dxrt_data['npus'][2]['temperature_C']:.1f}")
            self.log(message)
            self.model_result_messages.append(message)
            return False
        
        io = InferenceOption()
        io.use_ort = self.config.use_ort
        io.bound_option = self.config.npu_bound
        io.devices = self.config.devices
        with InferenceEngine(dxnn, io) as ie:
            if compile_type is None:
                self.compile_type = ie.get_compile_type()
                self.log(f"Parsed COMPILE TYPE : {self.compile_type}", logskip=True)
            else:
                self.compile_type = compile_type
                self.log(f"Given COMPILE TYPE : {self.compile_type}", logskip=True)

            task_order = list(ie.get_task_order())

            self.model_file_format_version = ie.get_model_version()
            self.log(f"Parsed MODEL FILE FORMAT VERSION : {self.model_file_format_version}", logskip=True)
        
            self._load_data(model_path, ie, gt_dir)
            
            # Validate that data loading was successful
            if not self.performance_mode and not self.input_data_list:
                self.log(f"No valid input data loaded for model {model_name}", 'error')
                dxrt_data = get_dxrt_info()
                message = (f"{model_path} , {model_name} , INPUTNOTFOUND , {0} , "
                        f"{0} , {task_order} , {ie.get_num_tail_tasks()} , "
                        f" {0} , "
                        f" {0} , "
                        f" {0} , "
                        f" {0} , "
                        f" {0} , "
                        f" {0} , "
                        f" {0} , " 
                        f" {0} , "
                        f"{dxrt_data['memory_speed_MHz']:.1f} , "
                        f"{dxrt_data['pcie']} , "
                        f"{dxrt_data['npus'][0]['voltage_mV']:.1f} , "
                        f"{dxrt_data['npus'][0]['clock_MHz']:.1f} , "
                        f"{dxrt_data['npus'][0]['temperature_C']:.1f} , "
                        f"{dxrt_data['npus'][1]['voltage_mV']:.1f} , "
                        f"{dxrt_data['npus'][1]['clock_MHz']:.1f} , "
                        f"{dxrt_data['npus'][1]['temperature_C']:.1f} , "
                        f"{dxrt_data['npus'][2]['voltage_mV']:.1f} , "
                        f"{dxrt_data['npus'][2]['clock_MHz']:.1f} , "
                        f"{dxrt_data['npus'][2]['temperature_C']:.1f}")
                self.log(message)
                self.model_result_messages.append(message)
                return False
                
            if not self.performance_mode:
                if self.gt_outputs is None:
                    self.log(f"gt files were not found in {model_path}", 'error')
                    dxrt_data = get_dxrt_info()
                    message = (f"{model_path} , {model_name} , GTNOTFOUND , {0} , "
                            f"{0} , , , "
                            f" {0} , "
                            f" {0} , "
                            f" {0} , "
                            f" {0} , "
                            f" {0} , "
                            f" {0} , "
                            f" {0} , " 
                            f" {0} , "
                            f"{dxrt_data['memory_speed_MHz']:.1f} , "
                            f"{dxrt_data['pcie']} , "
                            f"{dxrt_data['npus'][0]['voltage_mV']:.1f} , "
                            f"{dxrt_data['npus'][0]['clock_MHz']:.1f} , "
                            f"{dxrt_data['npus'][0]['temperature_C']:.1f} , "
                            f"{dxrt_data['npus'][1]['voltage_mV']:.1f} , "
                            f"{dxrt_data['npus'][1]['clock_MHz']:.1f} , "
                            f"{dxrt_data['npus'][1]['temperature_C']:.1f} , "
                            f"{dxrt_data['npus'][2]['voltage_mV']:.1f} , "
                            f"{dxrt_data['npus'][2]['clock_MHz']:.1f} , "
                            f"{dxrt_data['npus'][2]['temperature_C']:.1f}")
                    self.log(message)
                    self.model_result_messages.append(message)
                    return False
                if not ie.is_ppu():
                    self.is_ppu=False
                    if self.compile_type.casefold() == "release" and (ie.get_output_size() < self.gt_outputs[0]["LAST"].size):
                        if self.config.use_ort:
                            self.log(f"The output size is not the same. ({dxnn}) - ie.output_size : {ie.get_output_size()}, gt : {self.gt_outputs[0]['LAST'].size}", 'error')
                            dxrt_data = get_dxrt_info()
                            message = (f"{model_path} , {model_name} , OUTSIZE , {0} , "
                                    f"{0} , {task_order} , {ie.get_num_tail_tasks()} , "
                                    f" {0} , "
                                    f" {0} , "
                                    f" {0} , "
                                    f" {0} , "
                                    f" {0} , "
                                    f" {0} , "
                                    f" {0} , " 
                                    f" {0} , "
                                    f"{dxrt_data['memory_speed_MHz']:.1f} , "
                                    f"{dxrt_data['pcie']} , "
                                    f"{dxrt_data['npus'][0]['voltage_mV']:.1f} , "
                                    f"{dxrt_data['npus'][0]['clock_MHz']:.1f} , "
                                    f"{dxrt_data['npus'][0]['temperature_C']:.1f} , "
                                    f"{dxrt_data['npus'][1]['voltage_mV']:.1f} , "
                                    f"{dxrt_data['npus'][1]['clock_MHz']:.1f} , "
                                    f"{dxrt_data['npus'][1]['temperature_C']:.1f} , "
                                    f"{dxrt_data['npus'][2]['voltage_mV']:.1f} , "
                                    f"{dxrt_data['npus'][2]['clock_MHz']:.1f} , "
                                    f"{dxrt_data['npus'][2]['temperature_C']:.1f}")
                            
                            self.log(message)
                            self.model_result_messages.append(message)
                            return False
                else:
                    self.is_ppu=True

            # Execute based on compile_type first, then debug_mode
            if self.compile_type.casefold() == "debug":
                # DEBUG compile type: ALWAYS use ie.validate_device() 
                # (uses npu_0_input/output, not encoder/decoder)
                
                # If debug_mode is enabled, use temp directory for clean RT file analysis
                if self.config.debug_mode and not self.performance_mode:
                    import tempfile
                    import shutil
                    
                    # Create temporary directory for this test's RT files only
                    temp_rt_dir = tempfile.mkdtemp(prefix="rt_debug_", dir=os.path.dirname(rt_dir))
                    self.log(f"Using temporary RT directory for clean analysis: {temp_rt_dir}", logskip=True)
                    
                    # Temporarily change rt_dir for validate_device
                    original_rt_dir = self.rt_dir
                    self.rt_dir = temp_rt_dir
                    
                    try:
                        self._run_validation_mode(ie, loops)
                        
                        self.log("=" * 80, logskip=True)
                        self.log("Debug mode: Running intermediate analysis for DEBUG compile type...", 'info')
                        self.log("=" * 80, logskip=True)
                        
                        from dx_engine.bitmatch.module.debug_analyzer import DebugAnalyzer
                        
                        # DEBUG compile type only runs NPU tasks
                        # Pass NPU masks for intermediate file analysis (npu_N_output_M.bin)
                        self.log(f"Initializing DebugAnalyzer (GT: {gt_dir}, RT: {temp_rt_dir}, skip_cpu_tasks=True, npu_masks={len(self.npu_masks)})...", logskip=True)
                        analyzer = DebugAnalyzer(gt_dir, temp_rt_dir, verbose=self.config.verbose, skip_cpu_tasks=True, masks=self.npu_masks)
                        
                        self.log(f"Analyzing intermediate outputs for model: {model_name}...", logskip=True)
                        analysis_results = analyzer.analyze_intermediate_outputs(model_name, test_case_idx=0)
                        
                        # Always show summary
                        analyzer.print_summary(analysis_results)
                        
                        # Save detailed report only if requested
                        if self.config.save_debug_report:
                            report_path = os.path.join(rt_dir, f"{model_name}_debug_analysis.json")
                            try:
                                analyzer.save_report(analysis_results, report_path)
                                self.log(f"Debug analysis report saved to: {report_path}", logskip=True)
                            except Exception as save_error:
                                self.log(f"Warning: Could not save debug report: {save_error}", 'warning')
                        
                        # Include debug findings if critical issues found
                        if analysis_results and analysis_results.get('tasks_with_issues'):
                            critical_issues = [issue for issue in analysis_results['tasks_with_issues'] 
                                             if isinstance(issue.get('match_percentage'), (int, float)) and issue['match_percentage'] < 90]
                            if critical_issues:
                                self.log(f"Critical debug findings: {len(critical_issues)} tasks with <90% match", 'error')
                        
                        self.log("=" * 80, logskip=True)
                        self.log("Debug mode: Analysis completed successfully", 'info')
                        self.log("=" * 80, logskip=True)
                        
                    except Exception as e:
                        self.log("=" * 80, logskip=True)
                        self.log(f"Error during debug analysis: {str(e)}", 'error')
                        import traceback
                        self.log(f"Traceback: {traceback.format_exc()}", 'error')
                        self.log("=" * 80, logskip=True)
                    
                    finally:
                        # Copy RT files from temp to original rt_dir for reference
                        os.makedirs(original_rt_dir, exist_ok=True)
                        for file in os.listdir(temp_rt_dir):
                            src = os.path.join(temp_rt_dir, file)
                            dst = os.path.join(original_rt_dir, file)
                            if os.path.isfile(src):
                                shutil.copy2(src, dst)
                                self.log(f"Copied RT file to permanent location: {file}", logskip=True)
                        
                        # Clean up temp directory
                        shutil.rmtree(temp_rt_dir, ignore_errors=True)
                        self.log(f"Cleaned up temporary RT directory", logskip=True)
                        
                        # Restore original rt_dir
                        self.rt_dir = original_rt_dir
                
                else:
                    # No debug_mode: just run validation
                    self._run_validation_mode(ie, loops)
            
            elif self.config.debug_mode and not self.performance_mode:
                # RELEASE compile type + debug_mode: Use ie.run() for intermediate analysis
                import tempfile
                import shutil
                
                # Create temporary directory for clean RT file analysis
                temp_rt_dir = tempfile.mkdtemp(prefix="rt_debug_release_", dir=os.path.dirname(rt_dir))
                self.log(f"Using temporary RT directory for clean analysis: {temp_rt_dir}", logskip=True)
                
                # Temporarily change rt_dir
                original_rt_dir = self.rt_dir
                self.rt_dir = temp_rt_dir
                
                self.log("=" * 80, logskip=True)
                self.log("Debug mode: Running sequential analysis for RELEASE compile type...", 'info')
                self.log("=" * 80, logskip=True)
                
                try:
                    # Run sequential inference with ie.run() and analyze RT files per test case
                    # Environment variable DXRT_DEBUG_DATA=1 will be set inside the function
                    # Analysis is performed inside _run_debug_sequential_analysis() for each test case
                    debug_results = self._run_debug_sequential_analysis(ie, model_name, model_path, gt_dir, temp_rt_dir, loops)
                    
                    self.log("=" * 80, logskip=True)
                    self.log("Debug mode: Analysis completed successfully", 'info')
                    self.log("=" * 80, logskip=True)
                    
                except Exception as e:
                    self.log("=" * 80, logskip=True)
                    self.log(f"Error during debug analysis: {str(e)}", 'error')
                    import traceback
                    self.log(f"Traceback: {traceback.format_exc()}", 'error')
                    self.log("=" * 80, logskip=True)
                
                finally:
                    # Copy RT files from temp to original rt_dir for reference
                    os.makedirs(original_rt_dir, exist_ok=True)
                    for file in os.listdir(temp_rt_dir):
                        src = os.path.join(temp_rt_dir, file)
                        dst = os.path.join(original_rt_dir, file)
                        if os.path.isfile(src):
                            shutil.copy2(src, dst)
                            self.log(f"Copied RT file to permanent location: {file}", logskip=True)
                    
                    # Clean up temp directory
                    shutil.rmtree(temp_rt_dir, ignore_errors=True)
                    self.log(f"Cleaned up temporary RT directory", logskip=True)
                    
                    # Restore original rt_dir
                    self.rt_dir = original_rt_dir
                
            else:
                # NORMAL MODE (RELEASE + no debug_mode): Use regular inference modes
                if self.performance_mode:
                    ie.register_callback(self.callback_handler_without_bitmatch)
                else:
                    if self.config.sync_mode or self.config.batch_mode:
                        pass
                    else:
                        ie.register_callback(self.callback_handler)
                
                # Execute based on mode
                if self.config.batch_mode:
                    self._run_batch_mode(ie, loops)
                elif self.config.sync_mode:
                    self._run_sync_mode(ie, loops)
                else:
                    self._run_async_mode(ie, loops)
            
            # Determine success based on statistics
            success = self.stats.pass_count == self.stats.total_count and self.stats.total_count > 0
            
            # Log debug mode specific information
            if self.config.debug_mode and not self.performance_mode:
                if self.stats.total_count == 0:
                    self.log("Debug mode: No test cases were processed successfully", 'error')
                    success = False
                else:
                    self.log(f"Debug mode: Processed {self.stats.total_count} test cases, {self.stats.pass_count} passed", 'info')
            
            self._log_results(model_path, model_name, task_order, ie.get_num_tail_tasks())
        del io
        return success

    def _load_gt_outputs(self, model_path: str, gt_dir: str) -> Optional[List[Dict[str, np.ndarray]]]:
        grouped_outputs = defaultdict(
            lambda: {'NORMAL': np.array([]), 'PPU': np.array([]), 
                     'ARGMAX': np.array([]), 'LAST': np.array([])}
        )

        self.log("GT file list:", logskip=True)
        if self.compile_type.casefold() == "release" and self.config.use_ort:
            gt_pattern = "output_*.bin"
        elif self.compile_type.casefold() == "release" and not self.config.use_ort:
            if self.model_file_format_version == "7":
                gt_pattern = "npu_*_decoder_output_*.bin"
            elif self.model_file_format_version == "6":
                gt_pattern = "npu_*_output_*.bin"
            else:
                self.log(f"Unknown MODEL FILE FORMAT VERSION : {self.model_file_format_version}", 'error')
                return None
        else:
            # DEBUG mode: only use raw NPU outputs
            gt_pattern = "npu_*_output_*.bin"
        
        try:
            all_gt_files = glob.glob(os.path.join(gt_dir, gt_pattern))
            
            # DEBUG mode: filter out decoder/encoder files
            if self.compile_type.casefold() == "debug":
                gt_files = []
                for f in all_gt_files:
                    filename = os.path.basename(f)
                    # Exclude decoder and encoder files in DEBUG mode
                    if 'decoder' not in filename.lower() and 'encoder' not in filename.lower():
                        gt_files.append(f)
                gt_files = self._sort_files_by_number(gt_files)
                if len(all_gt_files) != len(gt_files):
                    self.log(f"DEBUG mode: Filtered out {len(all_gt_files) - len(gt_files)} decoder/encoder files from GT", logskip=True)
            else:
                gt_files = self._sort_files_by_number(all_gt_files)
        except Exception as e:
            self.log(f"Error finding GT files with pattern {gt_pattern} in {gt_dir}: {e}", 'error')
            return None
            
        if len(gt_files) == 0:
            return None

        for gt_file in gt_files:
            # Use basename instead of full path for filename parsing
            filename = os.path.basename(gt_file)
            try:
                key = int(filename.split("_")[-1].split(".")[0])
            except (ValueError, IndexError) as e:
                self.log(f"Failed to parse key from filename {filename}: {e}", 'warning')
                continue

            try:
                with open(gt_file, "rb") as f:
                    file_content = f.read()
                    if not file_content:
                        self.log(f"Empty GT file: {gt_file}", 'warning')
                        continue
                        
                    data = np.frombuffer(file_content, dtype=np.uint8)
                    if data.size == 0:
                        self.log(f"No data in GT file: {gt_file}", 'warning') 
                        continue
                    
                    # Use basename for pattern matching instead of full path
                    if 'ppu' in filename.lower():
                        grouped_outputs[key]["PPU"] = data
                        self.log(f" - {filename} (PPU) : {data.shape} {data[:10]}", logskip=True)
                    elif 'argmax' in filename.lower():
                        grouped_outputs[key]["ARGMAX"] = data
                        self.log(f" - {filename} (ARGMAX) : {data.shape} {data[:10]}", logskip=True)
                    elif 'pu_' in filename.lower():
                        grouped_outputs[key]["NORMAL"] = data
                        self.log(f" - {filename} (NORMAL) : {data.shape} {data[:10]}", logskip=True)
                        if self.compile_type.casefold() == "debug" and grouped_outputs[key]["LAST"].size == 0:
                            grouped_outputs[key]["LAST"] = data
                            self.log(f" - {filename} (LAST) : {data.shape} {data[:10]}", logskip=True)
                    else:
                        grouped_outputs[key]["LAST"] = data
                        self.log(f" - {filename} (LAST) : {data.shape} {data[:10]}", logskip=True)

                    if self.compile_type.casefold() == "release" and grouped_outputs[key]["LAST"].size == 0:
                        grouped_outputs[key]["LAST"] = data
                        self.log(f" - {filename} (LAST) : {data.shape} {data[:10]}", logskip=True)
            except (IOError, OSError) as e:
                self.log(f"Failed to read GT file {gt_file}: {e}", 'error')
                continue
            except Exception as e:
                self.log(f"Unexpected error processing GT file {gt_file}: {e}", 'error')
                continue
                
        if not grouped_outputs:
            self.log(f"No valid GT data loaded from {gt_dir}", 'warning')
            return None
            
        return [grouped_outputs[key] for key in sorted(grouped_outputs.keys())]

    def _load_data(self, model_path: str, ie: InferenceEngine, gt_dir: str):
        # Check if multi-input model and log info
        self.is_multi_input_model = ie.is_multi_input_model()
        
        # Log multi-input info if verbose mode is enabled
        if self.config.verbose:
            log_multi_input_info(ie, print)
        
        # Determine input file pattern based on compile type and configuration
        if self.compile_type.casefold() == "release" and self.config.use_ort:
            input_pattern = "input_*.bin"
        elif self.compile_type.casefold() == "release" and not self.config.use_ort:
            if self.model_file_format_version == "7":
                input_pattern = "npu_0_encoder_input_*.bin"
            elif self.model_file_format_version == "6":
                input_pattern = "npu_0_input_*.bin"
        else:
            if self.model_file_format_version in ["6", "7"]:
                input_pattern = "npu_0_input_*.bin"
            else:
                self.log(f"Unknown MODEL FILE FORMAT VERSION : {self.model_file_format_version}", 'error')
                return

        if self.compile_type.casefold() == "debug" and self.config.use_ort:
            self.log(f"DEBUG.dxnn bitmatch with USE_ORT is not supported. (Bitmatch only for the NPU Task in DEBUG.dxnn.)", 'warning')

        # Load ground truth outputs (only for non-performance mode)
        if not self.performance_mode:
            self.gt_outputs = self._load_gt_outputs(model_path, gt_dir)
        else:
            self.gt_outputs = None
            self.log(f"Performance mode: Ground truth outputs will not be loaded", logskip=True)

        # Load input files with validation (both performance and non-performance modes)
        self.input_files = self._sort_files_by_number(glob.glob(os.path.join(gt_dir, input_pattern)))
        
        # Validate input files exist
        if not self.input_files:
            self.log(f"No input files found with pattern: {input_pattern} in {gt_dir}", 'error')
            # For performance mode, create dummy data as fallback
            if self.performance_mode:
                self.log(f"Performance mode fallback: Creating dummy input data with size {ie.get_input_size()}", logskip=True)
                self.input_data_list = [[np.zeros(ie.get_input_size(), dtype=np.uint8)]]
            else:
                self.input_data_list = []
                return
        else:
            # Load input data with error handling
            self.input_data_list = []
            for input_file in self.input_files:
                try:
                    with open(input_file, "rb") as f:
                        file_content = f.read()
                        if not file_content:
                            self.log(f"Empty file: {input_file}", 'warning')
                            continue
                            
                        data = np.frombuffer(file_content, dtype=np.uint8)
                        if data.size == 0:
                            self.log(f"No data in file: {input_file}", 'warning')
                            continue
                            
                        self.input_data_list.append([data])
                        mode_str = "Performance mode" if self.performance_mode else "Regular mode"
                        self.log(f"{mode_str}: Loaded {input_file}: {data[:10]}", logskip=True)
                        
                except (IOError, OSError) as e:
                    self.log(f"Failed to load {input_file}: {e}", 'error')
                    continue
                except Exception as e:
                    self.log(f"Unexpected error loading {input_file}: {e}", 'error')
                    continue
            
            # Validate that we have at least one valid input file
            if not self.input_data_list:
                if self.performance_mode:
                    # For performance mode, create dummy data as fallback
                    self.log(f"Performance mode fallback: No valid input files, creating dummy data with size {ie.get_input_size()}", logskip=True)
                    self.input_data_list = [[np.zeros(ie.get_input_size(), dtype=np.uint8)]]
                else:
                    self.log(f"No valid input data loaded from {gt_dir}", 'error')
                    return

        # Prepare multi-input data if needed (unified handling)
        self._prepare_multi_input_data(ie)

        # Set bitmatch masks for general inference result comparison
        # (used in _compare_outputs for ie.run/validate_device results)
        if self.compile_type.casefold() == "release":
            if self.config.use_ort:
                # RELEASE + USE_ORT: no mask needed
                self.masks = [np.array([])]
            else:
                # RELEASE + non-USE_ORT
                if self.model_file_format_version in ["7", "8"]:
                    # Version 7, 8: no mask needed
                    self.masks = [np.array([])]
                elif self.model_file_format_version == "6":
                    # Version 6: use mask for npu_0
                    self.masks = [ie.get_bitmatch_mask(0)]
                else:
                    self.log(f"Unknown MODEL FILE FORMAT VERSION : {self.model_file_format_version}", 'error')
                    return None
        else:
            # DEBUG compile type: always use mask for npu_0
            self.masks = [ie.get_bitmatch_mask(0)]
        
        # Set NPU masks for debug_mode intermediate file analysis
        # (used in DebugAnalyzer for npu_N_output_M.bin comparison)
        # Extract NPU tasks from task order and collect masks for each NPU task
        self.npu_masks = []
        task_order = ie.get_task_order()
        npu_tasks = [task for task in task_order if task.startswith('npu_')]
        
        self.log(f"Found {len(npu_tasks)} NPU task(s) in model: {npu_tasks}", logskip=True)
        
        for npu_idx, npu_task_name in enumerate(npu_tasks):
            try:
                npu_mask = ie.get_bitmatch_mask(npu_idx)
                self.npu_masks.append(npu_mask)
                if self.config.verbose:
                    self.log(f"Loaded bitmatch mask for {npu_task_name} (index {npu_idx}): shape={npu_mask.shape}, size={npu_mask.nbytes} bytes", logskip=True)
            except Exception as e:
                self.log(f"Warning: Failed to get bitmatch mask for {npu_task_name} (index {npu_idx}): {e}", 'warning')
                self.npu_masks.append(np.array([]))  # Empty mask as fallback
    
    
    def _run_async_mode(self, ie: InferenceEngine, loops: int):
        self.log(f"Starting async mode inference ({loops} loop(s))...", logskip=True)
        start_time = time.time()
        for loop in range(loops):
            input_idx = self._get_next_input_idx()
            if self.is_multi_input_model:
                # Multi-input model: use multi-input data
                multi_input_data = self.multi_input_data_list[input_idx]
                input_data_for_api = convert_multi_input_to_api_format(multi_input_data, 'vector')
                self.log(f"Submitting loop {loop}/{loops} (multi-input, input_idx={input_idx})...", logskip=True)
                job_id = ie.run_async(input_data_for_api, user_arg=(loop, input_idx))
                self.log(f"Loop {loop} (multi-input) is requested with input {input_idx} ({self.config.input_order} order)", logskip=True)
            else:
                # Single-input model: use original data
                input_data = self.input_data_list[input_idx]
                self.log(f"Submitting loop {loop}/{loops} (single-input, input_idx={input_idx})...", logskip=True)
                job_id = ie.run_async(input_data, user_arg=(loop, input_idx))
                self.log(f"Loop {loop} (single-input) is requested with input {input_idx} ({self.config.input_order} order)", logskip=True)
                
            self.callback_queue.put(job_id)
            if not self.performance_mode:
                self.result_queue.put(job_id)
        
        self.log(f"All {loops} loop(s) submitted. Waiting for callbacks...", logskip=True)
        self.callback_queue.join()
        self.log(f"All callbacks completed.", logskip=True)
        
        self.stats.duration = (time.time() - start_time)*1000
        if not self.performance_mode:
            self.log(f"Waiting for result queue processing...", logskip=True)
            self.result_queue.join()
            self.log(f"Result queue processing completed.", logskip=True)

        latency_mean = ie.get_latency_mean()
        latency_std = ie.get_latency_std()
        latency_CV = latency_std / latency_mean if latency_mean else 0
        
        inf_time_mean = ie.get_npu_inference_time_mean()
        inf_time_std = ie.get_npu_inference_time_std()
        inf_time_CV = inf_time_std / inf_time_mean if inf_time_mean else 0
        
        self.stats.latency_mean = latency_mean
        self.stats.latency_stddev = latency_std
        self.stats.latency_CV = latency_CV

        self.stats.inf_time_mean = inf_time_mean
        self.stats.inf_time_stddev = inf_time_std
        self.stats.inf_time_CV = inf_time_CV
        
        if int(os.environ.get('DXRT_DEBUG_DATA', '0')) > 0:
            print("Waiting to save data")
            time.sleep(3)
            move_to_rt_dir(self.rt_dir)

    def _run_sync_mode(self, ie: InferenceEngine, loops: int):
        input_idxs = []
        sync_outputs = []

        start_time = time.time()

        for loop in range(loops):
            input_idx = self._get_next_input_idx()
            input_idxs.append(input_idx)
            
            if self.is_multi_input_model:
                # Multi-input model: use multi-input data
                multi_input_data = self.multi_input_data_list[input_idx]
                input_data_for_api = convert_multi_input_to_api_format(multi_input_data, 'vector')
                sync_outputs.append(ie.run(input_data_for_api))
                self.log(f"Sync loop {loop} (multi-input) processed with input {input_idx}", logskip=True)
            else:
                # Single-input model: use original data
                input_array = self.input_data_list[input_idx]
                sync_outputs.append(ie.run(input_array))
                self.log(f"Sync loop {loop} (single-input) processed with input {input_idx}", logskip=True)

        self.stats.duration = (time.time() - start_time)*1000
        self._process_batch_results(sync_outputs, input_idxs)

        latency_mean = ie.get_latency_mean()
        latency_std = ie.get_latency_std()
        latency_CV = latency_std / latency_mean if latency_mean else 0
        
        inf_time_mean = ie.get_npu_inference_time_mean()
        inf_time_std = ie.get_npu_inference_time_std()
        inf_time_CV = inf_time_std / inf_time_mean if inf_time_mean else 0
        
        self.stats.latency_mean = latency_mean
        self.stats.latency_stddev = latency_std
        self.stats.latency_CV = latency_CV

        self.stats.inf_time_mean = inf_time_mean
        self.stats.inf_time_stddev = inf_time_std
        self.stats.inf_time_CV = inf_time_CV
        
        if int(os.environ.get('DXRT_DEBUG_DATA', '0')) > 0:
            print("Waiting to save data")
            time.sleep(3)
            move_to_rt_dir(self.rt_dir)

    def _run_batch_mode(self, ie: InferenceEngine, loops: int):
        input_idxs = []
        batch_inputs = []
        batch_outputs = []
        
        for loop in range(loops):
            input_idx = self._get_next_input_idx()
            input_idxs.append(input_idx)
            
            if self.is_multi_input_model:
                # Multi-input model: prepare multi-input batch data
                multi_input_data = self.multi_input_data_list[input_idx]
                input_data_for_api = convert_multi_input_to_api_format(multi_input_data, 'vector')
                batch_inputs.append(input_data_for_api)
                self.log(f"Batch preparation {loop} (multi-input) with input {input_idx}", logskip=True)
            else:
                # Single-input model: use original data
                input_array = self.input_data_list[input_idx]
                batch_inputs.append(input_array)
                self.log(f"Batch preparation {loop} (single-input) with input {input_idx}", logskip=True)
                
            batch_outputs.append([np.empty(ie.get_output_size(), dtype=np.uint8)])

        start_time = time.time()
        batch_outputs = ie.run(batch_inputs, batch_outputs)
        self.stats.duration = (time.time() - start_time)*1000
        self._process_batch_results(batch_outputs, input_idxs)

        latency_mean = ie.get_latency_mean()
        latency_std = ie.get_latency_std()
        latency_CV = latency_std / latency_mean if latency_mean else 0
        
        inf_time_mean = ie.get_npu_inference_time_mean()
        inf_time_std = ie.get_npu_inference_time_std()
        inf_time_CV = inf_time_std / inf_time_mean if inf_time_mean else 0
        
        self.stats.latency_mean = latency_mean
        self.stats.latency_stddev = latency_std
        self.stats.latency_CV = latency_CV

        self.stats.inf_time_mean = inf_time_mean
        self.stats.inf_time_stddev = inf_time_std
        self.stats.inf_time_CV = inf_time_CV
        
        if int(os.environ.get('DXRT_DEBUG_DATA', '0')) > 0:
            print("Waiting to save data")
            time.sleep(3)
            move_to_rt_dir(self.rt_dir)

    def _run_validation_mode(self, ie: InferenceEngine, loops: int):
        self.log(f"Starting validation mode ({loops} loop(s))...", logskip=True)
        validation_outputs = []
        input_idxs = []
        start_time = time.time()
        
        for loop in range(loops):
            input_idx = self._get_next_input_idx()
            input_idxs.append(input_idx)
            
            if self.is_multi_input_model:
                # Multi-input model: use multi-input validation
                multi_input_data = self.multi_input_data_list[input_idx]
                input_data_for_api = convert_multi_input_to_api_format(multi_input_data, 'vector')
                self.log(f"Running validation {loop}/{loops} (multi-input, input_idx={input_idx})...", logskip=True)
                validation_outputs.append(ie.validate_device(input_data_for_api, 0))
                self.log(f"Validation loop {loop} (multi-input) processed with input {input_idx}", logskip=True)
            else:
                # Single-input model: use original data
                input_data = self.input_data_list[input_idx]
                self.log(f"Running validation {loop}/{loops} (single-input, input_idx={input_idx})...", logskip=True)
                validation_outputs.append(ie.validate_device(input_data, 0))
                self.log(f"Validation loop {loop} (single-input) processed with input {input_idx}", logskip=True)

        self.log(f"All {loops} validation loop(s) completed.", logskip=True)
        self.stats.duration = (time.time() - start_time)*1000
        self._process_batch_results(validation_outputs, input_idxs)

        latency_mean = ie.get_latency_mean()
        latency_std = ie.get_latency_std()
        latency_CV = latency_std / latency_mean if latency_mean else 0
        
        inf_time_mean = ie.get_npu_inference_time_mean()
        inf_time_std = ie.get_npu_inference_time_std()
        inf_time_CV = inf_time_std / inf_time_mean if inf_time_mean else 0
        
        self.stats.latency_mean = latency_mean
        self.stats.latency_stddev = latency_std
        self.stats.latency_CV = latency_CV

        self.stats.inf_time_mean = inf_time_mean
        self.stats.inf_time_stddev = inf_time_std
        self.stats.inf_time_CV = inf_time_CV
        
        if int(os.environ.get('DXRT_DEBUG_DATA', '0')) > 0:
            print("Waiting to save data")
            time.sleep(3)
            move_to_rt_dir(self.rt_dir)
            os.makedirs(self.rt_dir, exist_ok=True)
            for idx, validation_output in zip(input_idxs, validation_outputs):
                save_path = os.path.join(self.rt_dir, f"npu_0_output_{idx}.bin")
                with open(save_path, "wb") as f:
                    f.write(validation_output[0].tobytes())

    def _process_batch_results(self, batch_outputs: Union[List[np.ndarray], List[List[np.ndarray]]], input_idxs: List[int]):
        for idx, outputs in enumerate(batch_outputs):
            input_idx = input_idxs[idx]
            #ie_outputs = np.frombuffer(b''.join(output.tobytes() for output in outputs), dtype=np.uint8)
            ie_outputs = np.frombuffer(
                b''.join(output.flatten(order='C').tobytes() for output in outputs),
                dtype=np.uint8
            )
            if self.gt_outputs is None:
                self.log(f"GT outputs not available for input_idx: {input_idx}", 'error')
                return
            gt_output = self.gt_outputs[input_idx]
            if self.is_ppu and self.compile_type.casefold() == "debug":
            #if gt_output["PPU"].size > 0:
                all_mask = np.concatenate([
                    self.masks[0],
                    np.ones(len(gt_output["PPU"]), dtype=np.uint8),
                    np.zeros((128*1024 - len(gt_output["PPU"])), dtype=np.uint8)
                ])
                all_gt = np.concatenate([
                    gt_output["LAST"],
                    gt_output["PPU"],
                    np.zeros((128*1024 - len(gt_output["PPU"])), dtype=np.uint8)
                ])
                #print(len(ie_outputs),len(all_gt),len(all_mask))
                result, ie_outputs_masked, gt_output_masked = self.bitmatch_logic(
                    ie_outputs, all_gt, all_mask
                )
            else:
                result, ie_outputs_masked, gt_output_masked = self.bitmatch_logic(
                    ie_outputs, 
                    gt_output["LAST"],
                    self.masks[0]
                )

            self.log(f"Loop {idx} , Input ID: {input_idx} , Result: {result}", logskip=True)
            
            self.stats.total_count += 1
            if result == "PASS" or result == "PASS(CLOSE)":
                self.stats.pass_count += 1
            else:
                self._handle_mismatch(ie_outputs_masked, gt_output_masked, idx)

    def _log_results(self, model_path: str, model_name: str, task_order: List[str], num_tails: int):
        result = "PASS" if self.stats.pass_count == self.stats.total_count else "FAIL"
        duration = self.stats.duration
        latency_mean = self.stats.latency_mean
        latency_std = self.stats.latency_stddev
        latecny_cv = self.stats.latency_CV
        inf_time_mean = self.stats.inf_time_mean
        inf_time_std = self.stats.inf_time_stddev
        inf_time_cv = self.stats.inf_time_CV

        dxrt_data = get_dxrt_info()
        #print(dxrt_data)
        task_order_str = " -> ".join(task_order) if task_order else ""

        # Calculate FPS safely (avoid division by zero)
        fps = self.stats.total_count * 1000 / duration if duration > 0 else 0.0
        
        message = (f"{model_path} , {model_name} , {result} , {self.stats.pass_count} , "
                   f"{self.stats.total_count} , {task_order_str} , {num_tails} , "
                   f"{duration:.2f} , "
                   f"{fps:.2f} , "
                   f"{latency_mean:.2f} , "
                   f"{latecny_cv:.2f} , "
                   f"{latency_std:.2f} , "
                   f"{inf_time_mean:.2f} , "
                   f"{inf_time_cv:.2f} , "
                   f"{inf_time_std:.2f} , "
                   f"{dxrt_data['memory_speed_MHz']:.1f} , "
                   f"{dxrt_data['pcie']} , "
                   f"{dxrt_data['npus'][0]['voltage_mV']:.1f} , "
                   f"{dxrt_data['npus'][0]['clock_MHz']:.1f} , "
                   f"{dxrt_data['npus'][0]['temperature_C']:.1f} , "
                   f"{dxrt_data['npus'][1]['voltage_mV']:.1f} , "
                   f"{dxrt_data['npus'][1]['clock_MHz']:.1f} , "
                   f"{dxrt_data['npus'][1]['temperature_C']:.1f} , "
                   f"{dxrt_data['npus'][2]['voltage_mV']:.1f} , "
                   f"{dxrt_data['npus'][2]['clock_MHz']:.1f} , "
                   f"{dxrt_data['npus'][2]['temperature_C']:.1f}")

        self.model_result_messages.append(message)
        self.log(message)
        if TQDM_AVAILABLE:
            tqdm.write(message)
        else:
            print(message)

    def log_all_results(self, test_pass_count: int, test_total_count: int, failed_models: List[Dict[str, Any]]):
        self.config.verbose = True
        self.log("=" * 50)
        self.log(" MODEL PATH , MODEL NAME , RESULT , PASS , TOTAL , TASK ORDER , TAIL , DURATION (ms) , FPS ,"+
                 " LATENCY MEAN (us) , LATENCY CV , LATENCY STD , NPU INF MEAN (us) , NPU INF CV , NPU INF STD ,"+
                 " MEM_CLK_MHz , PCIe , NPU_0_VOL_mV , NPU_0_CLK_MHz , NPU_0_TEMP_C , NPU_1_VOL_mV , NPU_1_CLK_MHz , NPU_1_TEMP_C , NPU_2_VOL_mV , NPU_2_CLK_MHz , NPU_2_TEMP_C")
        self.log("-" * 50)

        model_summary: Dict[str, Dict[str, Any]] = defaultdict(lambda: {
            "model_path": "",
            "model_name": "",
            "result": [],
            "pass_count": 0,
            "total_count": 0,
            "task_order": [],
            "num_tails": None,
            "dur_sum": 0.0,
            "fps_sum": 0.0,
            "latency_mean_sum": 0.0,
            "latency_cv_sum": 0.0,
            "latency_std_sum": 0.0,
            "inf_time_mean_sum": 0.0,
            "inf_time_cv_sum": 0.0,
            "inf_time_std_sum": 0.0,
            "mem_clk": 0.0,
            "pcie":"",
            "n0_vol": 0.0,
            "n0_clk": 0.0,
            "n0_temp_sum": 0.0,
            "n1_vol": 0.0,
            "n1_clk": 0.0,
            "n1_temp_sum": 0.0,
            "n2_vol": 0.0,
            "n2_clk": 0.0,
            "n2_temp_sum": 0.0,
            "count": 0
        })
        for message in self.model_result_messages:
            parts = message.split(",")
            # Handle potential trailing comma and ensure we have exactly 26 parts
            if len(parts) > 26:
                parts = parts[:26]  # Take only first 26 parts
            elif len(parts) < 26:
                # Pad with empty strings if we have fewer than 26 parts
                parts.extend([''] * (26 - len(parts)))
            
            (model_path, model_name, result, pass_count, total_count, task_order_joined_str,
             num_tails, dur, fps, l_mean, l_cv, l_std, i_mean, i_cv, i_std,
             mem_clk, pcie, n0_vol, n0_clk, n0_temp, n1_vol, n1_clk, n1_temp, n2_vol, n2_clk, n2_temp) = parts
            
            # Safe type conversions based on actual data format
            def safe_int(value, default=0):
                try:
                    return int(value.strip()) if value.strip() else default
                except (ValueError, AttributeError):
                    return default
            
            def safe_float(value, default=0.0):
                try:
                    return float(value.strip()) if value.strip() else default
                except (ValueError, AttributeError):
                    return default
            
            def safe_str(value, default=""):
                return value.strip() if value else default
            
            # Parse fields with proper types
            pass_count = safe_int(pass_count)
            total_count = safe_int(total_count)
            
            # Task order is a string that may contain " -> " separator
            task_order_list = task_order_joined_str.split(' -> ') if task_order_joined_str else []
            if task_order_list == ['']: task_order_list = []
            
            # Numeric fields (all float)
            dur = safe_float(dur)
            fps = safe_float(fps)
            l_mean = safe_float(l_mean)
            l_cv = safe_float(l_cv)
            l_std = safe_float(l_std)
            i_mean = safe_float(i_mean)
            i_cv = safe_float(i_cv)
            i_std = safe_float(i_std)
            mem_clk = safe_float(mem_clk)
            
            # String field (PCIe info like "Gen3 X4")
            pcie = safe_str(pcie)
            
            # NPU numeric fields
            n0_vol = safe_float(n0_vol)
            n0_clk = safe_float(n0_clk)
            n0_temp = safe_float(n0_temp)
            n1_vol = safe_float(n1_vol)
            n1_clk = safe_float(n1_clk)
            n1_temp = safe_float(n1_temp)
            n2_vol = safe_float(n2_vol)
            n2_clk = safe_float(n2_clk)
            n2_temp = safe_float(n2_temp)

            model_summary[model_name]["model_path"] = model_path
            model_summary[model_name]["model_name"] = model_name
            model_summary[model_name]["result"].append(result)
            model_summary[model_name]["pass_count"] += pass_count
            model_summary[model_name]["total_count"] += total_count
            model_summary[model_name]["task_order"] = task_order_list
            model_summary[model_name]["num_tails"] = num_tails
            model_summary[model_name]["dur_sum"] += dur
            model_summary[model_name]["fps_sum"] += fps
            model_summary[model_name]["latency_mean_sum"] += l_mean
            model_summary[model_name]["latency_cv_sum"] += l_cv
            model_summary[model_name]["latency_std_sum"] += l_std
            model_summary[model_name]["inf_time_mean_sum"] += i_mean
            model_summary[model_name]["inf_time_cv_sum"] += i_cv
            model_summary[model_name]["inf_time_std_sum"] += i_std
            model_summary[model_name]["mem_clk"] = mem_clk
            model_summary[model_name]["pcie"] = pcie
            model_summary[model_name]["n0_vol"] = n0_vol
            model_summary[model_name]["n0_clk"] = n0_clk
            model_summary[model_name]["n0_temp_sum"] += n0_temp
            model_summary[model_name]["n1_vol"] = n1_vol
            model_summary[model_name]["n1_clk"] = n1_clk
            model_summary[model_name]["n1_temp_sum"] += n1_temp
            model_summary[model_name]["n2_vol"] = n2_vol
            model_summary[model_name]["n2_clk"] = n2_clk
            model_summary[model_name]["n2_temp_sum"] += n2_temp
            model_summary[model_name]["count"] += 1

        for model_name, data in model_summary.items():
            # Define error/exception cases that should be treated as failures
            error_cases = {
                'NOTFOUND', 'INPUTNOTFOUND', 'GTNOTFOUND', 'OUTSIZE', 'COMTYPE'
            }
            
            # Count different result types
            fail_count = 0
            pass_count = 0
            error_count = 0
            error_types = set()  # Track specific error types
            
            for result in data["result"]:
                result_clean = result.strip().upper()
                if result_clean in error_cases:
                    error_count += 1
                    error_types.add(result_clean)
                elif "FAIL" in result_clean:
                    fail_count += 1
                elif "PASS" in result_clean:
                    pass_count += 1
            
            # Determine final result summary
            if error_count > 0:
                # If all results are the same error type, show the specific error
                if error_count == len(data["result"]) and len(error_types) == 1:
                    result_summary = list(error_types)[0]  # Show specific error name
                else:
                    # Mixed error types or mixed with other results, show count
                    result_summary = f"ERROR ({error_count})"
            elif fail_count == 0 and pass_count > 0:
                result_summary = "PASS"
            elif pass_count == 0 and fail_count > 0:
                result_summary = "FAIL"
            elif fail_count > 0 and pass_count > 0:
                result_summary = f"FAIL {fail_count} / PASS {pass_count}"
            else:
                # No valid results found
                result_summary = "UNKNOWN"

            dur_mean = data["dur_sum"] / data["count"]
            fps_mean = data["fps_sum"] / data["count"]
            latency_mean = data["latency_mean_sum"] / data["count"]
            latency_cv = data["latency_cv_sum"] / data["count"]
            latency_std = data["latency_std_sum"] / data["count"]
            inf_time_mean = data["inf_time_mean_sum"] / data["count"]
            inf_time_cv = data["inf_time_cv_sum"] / data["count"]
            inf_time_std = data["inf_time_std_sum"] / data["count"]
            n0_temp_mean = data["n0_temp_sum"] / data["count"]
            n1_temp_mean = data["n1_temp_sum"] / data["count"]
            n2_temp_mean = data["n2_temp_sum"] / data["count"]

            task_order_display_str = " -> ".join(data['task_order']) if data['task_order'] else ""

            message = (f"{data['model_path']} , {model_name} , {result_summary} , {data['pass_count']} , {data['total_count']} , "
                       f"{task_order_display_str} , {data['num_tails']} , {dur_mean:.2f} , {fps_mean:.2f} , "
                       f"{latency_mean:.2f} , {latency_cv:.2f} , {latency_std:.2f} , {inf_time_mean:.2f} , {inf_time_cv:.2f} , {inf_time_std:.2f} , "
                       f"{data['mem_clk']:.1f} , {data['pcie']} , {data['n0_vol']:.1f} , {data['n0_clk']:.1f} , {n0_temp_mean:.1f} , {data['n1_vol']:.1f} , {data['n1_clk']:.1f} , {n1_temp_mean:.1f} , {data['n2_vol']:.1f} , {data['n2_clk']:.1f} , {n2_temp_mean:.1f} ")
            self.log(message)

        self.log("=" * 50)
        self.log(f"Pass Count: {test_pass_count} / Total Count: {test_total_count}")

        if failed_models:
            self.log("Failed Models:")
            
            failed_model_summary: Dict[str, Dict[str, Any]] = defaultdict(lambda: {"fail_count": 0, "total_count": 0, "failed_jobs": []})

            for model in failed_models:
                model_name = model["model"]
                failed_model_summary[model_name]["fail_count"] += model["fail_count"]
                failed_model_summary[model_name]["total_count"] += model["total_count"]
                failed_model_summary[model_name]["failed_jobs"].append(model["failed_jobs"])

            for model_name, data in failed_model_summary.items():
                self.log(f"\n🔹 Model: {model_name}")
                if data["fail_count"] == 0:
                    self.log(f"   Inference ERROR occurred\n")
                else:
                    self.log(f"   Failures: {data['fail_count']} / {data['total_count']}")
                    self.log(f"   Failed Jobs:")
                    for itr, job in enumerate(data["failed_jobs"]):
                        self.log("   " + "-" * 50)
                        self.log(f"   - Test Itr {itr}: {job[:30]}...")
            
            self.log("\n")
        else:
            self.log("All PASS")

    def _run_debug_sequential_analysis(self, ie: InferenceEngine, model_name: str, model_path: str, 
                                      gt_dir: str, rt_dir: str, loops: int) -> Optional[Dict[str, Any]]:
        """Run sequential inference execution with ie.run() to generate RT files via DXRT_DEBUG_DATA"""
        if not self.config.debug_mode:
            return None
        
        self.log(f"Debug mode: Starting sequential inference with ie.run() ({loops} loop(s))...", 'info')
        
        try:
            import time
            import shutil
            
            # Set environment variable for RT file generation
            os.environ['DXRT_DEBUG_DATA'] = '1'
            self.log("Environment: DXRT_DEBUG_DATA=1 (RT files will be generated)", logskip=True)
            
            # Ensure rt_dir exists
            os.makedirs(rt_dir, exist_ok=True)
            self.log(f"RT directory prepared: {rt_dir}", logskip=True)
            
            # Prepare all analysis results
            all_analysis_results = []
            combined_results = {
                "model_name": model_name,
                "total_tasks_analyzed": 0,
                "tasks_with_issues": [],
                "summary": {},
                "test_cases_analyzed": [],
                "detailed_results": []
            }
            
            # Get current working directory to monitor RT file outputs
            cwd = os.getcwd()
            self.log(f"Monitoring RT files in: {cwd}", logskip=True)
            
            # Clean up any existing RT files before starting
            self._cleanup_existing_rt_files(cwd)
            
            # Process each test case sequentially
            for test_case_idx in range(min(loops, len(self.input_data_list))):
                self.log(f"=" * 60, logskip=True)
                self.log(f"Processing test case {test_case_idx}/{min(loops, len(self.input_data_list))}...", logskip=True)
                self.log(f"=" * 60, logskip=True)
                
                # Record timestamp BEFORE inference for file filtering
                inference_start_time = time.time()
                
                # Prepare input for this test case
                if self.is_multi_input_model:
                    multi_input_data = convert_multi_input_to_api_format(self.multi_input_data_list[test_case_idx])
                    input_data = multi_input_data
                    self.log(f"Using multi-input data for test case {test_case_idx}", logskip=True)
                else:
                    input_data = self.input_data_list[test_case_idx]
                    self.log(f"Using single-input data for test case {test_case_idx}", logskip=True)
                
                # Run single inference
                self.log(f"Running inference for test case {test_case_idx}...", logskip=True)
                try:
                    result_outputs = ie.run(input_data)
                    self.log(f"Test case {test_case_idx}: Inference completed successfully", logskip=True)
                    
                    if self.config.verbose:
                        self.log(f"Output: {len(result_outputs)} tensors, shapes={[o.shape for o in result_outputs]}", logskip=True)
                    
                    # Convert outputs to bytes the same way as in callback_handler
                    ie_outputs = np.frombuffer(
                        b''.join(output.flatten(order='C').tobytes() for output in result_outputs),
                        dtype=np.uint8
                    )
                    
                    if self.gt_outputs is not None and test_case_idx < len(self.gt_outputs):
                        gt_output = self.gt_outputs[test_case_idx]
                        
                        # Perform bitmatch
                        result, ie_outputs_masked, gt_output_masked = self.bitmatch_logic(
                            ie_outputs, gt_output["LAST"], self.masks[0]
                        )
                        
                        self.stats.total_count += 1
                        # In debug mode, pass/fail is still determined by bitmatch (model I/O)
                        # Intermediate analysis provides additional diagnostic information
                        if result == "PASS" or result == "PASS(CLOSE)":
                            self.stats.pass_count += 1
                            self.log(f"Test case {test_case_idx}: Bitmatch {result}")
                        else:
                            self.log(f"Test case {test_case_idx}: Bitmatch {result} - analyzing intermediates", 'warning')
                            self._handle_mismatch(ie_outputs_masked, gt_output_masked, test_case_idx)
                    else:
                        # No GT available, count as pass for inference success
                        self.stats.total_count += 1
                        self.stats.pass_count += 1
                        self.log(f"Test case {test_case_idx}: No GT available, assuming pass", 'warning')
                    
                except Exception as inference_error:
                    import traceback
                    self.log(f"Test case {test_case_idx}: Exception occurred: {inference_error}", 'error')
                    self.log(f"Traceback: {traceback.format_exc()}", 'error')
                    self.stats.total_count += 1
                    # Don't increment pass_count for failed inference
                    continue  # Skip to next test case
                
                # Wait a bit for file system to flush RT files
                # Sync execution ensures RT files are written before we proceed
                time.sleep(1)
                
                # Create test-case-specific directory and MOVE RT files immediately
                test_case_rt_dir = os.path.join(rt_dir, f"test_case_{test_case_idx}")
                
                # Clean up existing test case directory
                if os.path.exists(test_case_rt_dir):
                    shutil.rmtree(test_case_rt_dir)
                os.makedirs(test_case_rt_dir, exist_ok=True)
                
                # Find RT files generated by this inference (based on timestamp)
                found_rt_files = self._find_recent_rt_files(cwd, inference_start_time)
                
                # Exception handling: Check if no RT files were found
                if not found_rt_files:
                    self.log(f"Warning: No RT files found after inference for test case {test_case_idx}. "
                           f"DXRT_DEBUG_DATA may not be working or files not yet written.", 'warning')
                    # Give extra time and retry once
                    time.sleep(2)
                    found_rt_files = self._find_recent_rt_files(cwd, inference_start_time)
                    if not found_rt_files:
                        self.log(f"Error: Still no RT files found for test case {test_case_idx}. Skipping analysis.", 'error')
                        continue
                
                # MOVE (not copy) RT files to test case directory to prevent overwriting
                # Sync execution (ie.run/validate_device) ensures no race condition
                moved_files = []
                for rt_file in found_rt_files:
                    # Double-check file still exists (exception: deleted by another process)
                    if not os.path.exists(rt_file):
                        self.log(f"Warning: RT file disappeared before move: {rt_file}", 'warning')
                        continue
                        
                    dest_file = os.path.join(test_case_rt_dir, os.path.basename(rt_file))
                    try:
                        # Exception handling: Prevent partial moves if destination already exists
                        if os.path.exists(dest_file):
                            self.log(f"Warning: Destination file already exists, removing: {dest_file}", 'warning')
                            os.remove(dest_file)
                        
                        shutil.move(rt_file, dest_file)  # MOVE instead of COPY
                        moved_files.append(dest_file)
                        self.log(f"Moved RT file: {os.path.basename(rt_file)} → test_case_{test_case_idx}/", logskip=True)
                    except (OSError, IOError) as e:
                        self.log(f"Warning: Could not move {rt_file}: {e}", 'warning')
                    except Exception as e:
                        self.log(f"Error: Unexpected error moving {rt_file}: {e}", 'error')
                
                # Verify that files were actually moved
                if self.config.verbose:
                    self.log(f"Test case {test_case_idx}: Moved {len(moved_files)}/{len(found_rt_files)} RT files successfully", logskip=True)
                
                # Exception check: Verify no RT files remain in cwd (all should be moved)
                remaining_rt_files = self._find_recent_rt_files(cwd, inference_start_time)
                if remaining_rt_files:
                    self.log(f"Warning: {len(remaining_rt_files)} RT files remain in working directory after move. "
                           f"This may cause issues in next test case.", 'warning')
                    # Force cleanup of remaining files to prevent interference
                    for leftover_file in remaining_rt_files:
                        try:
                            os.remove(leftover_file)
                            self.log(f"Cleaned up leftover RT file: {os.path.basename(leftover_file)}", logskip=True)
                        except Exception as e:
                            self.log(f"Failed to clean up {leftover_file}: {e}", 'warning')
                
                # Run analysis for this test case
                if moved_files:
                    try:
                        from dx_engine.bitmatch.module.debug_analyzer import DebugAnalyzer
                        analyzer = DebugAnalyzer(gt_dir, test_case_rt_dir, verbose=self.config.verbose, skip_cpu_tasks=False, masks=self.npu_masks)
                        test_case_results = analyzer.analyze_intermediate_outputs(f"{model_name} [Test Case {test_case_idx}]", test_case_idx)
                        
                        if test_case_results:
                            # Add test case info to results
                            test_case_results["test_case_index"] = test_case_idx
                            all_analysis_results.append(test_case_results)
                            combined_results["test_cases_analyzed"].append(test_case_idx)
                            
                            # Print summary immediately for this test case
                            print(f"\n{'='*80}")
                            print(f"TEST CASE {test_case_idx} ANALYSIS")
                            print(f"{'='*80}")
                            analyzer.print_summary(test_case_results)
                            
                            # Intermediate analysis provides diagnostic information only
                            # It does NOT affect pass/fail determination (which is based on bitmatch)
                            detailed_results = test_case_results.get('detailed_results', [])
                            task_results = [r for r in detailed_results if not r.get('task_device', '').startswith('__model_')]
                            
                            # Check if there are task-level failures for logging purposes
                            failed_tasks = [r for r in task_results if not r.get('all_files_pass', False)]
                            if failed_tasks and self.config.verbose:
                                self.log(f"Test case {test_case_idx}: {len(failed_tasks)} task(s) failed intermediate analysis", 'warning')
                                for failed_task in failed_tasks:
                                    task_name = failed_task.get('task_device', 'Unknown')
                                    passed = failed_task.get('passed_files', 0)
                                    total = failed_task.get('total_files', 0)
                                    self.log(f"  - {task_name}: {passed}/{total} files passed", 'warning')
                        else:
                            self.log(f"Test case {test_case_idx}: No analysis results", 'warning')
                    except Exception as analysis_error:
                        self.log(f"Test case {test_case_idx}: Analysis failed: {analysis_error}", 'error')
                else:
                    self.log(f"Test case {test_case_idx}: No RT files found", 'warning')
            
            # Combine results from all test cases
            if all_analysis_results:
                # Aggregate statistics
                total_tasks = sum(r["total_tasks_analyzed"] for r in all_analysis_results)
                all_issues = []
                all_detailed = []
                test_cases_with_critical_issues = set()
                
                for result in all_analysis_results:
                    test_case_idx = result["test_case_index"]
                    has_critical_issue = False
                    
                    # Add test case prefix to task issues
                    for issue in result.get("tasks_with_issues", []):
                        issue["test_case"] = test_case_idx
                        all_issues.append(issue)
                        # Check if this is a critical issue (match < 90%)
                        # Exclude 'output' file as it may have format differences between debug and non-debug modes
                        task_name = issue.get('task_name', '')
                        match_pct = issue.get('match_percentage')
                        if task_name != 'output' and isinstance(match_pct, (int, float)) and match_pct < 90:
                            has_critical_issue = True
                    
                    if has_critical_issue:
                        test_cases_with_critical_issues.add(test_case_idx)
                    
                    for detail in result.get("detailed_results", []):
                        detail["test_case"] = test_case_idx
                        all_detailed.append(detail)
                
                # Adjust pass_count based on critical issues
                # If a test case has critical issues, decrement pass_count
                # Note: 'output' file is excluded from critical checks as it contains internal encoded format
                # that differs from user-facing decoded outputs returned by ie.run()
                for test_case_idx in test_cases_with_critical_issues:
                    if self.stats.pass_count > 0:
                        self.stats.pass_count -= 1
                        self.log(f"Test case {test_case_idx}: Adjusted pass_count due to critical issues", 'warning')
                
                combined_results.update({
                    "total_tasks_analyzed": total_tasks,
                    "tasks_with_issues": all_issues,
                    "detailed_results": all_detailed,
                    "summary": {
                        "total_test_cases": len(all_analysis_results),
                        "total_tasks": total_tasks,
                        "tasks_with_differences": len(all_issues),
                        "success_rate": ((total_tasks - len(all_issues)) / total_tasks * 100) if total_tasks > 0 else 0.0
                    }
                })
                
                # Print overall summary
                print(f"\n{'='*80}")
                print(f"OVERALL SUMMARY - All Test Cases")
                print(f"{'='*80}")
                print(f"Total test cases analyzed: {len(all_analysis_results)}")
                print(f"Total tasks analyzed: {total_tasks}")
                print(f"Tasks with differences: {len(all_issues)}")
                if total_tasks > 0:
                    success_rate = (total_tasks - len(all_issues)) / total_tasks * 100
                    print(f"Task success rate: {success_rate:.1f}%")
                print(f"{'='*80}\n")
                
                self.log(f"Debug analysis completed: {len(all_analysis_results)} test cases, "
                       f"{total_tasks} tasks, {len(all_issues)} with differences", logskip=True)
                
                # Save detailed report only if requested
                if self.config.save_debug_report:
                    try:
                        from dx_engine.bitmatch.module.debug_analyzer import DebugAnalyzer
                        analyzer = DebugAnalyzer(gt_dir, rt_dir, verbose=False, skip_cpu_tasks=False, masks=self.npu_masks)
                        debug_report_path = os.path.join(model_path, f"{model_name}_debug_analysis.json")
                        analyzer.save_report(combined_results, debug_report_path)
                        self.log(f"Debug analysis report saved to: {debug_report_path}", logskip=True)
                    except Exception as save_error:
                        self.log(f"Failed to save debug report: {save_error}", 'warning')
                
                return combined_results
            else:
                self.log("Sequential debug analysis: No valid results from any test case", 'warning')
                # Ensure failure is recorded in statistics
                if self.stats.total_count == 0:
                    self.stats.total_count = 1  # At least record one attempt
                self.stats.pass_count = 0  # Ensure failure
                return None
                
        except Exception as e:
            self.log(f"Error during sequential debug analysis: {str(e)}", 'error')
            return None
    
    def _run_debug_analysis(self, model_name: str, model_path: str, gt_dir: str) -> Optional[Dict[str, Any]]:
        """Run advanced debug analysis on intermediate outputs (legacy method)"""
        if not self.config.debug_mode:
            return None
        
        self.log("Starting advanced debug analysis for intermediate outputs...", logskip=True)
        
        try:
            # Initialize debug analyzer
            analyzer = DebugAnalyzer(gt_dir, self.rt_dir, verbose=self.config.verbose)
            
            # Perform analysis
            analysis_results = analyzer.analyze_intermediate_outputs(model_name)
            
            # Always show summary
            analyzer.print_summary(analysis_results)
            
            # Save detailed report only if requested
            if self.config.save_debug_report:
                report_path = os.path.join(self.rt_dir, f"debug_analysis_{model_name}.json")
                analyzer.save_report(analysis_results, report_path)
                self.log(f"Debug analysis report saved to: {report_path}", logskip=True)
            
            # Log critical findings
            if analysis_results.get('tasks_with_issues'):
                self.log(f"Debug Analysis: Found {len(analysis_results['tasks_with_issues'])} tasks with differences", 'warning')
                
                # Group issues by type
                issue_types = {}
                for task in analysis_results['tasks_with_issues']:
                    issue_type = task['issue_type']
                    if issue_type not in issue_types:
                        issue_types[issue_type] = []
                    issue_types[issue_type].append(task['task_name'])
                
                for issue_type, tasks in issue_types.items():
                    self.log(f"  {issue_type}: {len(tasks)} tasks", 'warning')
                    if self.config.verbose:
                        for task in tasks[:3]:  # Show first 3 examples
                            self.log(f"    - {task}", 'warning')
            else:
                self.log("Debug Analysis: All intermediate tasks match perfectly!", 'info')
            
            return analysis_results
            
        except Exception as e:
            self.log(f"Error during debug analysis: {str(e)}", 'error')
            return None

    def _cleanup_existing_rt_files(self, directory: str):
        """Clean up existing RT files that might interfere with debug analysis"""
        try:
            rt_patterns = [
                "npu_*_*put*.bin",
                "cpu_*.*put*.bin", 
                "input*.bin",
                "output*.bin"
            ]
            
            cleaned_count = 0
            for pattern in rt_patterns:
                files = glob.glob(os.path.join(directory, pattern))
                for file_path in files:
                    # Skip files in test_case directories and GT directories
                    if 'test_case' in file_path or '/gt/' in file_path:
                        continue
                    try:
                        os.remove(file_path)
                        cleaned_count += 1
                        self.log(f"Cleaned up existing RT file: {os.path.basename(file_path)}", logskip=True)
                    except Exception as e:
                        self.log(f"Warning: Could not remove {file_path}: {e}", 'warning')
            
            if cleaned_count > 0:
                self.log(f"Cleaned up {cleaned_count} existing RT files", logskip=True)
        except Exception as e:
            self.log(f"Warning: RT file cleanup failed: {e}", 'warning')
    
    def _find_recent_rt_files(self, directory: str, start_time: float, time_tolerance: float = 5.0) -> List[str]:
        """Find RT files created after the given start time"""
        rt_patterns = [
            "npu_*.bin",
            "cpu_*.bin",
            "input*.bin", 
            "output*.bin"
        ]
        
        recent_files = []
        
        for pattern in rt_patterns:
            files = glob.glob(os.path.join(directory, pattern))
            for file_path in files:
                try:
                    # Skip files in subdirectories (test_case, gt, etc.)
                    if os.path.dirname(file_path) != directory:
                        continue
                    
                    # Check if file was created/modified after inference start
                    file_mtime = os.path.getmtime(file_path)
                    if file_mtime >= (start_time - 1.0):  # 1 second tolerance for clock differences
                        # Verify it's actually an RT output file
                        basename = os.path.basename(file_path)
                        if any(keyword in basename.lower() for keyword in ['npu', 'cpu', 'input', 'output']):
                            recent_files.append(file_path)
                            self.log(f"Found recent RT file: {basename} (modified: {file_mtime:.2f}, start: {start_time:.2f})", logskip=True)
                except Exception as e:
                    self.log(f"Warning: Could not check timestamp for {file_path}: {e}", 'warning')
        
        return recent_files

    def _prepare_multi_input_data(self, ie: InferenceEngine):
        """
        Prepare multi-input data for both performance and regular modes.
        This method unifies the multi-input data preparation logic.
        """
        if self.is_multi_input_model:
            try:
                self.input_tensor_info = ie.get_input_tensors_info()
                self.multi_input_data_list = prepare_multi_input_data_list(
                    self.input_data_list, ie, performance_mode=self.performance_mode)
                
                # Determine if using real files or dummy data
                using_dummy_data = (len(self.input_data_list) == 1 and 
                                  len(self.input_data_list[0]) == 1 and 
                                  np.all(self.input_data_list[0][0] == 0))
                
                if self.performance_mode:
                    data_type = "dummy data" if using_dummy_data else "real input files"
                    self.log(f"Performance mode: Prepared multi-input data for {len(self.multi_input_data_list)} inputs, {len(self.multi_input_data_list[0])} tensors (using {data_type})", logskip=True)
                else:
                    self.log(f"Regular mode: Prepared multi-input data for {len(self.multi_input_data_list)} inputs", logskip=True)
                    
            except Exception as e:
                self.log(f"Failed to prepare multi-input data: {e}", 'error')
                # Fallback to treating as single input
                self.is_multi_input_model = False
                self.multi_input_data_list = []
        else:
            # Single input model - no additional preparation needed
            self.multi_input_data_list = []
