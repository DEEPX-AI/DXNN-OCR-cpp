"""
Executor classes for different inference execution modes
Ported from C++ version
"""

import time
import threading
from abc import ABC, abstractmethod
from typing import Optional, List, Any, Dict
from dataclasses import dataclass
import numpy as np
import sys

from generator import TestCase, ExecutionOption
from bitmatcher import BitMatcher, BitMatchResult
from concurrent_queue import ConcurrentQueue

# DXRT Python bindings
from dx_engine import InferenceEngine

@dataclass
class ThreadSyncData:
    """Per-thread synchronization data"""
    run_count: int = 0
    callback_count: int = 0
    thread_id: int = -1
    mutex: threading.Lock = threading.Lock()
    condition: threading.Condition = None
    
    def __post_init__(self):
        if self.condition is None:
            self.condition = threading.Condition(self.mutex)

class BaseExecutor(ABC):
    """Base class for all executor types"""
    
    def __init__(self, ie: InferenceEngine, test_case: TestCase, exec_option: ExecutionOption, 
                 input_buffer, version: int, input_path: str):
        self.ie = ie
        self.test_case = test_case
        self.exec_option = exec_option
        self.input_buffer = input_buffer
        self.version = version
        self.input_path = input_path
        
        self._validate_options()
        
        if self.exec_option.time > 0:
            self.time = self.exec_option.time
            self.loop = 0
        else:
            self.time = 0
            self.loop = self.exec_option.loop
        
        self.count = 0
        
        # Initialize _outputPtr (will be set by derived classes)
        self.output_ptr = None
        
        self._should_bit_match = exec_option.bitmatch
        
        if ie.get_compile_type() == "debug" or not self.test_case.ie_option.ort:
            self._should_bit_match = False
        
        # Prepare bitmatch mask for v6 models
        self.mask = []
        if self.version == 6 and self._should_bit_match:
            self.mask = ie.get_bitmatch_mask(0)
        
        self.bm = BitMatcher(self.input_path, self.version, 
                           self.test_case.ie_option.ort, ie.get_output_size(), self.mask)
        
        if self._should_bit_match:
            self.bm.load_gt_buffer()

    def __del__(self):
        """Cleanup resources"""
        if hasattr(self, 'bm') and self.bm:
            del self.bm
        if hasattr(self, 'mask'):
            self.mask.clear()

    def execute(self) -> BitMatchResult:
        """Template method - handles common validation logic"""
        # Skip model run if not valid
        if not self.is_valid:
            return BitMatchResult(model_run=False, bit_match_run=False, is_fail=False, fail_count=0)
        
        self.do_execute()
        return self.bm.get_result()

    @abstractmethod
    def do_execute(self):
        """Pure virtual method for actual execution logic"""
        pass

    def set_output(self, outputs):
        """Wrapping function for Shared IE Bitmatch"""
        self.bm.set_output(outputs)

    def bit_match(self):
        """Wrapping function for Shared IE Bitmatch"""
        self.bm.bit_match()

    def _run_inference(self):
        """Helper method for input style-aware inference"""
        if self.exec_option.input_style == "multi-map":
            # Multi-map input style implementation - Dictionary format
            input_names = self.ie.get_input_tensor_names()
            input_sizes = self.ie.get_input_tensor_sizes()
            
            input_map = {}
            offset = 0
            buffer_ptr = self.input_buffer
            
            for i, name in enumerate(input_names):
                # Create view of input buffer for each tensor
                size = input_sizes[i]
                if isinstance(buffer_ptr, np.ndarray):
                    input_map[name] = buffer_ptr[offset:offset+size]
                else:
                    # Handle byte buffer case
                    input_map[name] = np.frombuffer(buffer_ptr, dtype=np.uint8, count=size, offset=offset)
                offset += size
            
            if self.exec_option.output_buffer == "user":
                return self.ie.run_multi_input(input_map, output_buffers=self.output_ptr)
            else:
                return self.ie.run_multi_input(input_map)
            
        elif self.exec_option.input_style == "multi-vec":
            # Multi-vector input style implementation - List format
            input_sizes = self.ie.get_input_tensor_sizes()
            
            input_vector = []
            offset = 0
            buffer_ptr = self.input_buffer
            
            for size in input_sizes:
                if isinstance(buffer_ptr, np.ndarray):
                    input_vector.append(buffer_ptr[offset:offset+size])
                else:
                    # Handle byte buffer case
                    input_vector.append(np.frombuffer(buffer_ptr, dtype=np.uint8, count=size, offset=offset))
                offset += size
            
            if self.exec_option.output_buffer == "user":
                return self.ie.run(input_vector, output_buffers=self.output_ptr)
            else:
                return self.ie.run(input_vector)
        else:
            # Single input style implementation ("auto-split") - Direct buffer
            if isinstance(self.input_buffer, list):
                input_data = self.input_buffer
            else:
                # Pass buffer directly to engine
                if isinstance(self.input_buffer, np.ndarray):
                    input_data = self.input_buffer
                else:
                    # Convert byte buffer to numpy array
                    input_data = np.frombuffer(self.input_buffer, dtype=np.uint8)
            
            if self.exec_option.output_buffer == "user":
                return self.ie.run(input_data, output_buffers=self.output_ptr)
            else:
                return self.ie.run(input_data)

    def _run_inference_async(self, user_arg=None):
        """Helper method for async inference"""
        if self.exec_option.input_style == "multi-map":
            # Multi-map async implementation - Use run_async_multi_input with dictionary
            input_names = self.ie.get_input_tensor_names()
            input_sizes = self.ie.get_input_tensor_sizes()
            
            input_map = {}
            offset = 0
            buffer_ptr = self.input_buffer
            
            for i, name in enumerate(input_names):
                # Create view of input buffer for each tensor
                size = input_sizes[i]
                if isinstance(buffer_ptr, np.ndarray):
                    input_map[name] = buffer_ptr[offset:offset+size]
                else:
                    # Handle byte buffer case
                    input_map[name] = np.frombuffer(buffer_ptr, dtype=np.uint8, count=size, offset=offset)
                offset += size
            
            if self.exec_option.output_buffer == "user":
                return self.ie.run_async_multi_input(input_map, user_arg=user_arg, output_buffer=self.output_ptr)
            else:
                return self.ie.run_async_multi_input(input_map, user_arg=user_arg)
                
        elif self.exec_option.input_style == "multi-vec":
            # Multi-vector async implementation - Use run_async with list
            input_sizes = self.ie.get_input_tensor_sizes()
            
            input_vector = []
            offset = 0
            buffer_ptr = self.input_buffer
            
            for size in input_sizes:
                if isinstance(buffer_ptr, np.ndarray):
                    input_vector.append(buffer_ptr[offset:offset+size])
                else:
                    # Handle byte buffer case
                    input_vector.append(np.frombuffer(buffer_ptr, dtype=np.uint8, count=size, offset=offset))
                offset += size
            
            if self.exec_option.output_buffer == "user":
                return self.ie.run_async(input_vector, user_arg=user_arg, output_buffer=self.output_ptr)
            else:
                return self.ie.run_async(input_vector, user_arg=user_arg)
        else:
            # Single async implementation ("auto-split") - Direct buffer
            if isinstance(self.input_buffer, list):
                input_data = self.input_buffer
            else:
                # Pass buffer directly to engine
                if isinstance(self.input_buffer, np.ndarray):
                    input_data = self.input_buffer
                else:
                    # Convert byte buffer to numpy array
                    input_data = np.frombuffer(self.input_buffer, dtype=np.uint8)
            
            if self.exec_option.output_buffer == "user":
                return self.ie.run_async(input_data, user_arg=user_arg, output_buffer=self.output_ptr)
            else:
                return self.ie.run_async(input_data, user_arg=user_arg)

    def _validate_options(self):
        """Validate execution options"""
        self.is_valid = True
        
        if self.exec_option.inference_function == "batch":
            if self.exec_option.output_buffer == "internal":
                self.is_valid = False
                return
            else:
                if self.ie.is_multi_input_model():
                    if self.exec_option.input_style != "multi-autosplit":
                        self.is_valid = False
                        return
                else:
                    if self.exec_option.input_style != "single":
                        self.is_valid = False
                        return

        # single input model
        if not self.ie.is_multi_input_model():
            if "multi" in self.exec_option.input_style:
                self.is_valid = False
                return
            else:
                self.input_count = 1

        # multi input model
        else:
            if "single" in self.exec_option.input_style:
                self.is_valid = False
                return
            else:
                self.input_count = self.ie.get_input_tensor_count()

