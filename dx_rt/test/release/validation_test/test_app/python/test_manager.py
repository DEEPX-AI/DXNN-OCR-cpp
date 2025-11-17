"""
TestManager class for managing and executing test cases
Ported from C++ version
"""

import json
import time
import os
from datetime import datetime, timezone
from typing import List, Dict, Any
from enum import Enum
from dataclasses import dataclass
import threading

from generator import TestCase, IEOption, ExecutionOption
from executor_manager import ExecutorManager, RunResult

from dx_engine.configuration import Configuration

class ResultStatus(Enum):
    FAIL = "fail"
    SUCCESS = "success"
    SKIP = "skip"

class ResultType(Enum):
    NONE = "none"
    BITMATCH = "bitmatch"
    EXECUTION = "execution"

@dataclass
class ResultInform:
    """Information about test result"""
    status: ResultStatus
    type: ResultType
    ie_option: IEOption
    exec_option: ExecutionOption
    duration: float  # duration in seconds

class TestManager:
    """Manages test execution and reporting"""
    
    def __init__(self, test_cases: List[TestCase], verbose: int, log_level: int = 0, result_name: str = ""):
        self.test_cases = test_cases
        self.verbose = verbose # 0: failed only, 1: show progress, 2: include skipped, 3: all, 4: debug
        self.log_level = log_level  # 0: failed only, 1: show progress, 2: include skipped, 3: all, 4: debug
        self.result_name = result_name
        
        # Set up temp path for configuration files
        self._setup_temp_path()
        
        # Count total execution options across all test cases
        self.total_tests = 0
        for test_case in test_cases:
            self.total_tests += len(test_case.exec_options)
        
        # Test result counters
        self.passed_tests = 0
        self.bm_failed_tests = 0
        self.tc_invalid_tests = 0
        self.bm_skipped_tests = 0
        self.tc_skipped_tests = 0

        self.total_passed_tests = 0
        self.total_bm_failed_tests = 0
        self.total_tc_invalid_tests = 0
        self.total_bm_skipped_tests = 0
        self.total_tc_skipped_tests = 0
        
        # Progress tracking
        self.current_run = 0
        
        # Timing
        self.start_time = time.time()
        self.system_start = datetime.now(timezone.utc)
        self.end_time = 0
        
        # Saved test results for reporting
        self.saved_tests: List[ResultInform] = []

    def _setup_temp_path(self):
        """Set up temporary path for configuration files based on current executable location"""
        try:
            # Get current script directory
            current_file = os.path.abspath(__file__)
            current_dir = os.path.dirname(current_file)
            
            # Find project root by looking for the consistent path pattern
            # Expected path: /some/path/repo_name/test/release/validation_test/test_app/python/test_manager.py
            pattern = "/test/release/validation_test/test_app/python"
            pattern_pos = current_dir.find(pattern)
            
            if pattern_pos != -1:
                # Extract project root path: everything before "/test/release/..."
                project_root = current_dir[:pattern_pos]
                
                # Set temp_path to test_config/tmp relative to project root
                self.temp_path = os.path.join(project_root, "test", "release", "validation_test", "test_config", "tmp")
            else:
                # Fallback: use relative path from current script directory
                # Go up to validation_test directory, then navigate to test_config/tmp
                # ../../test_config/tmp (from python to test_config/tmp)
                self.temp_path = os.path.join(current_dir, "..", "..", "test_config", "tmp")
                
            # Create temp directory if it doesn't exist
            os.makedirs(self.temp_path, exist_ok=True)
            
        except Exception as e:
            print(f"Warning: Failed to set up temp path: {e}")
            self.temp_path = "."  # Fallback to current directory

    def save_temp_result(self, ie_option: IEOption, exec_option: ExecutionOption):
        """Save temporary test configuration as JSON file"""
        try:
            # Extract model name from path
            model_name = ie_option.model_path
            
            # Remove the filename first
            if "/" in model_name or "\\" in model_name:
                model_name = os.path.dirname(model_name)
                
                # Now extract the last directory name (model name)
                model_name = os.path.basename(model_name)
            
            file_name = "temp_config.json"
            full_path = os.path.join(self.temp_path, file_name)
            
            # Create JSON configuration in partial_test.json format
            config = {
                "model": [model_name],
                
                "configuration": {
                    "dynamic-cpu-offloading": [ie_option.dynamic_cpu_offloading]
                },
                
                "threadStyle": [{
                    "type": ie_option.thread_type,
                    "count": ie_option.thread_count
                }],
                
                "ieOption": {
                    "ort": [ie_option.ort],
                    "bound": [ie_option.bound],
                    "device": [ie_option.device]
                },
                
                "inferenceFunction": [exec_option.inference_function],
                
                "inoutOption": {
                    "inputStyle": [exec_option.input_style],
                    "outputBuffer": [exec_option.output_buffer],
                    "asyncMethod": [exec_option.async_method],
                    "callbackDelay": 100,
                    "loop": exec_option.loop,
                    "time": exec_option.time,
                    "bitmatch": exec_option.bitmatch
                }
            }
            
            with open(full_path, 'w') as json_file:
                json.dump(config, json_file, indent=2)
                
        except Exception as e:
            print(f"Error: Could not create temp result file: {e}")

    def run(self):
        """Run all test cases"""
        test_case_num = 0
        current_model = ""
        
        print(f"Run total {self.total_tests} test cases")
        
        for test_case in self.test_cases:
            if current_model != test_case.ie_option.model_path:
                if current_model != "":
                    self._print_test_summary(current_model)
                    self._flush_results()
                current_model = test_case.ie_option.model_path
                print()
                print(f"Current Model: {current_model}")
            
            test_case_num += 1
            self._run_single_test_case(test_case, test_case_num)
        
        # Print summary for the last model
        if current_model:
            self._print_test_summary(current_model)
            self._flush_results()

    def _run_single_test_case(self, test_case: TestCase, test_case_num: int):
        """Run a single test case"""
        try:
            config = Configuration()
            config.set_enable(Configuration.ITEM.SHOW_PROFILE, False)

            if test_case.ie_option.dynamic_cpu_offloading == "on":
                config.set_enable(Configuration.ITEM.DYNAMIC_CPU_THREAD, True)

            else:
                config.set_enable(Configuration.ITEM.DYNAMIC_CPU_THREAD, False)
            
            for exec_option in test_case.exec_options:
                self._run_execution_option(test_case, exec_option)
                
        except (RuntimeError, ValueError, IOError) as e:
            # DXRT exceptions are translated to standard Python exceptions
            print(f"====== Test Case {test_case_num} failed (dxrt) ======")
            print(f"Error: {e}")
        except Exception as e:
            print(f"====== Test Case {test_case_num} failed ======")
            print(f"Error: {e}")

    def _run_execution_option(self, test_case: TestCase, exec_option: ExecutionOption):
        """Run a single execution option"""
        self.current_run += 1
        if self.verbose >= 1:
            print(f"Progress: {self.current_run}/{self.total_tests}")

        # For debug
        if self.verbose >= 4:
            print("--------------------- RUNNING TEST CASE ---------------------")
            self._print_test_case_info(test_case)
            self._print_execution_option_info(exec_option)

        # Save temporary result configuration
        self.save_temp_result(test_case.ie_option, exec_option)

        if self._validate(test_case, exec_option):
            manager = ExecutorManager.create_executor_manager(test_case, exec_option)
            
            duration_start = time.time()
            result = manager.run()
            duration_end = time.time()
            duration = duration_end - duration_start

            if result == RunResult.BM_PASS:
                self.passed_tests += 1
                if self.verbose >= 3:
                    print()
                    print("++++++++++++++++++++ SUCCESS TEST CASE ++++++++++++++++++++")
                    self._print_test_case_info(test_case)
                    self._print_execution_option_info(exec_option)
                
                if self.log_level >= 3:
                    self.saved_tests.append(ResultInform(
                        ResultStatus.SUCCESS, ResultType.BITMATCH, 
                        test_case.ie_option, exec_option, duration))

            elif result == RunResult.BM_SKIP:
                self.bm_skipped_tests += 1
                if self.verbose >= 2:
                    print()
                    print("==================== BITMATCH SKIPPED TEST CASE ====================")
                    self._print_test_case_info(test_case)
                    self._print_execution_option_info(exec_option)
                
                if self.log_level >= 2:
                    self.saved_tests.append(ResultInform(
                        ResultStatus.SKIP, ResultType.BITMATCH,
                        test_case.ie_option, exec_option, duration))

            elif result == RunResult.TC_SKIP:
                self.tc_skipped_tests += 1
                if self.verbose >= 2:
                    print()
                    print("==================== TEST CASE IGNORED ====================")
                    self._print_test_case_info(test_case)
                    self._print_execution_option_info(exec_option)
                
                if self.log_level >= 2:
                    self.saved_tests.append(ResultInform(
                        ResultStatus.SKIP, ResultType.EXECUTION,
                        test_case.ie_option, exec_option, duration))

            elif result == RunResult.BM_FAIL:
                print()
                print("xxxxxxxxxxxxxxxxxxxx BITMATCH FAILED TEST CASE xxxxxxxxxxxxxxxxxxxx")
                self.bm_failed_tests += 1
                self._print_test_case_info(test_case)
                self._print_execution_option_info(exec_option)
                self.saved_tests.append(ResultInform(
                    ResultStatus.FAIL, ResultType.BITMATCH,
                    test_case.ie_option, exec_option, duration))

            elif result == RunResult.TC_INVALID:
                print()
                print("xxxxxxxxxxxxxxxxxxxx INVALID TEST CASE xxxxxxxxxxxxxxxxxxxx")
                self.tc_invalid_tests += 1
                self._print_test_case_info(test_case)
                self._print_execution_option_info(exec_option)
                self.saved_tests.append(ResultInform(
                    ResultStatus.FAIL, ResultType.EXECUTION,
                    test_case.ie_option, exec_option, duration))

            else:
                print()
                print("xxxxxxxxxxxxxxxxxxxx INVALID TEST RESULT xxxxxxxxxxxxxxxxxxxx")
                print("Error: Unknown RunResult value")
                self.tc_invalid_tests += 1
        else:
            self.tc_invalid_tests += 1

    def _validate(self, test_case: TestCase, exec_option: ExecutionOption) -> bool:
        """Validate test case and execution option"""
        if test_case.ie_option.thread_type not in ["single-ie", "multi-ie"]:
            print(f"Error: Unknown thread type: {test_case.ie_option.thread_type}")
            return False

        if test_case.ie_option.thread_count < 1:
            print(f"Error: Invalid thread count: {test_case.ie_option.thread_count}")
            return False

        if exec_option.time <= 0 and exec_option.loop <= 0:
            print("Error: both time and loop cannot be zero, skipping this execution option")
            return False

        return True

    def _flush_results(self):
        """Reset counters for the next model"""
        self.total_passed_tests += self.passed_tests
        self.total_bm_failed_tests += self.bm_failed_tests
        self.total_tc_invalid_tests += self.tc_invalid_tests
        self.total_bm_skipped_tests += self.bm_skipped_tests
        self.total_tc_skipped_tests += self.tc_skipped_tests

        self.passed_tests = 0
        self.bm_failed_tests = 0
        self.tc_invalid_tests = 0
        self.bm_skipped_tests = 0
        self.tc_skipped_tests = 0

    def _print_test_summary(self, model_name: str):
        """Print test summary for a model"""
        current_model_tests = (self.passed_tests + self.bm_failed_tests + 
                              self.tc_invalid_tests + self.bm_skipped_tests + 
                              self.tc_skipped_tests)
        
        print()
        print("================== Model Test Summary ==================")
        print(f"Model Name: {model_name}")
        print(f"Total Tests: {current_model_tests}")
        print(f"   Test Pass: {self.passed_tests}")
        print(f"   Test Case Ignored: {self.tc_skipped_tests}")
        print(f"   BitMatch Skipped: {self.bm_skipped_tests}")
        print(f"   BitMatch Failed: {self.bm_failed_tests}")
        print(f"   Invalid: {self.tc_invalid_tests}")

    def _print_test_case_info(self, test_case: TestCase):
        """Print test case information"""
        ie_opt = test_case.ie_option
        print(f"  Model: {ie_opt.model_path}")
        print(f"  Dynamic CPU Offloading: {ie_opt.dynamic_cpu_offloading}")
        print(f"  Thread Type: {ie_opt.thread_type}")
        print(f"  Thread Count: {ie_opt.thread_count}")
        print(f"  ORT: {ie_opt.ort}")
        print(f"  Bound: {ie_opt.bound}")
        print(f"  Device: {ie_opt.device}")

    def _print_execution_option_info(self, exec_option: ExecutionOption):
        """Print execution option information"""
        print(f"  Inference Function: {exec_option.inference_function}")
        if exec_option.inference_function == "async":
            print(f"      Async Method: {exec_option.async_method}")
        print(f"  Input Style: {exec_option.input_style}")
        print(f"  Output Buffer: {exec_option.output_buffer}")
        if exec_option.time > 0:
            print(f"  Time: {exec_option.time}")
        else:
            print(f"  Loop: {exec_option.loop}")
        print(f"  Bitmatch: {exec_option.bitmatch}")

    def make_report(self):
        """Generate JSON report"""
        self.end_time = time.time()
        total_duration = self.end_time - self.start_time

        # Generate JSON report
        try:
            with open(self.result_name, 'w') as report_file:
                # Convert start time to ISO 8601 string
                start_time_str = self.system_start.strftime('%Y-%m-%dT%H:%M:%SZ')

                report_data = {
                    "test_suite_name": "validation_app_test",
                    "start_time": start_time_str,
                    "run_time_sec": total_duration,
                    "summary": {
                        "total_cases": self.total_tests,
                        "passed": self.total_passed_tests,
                        "failed": self.total_bm_failed_tests,
                        "ignored": self.total_tc_skipped_tests,
                        "skipped": self.total_bm_skipped_tests,
                        "invalid": self.total_tc_invalid_tests
                    },
                    "results": {}
                }

                # Group test results by model path
                model_results: Dict[str, List[Dict[str, Any]]] = {}
                for save in self.saved_tests:
                    model_path = save.ie_option.model_path
                    if model_path not in model_results:
                        model_results[model_path] = []
                    
                    result_entry = {
                        "status": save.status.value,
                        "duration_sec": save.duration,
                        "dynamic_cpu_offloading": save.ie_option.dynamic_cpu_offloading == "on",
                        "thread_type": save.ie_option.thread_type,
                        "thread_count": save.ie_option.thread_count,
                        "ort": save.ie_option.ort,
                        "core_bound": save.ie_option.bound,
                        "device_bound": save.ie_option.device,
                        "inference_function": save.exec_option.inference_function,
                        "input_style": save.exec_option.input_style,
                        "output_buffer": save.exec_option.output_buffer,
                        "async_method": save.exec_option.async_method,
                        "loop": save.exec_option.loop,
                        "time": save.exec_option.time,
                        "bitmatch": save.exec_option.bitmatch
                    }

                    # Add type based on status and result type
                    if save.status == ResultStatus.SUCCESS:
                        if save.type == ResultType.BITMATCH:
                            result_entry["type"] = "bitmatch_pass"
                    elif save.status == ResultStatus.FAIL:
                        if save.type == ResultType.BITMATCH:
                            result_entry["type"] = "bitmatch_fail"
                        elif save.type == ResultType.EXECUTION:
                            result_entry["type"] = "execution_fail"
                    elif save.status == ResultStatus.SKIP:
                        if save.type == ResultType.BITMATCH:
                            result_entry["type"] = "bitmatch_skip"
                        elif save.type == ResultType.EXECUTION:
                            result_entry["type"] = "execution_ignored"

                    model_results[model_path].append(result_entry)

                # Add model results to report
                report_data["results"] = model_results

                # Write JSON to file
                json.dump(report_data, report_file, indent=2)

            print(f"\nTest report saved to: {self.result_name}")
            print(f"Total execution time: {total_duration} sec")

        except Exception as e:
            print(f"Error: Could not write report file {self.result_name}: {e}")