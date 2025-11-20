"""
ExecutorManager class for managing threads and InferenceEngine instances
Ported from C++ version
"""

import threading
import time
from typing import List, Optional, Dict, Any
from enum import Enum
from dataclasses import dataclass
import sys

from generator import TestCase, ExecutionOption, set_inference_configuration_from_ie_option
from executor import BaseExecutor, create_executor
from input_utils import InputUtils

from dx_engine import InferenceEngine, InferenceOption

class RunResult(Enum):
    """Result of test execution"""
    BM_PASS = 0     # Assume that TC is actually run
    BM_SKIP = 1     # Assume that TC is actually run
    TC_SKIP = 2
    BM_FAIL = 3     # Assume that TC is actually run
    TC_INVALID = 4  # Test case run failed due to invalid option

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

class ExecutorManager:
    """Manages threads and InferenceEngine instances"""
    
    def __init__(self, test_case: TestCase, exec_option: ExecutionOption):
        self.test_case = test_case
        self.exec_option = exec_option
        self.shared_ie = None  # Only for single-ie mode
        
        # Thread management
        self.threads: List[threading.Thread] = []
        self.bit_match_results: List[int] = []
        
        # Only for single-ie mode
        self.executors: List[BaseExecutor] = []
        self.all_thread_sync_data: List[ThreadSyncData] = []
        
        # Global callback registration
        self.is_global_callback_registered = False
        self.global_callback_registration_mutex = threading.Lock()
        
        # Mutex for thread-safe result gathering
        self.mutex = threading.Lock()  # Changed from result_mutex to match C++ _mutex

    def __del__(self):
        """Cleanup resources"""
        # Ensure all threads are joined before cleanup
        for thread in self.threads:
            if thread.is_alive():
                thread.join(timeout=1.0)
        
        # Clear all resources
        self.threads.clear()
        self.executors.clear()
        self.all_thread_sync_data.clear()
        self.shared_ie = None
        self.bit_match_results.clear()

    def run(self) -> RunResult:
        """Execute all threads and wait for completion"""
        # Initialize result vector
        self.bit_match_results = [RunResult.TC_SKIP.value] * self.test_case.ie_option.thread_count
        
        if self.test_case.ie_option.thread_type == "single-ie":
            self._execute_with_shared_ie()
        elif self.test_case.ie_option.thread_type == "multi-ie":
            self._execute_with_multi_ie()
        else:
            print(f"Error: Unknown thread type: {self.test_case.ie_option.thread_type}")
            return RunResult.TC_INVALID
        
        # Wait for all threads to complete
        for thread in self.threads:
            if thread.is_alive():
                thread.join()
        
        # Find the maximum (worst) result among all threads
        return self._get_result()

    def _execute_with_shared_ie(self):
        """Execute with shared InferenceEngine"""
        # Create shared InferenceEngine
        self._create_shared_inference_engine()
        
        if not self.shared_ie:
            print("Error: Failed to create shared InferenceEngine")
            return
        
        # Initialize thread synchronization data
        self.all_thread_sync_data.clear()
        self.executors = [None] * self.test_case.ie_option.thread_count
        
        for i in range(self.test_case.ie_option.thread_count):
            sync_data = ThreadSyncData(thread_id=i)
            self.all_thread_sync_data.append(sync_data)

        # Global callback registration for async callback mode
        with self.global_callback_registration_mutex:
            if (self.exec_option.inference_function == "async" and
                not self.is_global_callback_registered and
                self.exec_option.async_method == "callback"):
                
                def global_callback(outputs, user_arg):
                    if user_arg is None:
                        print("Error: user_arg is null in global callback!")
                        sys.exit(-1)

                    # Cast user_arg back to ThreadSyncData
                    sync_data = user_arg  # In Python, user_arg should be a ThreadSyncData instance
                    thread_id = sync_data.thread_id

                    # Now we can access the specific executor for this thread
                    # Each thread access different executor instance but may need to protected by mutex
                    if (thread_id >= 0 and thread_id < len(self.executors) and self.executors[thread_id]):
                        if (getattr(self.exec_option, 'bitmatch', False) and getattr(self.shared_ie, 'get_compile_type', lambda: None)() != "debug"):
                            # Use the executor - for example, for bitmatch processing
                            self.executors[thread_id].set_output(outputs)
                            self.executors[thread_id].bit_match()

                    with sync_data.mutex:
                        sync_data.callback_count += 1
                        if sync_data.run_count == sync_data.callback_count:
                            sync_data.condition.notify()  # In Python, notify() is equivalent to notify_one()

                    return 0

                # Register the callback using the dxrt API (snake_case)
                self.shared_ie.register_callback(global_callback)
                self.is_global_callback_registered = True

        # Create threads that share the same IE
        for t in range(self.test_case.ie_option.thread_count):
            thread = threading.Thread(
                target=self._worker_thread_shared_ie,
                args=(t,)
            )
            thread.daemon = True
            self.threads.append(thread)
            thread.start()

    def _execute_with_multi_ie(self):
        """Execute with multiple InferenceEngine instances"""
        # Create threads that each have their own IE
        for t in range(self.test_case.ie_option.thread_count):
            thread = threading.Thread(
                target=self._worker_thread_multi_ie,
                args=(t,)
            )
            thread.daemon = True
            self.threads.append(thread)
            thread.start()

    def _worker_thread_shared_ie(self, thread_index: int):
        """Worker thread for shared IE mode"""
        try:
            # Create input buffer for this thread using InputUtils
            iu = InputUtils(self.exec_option, self.test_case, self.shared_ie)
            iu.create_input_buffer()
            thread_input_buffer = iu.get_input_buffer()
            version = iu.get_version()
            input_path = iu.get_file_path()
            
            sync_data = self.all_thread_sync_data[thread_index] if thread_index < len(self.all_thread_sync_data) else None
            
            executor = create_executor(
                self.shared_ie, self.test_case, self.exec_option,
                thread_input_buffer, version, input_path, sync_data
            )
            
            if executor:
                self.executors[thread_index] = executor
                result = executor.execute()
                self._gather_results(result, thread_index)
            else:
                self._gather_results(None, thread_index)
                
        except Exception as e:
            print(f"Error in worker thread {thread_index}: {e}")
            self._gather_results(None, thread_index)

    def _worker_thread_multi_ie(self, thread_index: int):
        """Worker thread for multi IE mode"""
        try:
            op = InferenceOption()
            set_inference_configuration_from_ie_option(op, self.test_case.ie_option)
            ie = InferenceEngine(self.test_case.ie_option.model_path, op)
            
            # Create input buffer for this thread using InputUtils
            iu = InputUtils(self.exec_option, self.test_case, ie)
            iu.create_input_buffer()
            thread_input_buffer = iu.get_input_buffer()
            version = iu.get_version()
            input_path = iu.get_file_path()
            
            executor = create_executor(
                ie, self.test_case, self.exec_option,
                thread_input_buffer, version, input_path, None
            )
            
            if executor:
                result = executor.execute()
                self._gather_results(result, thread_index)
            else:
                self._gather_results(None, thread_index)
                
        except Exception as e:
            print(f"Error in worker thread {thread_index}: {e}")
            self._gather_results(None, thread_index)

    def _gather_results(self, result, thread_index: int):
        """Gather results from threads"""
        with self.mutex:  # Changed from result_mutex to match C++ _mutex
            if result and result.model_run:
                if result.bit_match_run:
                    if not result.is_fail:
                        self.bit_match_results[thread_index] = RunResult.BM_PASS.value
                        return
                    else:
                        self.bit_match_results[thread_index] = RunResult.BM_FAIL.value
                        return
                else:
                    self.bit_match_results[thread_index] = RunResult.BM_SKIP.value
                    return
            else:
                self.bit_match_results[thread_index] = RunResult.TC_SKIP.value
                return

    def _get_result(self) -> RunResult:
        """Get the final result"""
        if not self.bit_match_results:
            return RunResult.TC_INVALID
        
        # Find maximum value (worst result)
        max_result = max(self.bit_match_results)
        return RunResult(max_result)

    def _create_shared_inference_engine(self):
        """Create shared InferenceEngine"""
        try:
            op = InferenceOption()
            set_inference_configuration_from_ie_option(op, self.test_case.ie_option)
            self.shared_ie = InferenceEngine(self.test_case.ie_option.model_path, op)
        except Exception as e:
            print(f"Error creating shared InferenceEngine: {e}")
            self.shared_ie = None

    @staticmethod
    def create_executor_manager(test_case: TestCase, exec_option: ExecutionOption) -> 'ExecutorManager':
        """Factory function to create ExecutorManager"""
        return ExecutorManager(test_case, exec_option)