class SyncExecutor(BaseExecutor):
    """Synchronous executor implementation"""
    
    def __init__(self, ie, test_case: TestCase, exec_option: ExecutionOption, 
                 input_buffer: Any, version: int, input_path: str):
        super().__init__(ie, test_case, exec_option, input_buffer, version, input_path)
        
        if self.exec_option.output_buffer == "user":
            self.output_ptr = np.zeros(self.ie.get_output_size(), dtype=np.uint8)
        else:
            self.output_ptr = None
        
        self.ie.register_callback(None)

    def __del__(self):
        """Cleanup resources"""
        # Free output buffer if allocated (handled by GC in Python)
        self.output_ptr = None
        super().__del__()

    def do_execute(self):
        """Synchronous execution implementation"""
        if self.time > 0:
            # Time-based execution
            start_time = time.time()
            TIME_CHECK_INTERVAL = 100
            iter_count = 0
            
            while True:
                outputs = self._run_inference()
                
                if self._should_bit_match:
                    self.bm.set_output(outputs)
                    self.bm.bit_match()
                
                if iter_count % TIME_CHECK_INTERVAL == 0:
                    current_time = time.time()
                    duration_sec = int(current_time - start_time)
                    if duration_sec >= self.time:
                        break
                
                self.count += 1
                iter_count += 1
        else:
            # Loop-based execution
            for i in range(self.loop):
                outputs = self._run_inference()
                
                if self._should_bit_match:
                    self.bm.set_output(outputs)
                    self.bm.bit_match()
                    
                self.count += 1

class AsyncCallbackExecutor(BaseExecutor):
    """Asynchronous callback executor implementation"""
    
    def __init__(self, ie, test_case: TestCase, exec_option: ExecutionOption, 
                 input_buffer: Any, version: int, input_path: str, sync_data: Optional[ThreadSyncData] = None):
        super().__init__(ie, test_case, exec_option, input_buffer, version, input_path)
        self.my_sync_data = sync_data
        
        if self.exec_option.output_buffer == "user":
            self.output_ptr = np.zeros(self.ie.get_output_size(), dtype=np.uint8)
        else:
            self.output_ptr = None
        
        # For multi-ie mode (when sync_data is None)
        self.individual_run_count = 0
        self.individual_callback_count = 0
        self.individual_cb_mutex = threading.Lock()
        self.individual_cb_cv = threading.Condition(self.individual_cb_mutex)
        
        # If my_sync_data is None, this is a multi-IE scenario, so register a per-thread callback
        if self.my_sync_data is None:
            def individual_callback(outputs, user_arg):
                # Suppress unused parameter warning - user_arg not used
                if self._should_bit_match:
                    self.bm.set_output(outputs)
                    self.bm.bit_match()
                
                with self.individual_cb_mutex:
                    self.individual_callback_count += 1
                    if self.individual_run_count == self.individual_callback_count:
                        self.individual_cb_cv.notify()
                return 0
            
            self.ie.register_callback(individual_callback)

    def __del__(self):
        """Cleanup resources"""
        # Free output buffer if allocated (handled by GC in Python)
        self.output_ptr = None
        # Reset callback counters for safety
        self.individual_callback_count = 0
        self.individual_run_count = 0
        super().__del__()

    def do_execute(self):
        """Asynchronous callback execution implementation"""
        if self.my_sync_data:
            # Shared IE mode with synchronization
            self._execute_shared_ie_mode()
        else:
            # Multi IE mode without synchronization
            self._execute_multi_ie_mode()

    def _execute_shared_ie_mode(self):
        """Execute in shared IE mode with thread synchronization"""
        if self.time > 0:
            # Time-based execution
            start_time = time.time()
            TIME_CHECK_INTERVAL = 100
            iter_count = 0
            
            while True:
                inference_id = self._run_inference_async(self.my_sync_data)
                
                with self.my_sync_data.mutex:
                    self.my_sync_data.run_count += 1
                
                if iter_count % TIME_CHECK_INTERVAL == 0:
                    current_time = time.time()
                    duration = current_time - start_time
                    if duration >= self.time:
                        break
                
                iter_count += 1
                self.count += 1
        else:
            # Loop-based execution
            for _ in range(self.loop):
                inference_id = self._run_inference_async(self.my_sync_data)
                
                with self.my_sync_data.mutex:
                    self.my_sync_data.run_count += 1
                    
                self.count += 1
        
        # Wait for all callbacks for this thread to complete
        with self.my_sync_data.condition:
            timeout = getattr(self.exec_option, 'callback_delay', 30)
            if not self.my_sync_data.condition.wait_for(
                lambda: self.my_sync_data.callback_count == self.my_sync_data.run_count, 
                timeout=timeout
            ):
                print("Timeout waiting for shared IE callbacks to complete")

    def _execute_multi_ie_mode(self):
        """Execute in multi IE mode"""
        # Individual callback is already registered in constructor
        if self.time > 0:
            # Time-based execution
            start_time = time.time()
            TIME_CHECK_INTERVAL = 100
            iter_count = 0
            
            while True:
                self._run_inference_async()
                
                with self.individual_cb_mutex:
                    self.individual_run_count += 1
                
                if iter_count % TIME_CHECK_INTERVAL == 0:
                    current_time = time.time()
                    duration = current_time - start_time
                    if duration >= self.time:
                        break
                
                iter_count += 1
                self.count += 1
        else:
            # Loop-based execution
            for _ in range(self.loop):
                self._run_inference_async()
                
                with self.individual_cb_mutex:
                    self.individual_run_count += 1
                    
                self.count += 1
        
        # Wait for all callbacks for this individual IE to complete
        with self.individual_cb_cv:
            timeout = getattr(self.exec_option, 'callback_delay', 30)
            if not self.individual_cb_cv.wait_for(
                lambda: self.individual_callback_count == self.individual_run_count,
                timeout=timeout
            ):
                print("Timeout waiting for individual IE callbacks to complete")

class AsyncWaitExecutor(BaseExecutor):
    """Asynchronous wait executor implementation"""
    
    def __init__(self, ie, test_case: TestCase, exec_option: ExecutionOption, 
                 input_buffer: Any, version: int, input_path: str):
        super().__init__(ie, test_case, exec_option, input_buffer, version, input_path)
        self.id_queue = ConcurrentQueue(1000)
        self.producer_done = threading.Event()
        self.consumer_thread = None
        
        if self.exec_option.output_buffer == "user":
            self.output_ptr = np.zeros(self.ie.get_output_size(), dtype=np.uint8)
        else:
            self.output_ptr = None
        
        self.ie.register_callback(None)

    def __del__(self):
        """Cleanup resources"""
        # Signal consumer thread to stop and wait for it to finish
        self.producer_done.set()
        if self.consumer_thread and self.consumer_thread.is_alive():
            self.consumer_thread.join()
        
        # Free output buffer if allocated (handled by GC in Python)
        self.output_ptr = None
        
        # Clear any remaining items in queue
        while not self.id_queue.empty():
            try:
                self.id_queue.get_nowait()
            except:
                break
                
        super().__del__()

    def _consumer_worker(self):
        """Consumer worker thread function"""
        try:
            while not self.producer_done.is_set() or not self.id_queue.empty():
                if not self.id_queue.empty():
                    inference_id = self.id_queue.get()
                    # Wait for inference completion using DXRT Python API
                    outputs = self.ie.wait(inference_id)
                    if self._should_bit_match:
                        self.bm.set_output(outputs)
                        self.bm.bit_match()
                else:
                    # Small delay to prevent busy waiting when queue is empty
                    time.sleep(0.001)  # 1ms delay
        except Exception as e:
            print(f"Consumer thread error: {e}")

    def do_execute(self):
        """Asynchronous wait execution implementation"""
        # Reset producer done flag
        self.producer_done.clear()
        
        # Start consumer thread
        self.consumer_thread = threading.Thread(target=self._consumer_worker)
        self.consumer_thread.start()

        # Producer logic
        if self.time > 0:
            # Time-based execution
            start_time = time.time()
            TIME_CHECK_INTERVAL = 100
            iter_count = 0
            
            while True:
                inference_id = self._run_inference_async()
                self.id_queue.put(inference_id)
                
                if iter_count % TIME_CHECK_INTERVAL == 0:
                    current_time = time.time()
                    duration_sec = int(current_time - start_time)
                    if duration_sec >= self.time:
                        break
                
                iter_count += 1
                self.count += 1
        else:
            # Loop-based execution
            for _ in range(self.loop):
                inference_id = self._run_inference_async()
                self.id_queue.put(inference_id)
                self.count += 1
        
        # Signal producer is done
        self.producer_done.set()
        
        # Wait for consumer to finish processing all items
        if self.consumer_thread:
            self.consumer_thread.join()
        
        # Verify all jobs are completed
        if not self.id_queue.empty():
            print("Error: Some jobs are not completed")
            exit(-1)

class BatchExecutor(BaseExecutor):
    """Batch executor implementation"""
    
    def __init__(self, ie, test_case: TestCase, exec_option: ExecutionOption, 
                 input_buffer: Any, version: int, input_path: str):
        super().__init__(ie, test_case, exec_option, input_buffer, version, input_path)
        
        # Default batch size (should be configurable from test case if available)
        self.batch_size = getattr(test_case.ie_option, 'batch_size', 3)
        
        # Create batch input/output buffers based on multi_input_model_inference.py logic
        self._create_batch_buffers()
        
        self.ie.register_callback(None)

    def _create_batch_buffers(self):
        """Create batch input and output buffers using explicit batch format"""
        if self.ie.is_multi_input_model():
            # Multi-input model: Create List[List[np.ndarray]] format
            input_sizes = self.ie.get_input_tensor_sizes()
            output_sizes = self.ie.get_output_tensor_sizes()
            
            # Create batch input data - Explicit Batch Format
            self.input_buffers = []
            self.output_buffers = []
            
            for i in range(self.batch_size):
                # Create input tensors for this sample
                sample_inputs = []
                offset = 0
                buffer_ptr = self.input_buffer
                
                for size in input_sizes:
                    if isinstance(buffer_ptr, np.ndarray):
                        # Create view of input buffer for each tensor
                        sample_input = buffer_ptr[offset:offset+size]
                    else:
                        # Handle byte buffer case
                        sample_input = np.frombuffer(buffer_ptr, dtype=np.uint8, count=size, offset=offset)
                    
                    sample_inputs.append(sample_input)
                    offset += size
                
                self.input_buffers.append(sample_inputs)
                
                # Create output buffers for this sample if user-managed
                if self.exec_option.output_buffer == "user":
                    sample_outputs = []
                    for size in output_sizes:
                        output_buffer = np.zeros(size, dtype=np.uint8)
                        sample_outputs.append(output_buffer)
                    self.output_buffers.append(sample_outputs)

            if self.exec_option.output_buffer == "internal":
                self.output_buffers = []
        else:
            # Single-input model: Create List[List[np.ndarray]] format like run_batch_model.py
            self.input_buffers = []
            self.output_buffers = []
            
            for i in range(self.batch_size):
                # Each batch sample is wrapped in a list (run_batch_model.py style)
                if isinstance(self.input_buffer, np.ndarray):
                    batch_input = self.input_buffer.copy()
                    # Add variation for each batch sample
                    self.input_buffers.append([batch_input])  # Wrap in list
                else:
                    # Handle byte buffer case
                    batch_input = np.frombuffer(self.input_buffer, dtype=np.uint8)
                    self.input_buffers.append([batch_input])  # Wrap in list
                
                # Create output buffers if user-managed
                if self.exec_option.output_buffer == "user":
                    output_buffer = np.zeros(self.ie.get_output_size(), dtype=np.uint8)
                    self.output_buffers.append([output_buffer])  # Wrap in list
            
            # For single-input, if internal buffer, still need empty output_buffers
            if self.exec_option.output_buffer != "user":
                self.output_buffers = []

    def __del__(self):
        """Cleanup resources"""
        # Clean up batch buffers
        if hasattr(self, 'input_buffers'):
            self.input_buffers.clear()
        if hasattr(self, 'output_buffers'):
            self.output_buffers.clear()
        if hasattr(self, 'batch_inputs'):
            self.batch_inputs.clear()
        if hasattr(self, 'batch_outputs'):
            self.batch_outputs.clear()
        super().__del__()

    def do_execute(self):
        """Batch execution implementation"""
        if self.time > 0:
            # Time-based batch execution
            start_time = time.time()
            TIME_CHECK_INTERVAL = 100
            
            while True:
                # Run batch inference using DXRT Python API
                if self.output_buffers:
                    batch_results = self.ie.run(self.input_buffers, output_buffers=self.output_buffers)
                else:
                    batch_results = self.ie.run(self.input_buffers)
                
                # Process each sample output in the batch
                # batch_results is List[List[np.ndarray]] for multi-input or List[np.ndarray] for single-input
                if self.ie.is_multi_input_model():
                    # Multi-input: batch_results is List[List[np.ndarray]]

                    for sample_outputs in batch_results:
                        if self._should_bit_match:
                            self.bm.set_output(sample_outputs)
                            self.bm.bit_match()
                else:
                    # Single-input: batch_results is List[np.ndarray]
                    for output in batch_results:
                        if self._should_bit_match:
                            self.bm.set_output(output)
                            self.bm.bit_match()
                
                if self.count % TIME_CHECK_INTERVAL == 0:
                    current_time = time.time()
                    duration_sec = int(current_time - start_time)
                    if duration_sec >= self.time:
                        break
                
                self.count += 1
        else:
            # Loop-based batch execution
            for _ in range(self.loop):
                # Run batch inference using DXRT Python API
                if self.output_buffers:
                    batch_results = self.ie.run(self.input_buffers, output_buffers=self.output_buffers)
                else:
                    batch_results = self.ie.run(self.input_buffers)
                
                # Process each sample output in the batch
                if self.ie.is_multi_input_model():
                    # Multi-input: batch_results is List[List[np.ndarray]]
                    for sample_outputs in batch_results:
                        if self._should_bit_match:
                            self.bm.set_output(sample_outputs)
                            self.bm.bit_match()
                else:
                    # Single-input: batch_results is List[np.ndarray]
                    for output in batch_results:
                        if self._should_bit_match:
                            self.bm.set_output([output])  # Wrap single output in list for consistency
                            self.bm.bit_match()
                        
                self.count += 1

def create_executor(ie, test_case: TestCase, exec_option: ExecutionOption, 
                   input_buffer: Any, version: int, input_path: str, 
                   sync_data: Optional[ThreadSyncData] = None) -> Optional[BaseExecutor]:
    """Factory function to create BaseExecutor instances"""
    
    if exec_option.inference_function == "sync":
        return SyncExecutor(ie, test_case, exec_option, input_buffer, version, input_path)
    elif exec_option.inference_function == "async":
        if exec_option.async_method == "callback":
            return AsyncCallbackExecutor(ie, test_case, exec_option, input_buffer, version, input_path, sync_data)
        elif exec_option.async_method == "wait":
            return AsyncWaitExecutor(ie, test_case, exec_option, input_buffer, version, input_path)
        else:
            print(f"Error: Unknown async method: {exec_option.async_method}")
            return None
    elif exec_option.inference_function == "batch":
        return BatchExecutor(ie, test_case, exec_option, input_buffer, version, input_path)
    else:
        print(f"Error: Unknown inference function: {exec_option.inference_function}")
        return None