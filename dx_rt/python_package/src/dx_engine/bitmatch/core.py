#
# Copyright (C) 2018- DEEPX Ltd.
# All rights reserved.
#
# This software is the property of DEEPX and is provided exclusively to customers 
# who are supplied with DEEPX NPU (Neural Processing Unit). 
# Unauthorized sharing or usage is strictly prohibited by law.
#

import os
import sys 
import glob
import argparse
import gc
import time

import subprocess
import multiprocessing 
from dx_engine import InferenceOption

try:
    from tqdm import tqdm
    TQDM_AVAILABLE = True
except ImportError:
    TQDM_AVAILABLE = False
    print("Warning: tqdm not available. Install with 'pip install tqdm' for progress bars.")

from .module.config import TestConfig
from .module.tester import BitMatchTester
from .module.utils import process_paths, pcie_rescan, parse_devices_option


def _check_ipc_resources():
    """Check and log current IPC message queue usage"""
    try:
        result = subprocess.run(['ipcs', '-q'], capture_output=True, text=True, timeout=2)
        if result.returncode == 0:
            lines = result.stdout.strip().split('\n')
            # Count message queues (skip header lines)
            mq_count = sum(1 for line in lines if line and not line.startswith('-') and 'msqid' not in line)
            if mq_count > 10:
                print(f"[IPC Warning] High number of message queues: {mq_count}")
    except Exception as e:
        print(f"[IPC] Failed to check IPC resources: {e}")


def _cleanup_ipc_resources():
    """Clean up stale IPC message queues"""
    try:
        print("[IPC] Cleaning up IPC message queues...")
        # Get list of message queues
        result = subprocess.run(['ipcs', '-q'], capture_output=True, text=True, timeout=2)
        if result.returncode == 0:
            lines = result.stdout.strip().split('\n')
            for line in lines:
                if line and not line.startswith('-') and 'msqid' not in line:
                    parts = line.split()
                    if len(parts) >= 2:
                        msqid = parts[1]
                        try:
                            subprocess.run(['ipcrm', '-q', msqid], capture_output=True, timeout=1)
                        except:
                            pass
        print("[IPC] IPC cleanup completed")
    except Exception as e:
        print(f"[IPC] Failed to cleanup IPC resources: {e}")


def _handle_ipc_error(model_name, error_message):
    """Handle IPC-related errors with appropriate recovery"""
    ipc_keywords = ['IPC', 'msgrcv', 'msgsnd', 'Identifier removed', 'errno 43', 'EIDRM']
    
    if any(keyword in str(error_message) for keyword in ipc_keywords):
        print(f"[IPC Error] Detected IPC error for model '{model_name}': {error_message}")
        print("[IPC] Attempting recovery...")
        
        # Wait a bit for system to stabilize
        time.sleep(1)
        
        # Clean up IPC resources
        _cleanup_ipc_resources()
        
        # Wait again
        time.sleep(1)
        
        return True  # Indicate this was an IPC error
    
    return False  # Not an IPC error

def run_bitmatch_test(args: argparse.Namespace) -> int:
    # Debug mode setting:
    # args.debug >= 1: Enable RT binary generation via DXRT_DEBUG_DATA
    # args.debug == 2: Enable debug analyzer for intermediate file analysis
    debug_value = args.debug if hasattr(args, 'debug') else 0
    os.environ['DXRT_DEBUG_DATA'] = "1" if debug_value >= 1 else "0"
    debug_mode = (debug_value == 2)

    # Optional: Call pcie_rescan if needed before tests
    # if hasattr(args, 'rescan_pcie') and args.rescan_pcie:
    #     pcie_rescan()

    config = TestConfig(
        sync_mode=args.sync,
        batch_mode=args.batch,
        iterations=args.batch_iteration,
        verbose=args.verbose,
        log_enabled=not args.no_logging,
        performance_mode=args.performance,
        debug_mode=debug_mode,
        save_debug_report=args.save_debug_report,
        use_ort=args.use_ort,
        npu_bound=InferenceOption.BOUND_OPTION(args.npu),
        devices=[int(d) for d in parse_devices_option(args.devices)],
        input_order=args.input_order
    )

    tester = BitMatchTester(config)
    failed_models = []
    final_total_count = 0
    final_pass_count = 0 

    # Load model filter names
    filtered_model_names = None
    if args.model_filter:
        if os.path.exists(args.model_filter):
            try:
                with open(args.model_filter, "r", encoding="utf-8") as file:
                    # Filter out empty lines and lines starting with #
                    filtered_model_names = {line.strip() for line in file if line.strip() and not line.strip().startswith('#')}
            except Exception as e:
                 print(f"Warning: Error reading model filter file '{args.model_filter}': {e}. Filter will not be applied.")
                 filtered_model_names = None
        else:
             print(f"Warning: Model filter file '{args.model_filter}' not found. Filter will not be applied.")


    model_paths_to_process = []

    # Determine list of models/directories to test
    if args.dir:
        # Regression directory mode (--dir)
        potential_model_dirs = [d for d in glob.glob(os.path.join(args.dir, "*")) if os.path.isdir(d)]
        if filtered_model_names is not None:
            # Apply filter if available
            model_paths_to_process = [d for d in potential_model_dirs if os.path.basename(d) in filtered_model_names]
        else:
            model_paths_to_process = potential_model_dirs

        if not model_paths_to_process:
             print(f"Warning: No model directories found in '{args.dir}' (or matching filter) to test.")
             return 0 # Exit successfully if no models found?

    elif args.model_path:
        # Single model/directory mode (--model_path)
        model_paths_to_process = [args.model_path]
        if filtered_model_names is not None:
             # Check if the single model is filtered out
             model_name_to_check = os.path.basename(args.model_path.rstrip('/'))
             if model_name_to_check not in filtered_model_names:
                 print(f"Model '{model_name_to_check}' is excluded by the filter.")
                 return 0 # Exit successfully as this specific model is filtered out

    else:
        print("Error: Please specify either --model_path or --dir argument to run tests.")
        return 1 # Indicate failure due to missing arguments

    print(f"Processing {len(model_paths_to_process)} models...")

    exit_code = 0 # Assume success initially, set to 1 if any test fails

    # Run tests for the specified number of iterations
    for test_itr in range(args.test_iteration):
        print(f"\n--- Test Iteration {test_itr + 1}/{args.test_iteration} ---")

        # Process each model with progress bar
        if TQDM_AVAILABLE and args.dir:
            # Use tqdm for directory mode to show progress with PASS/FAIL counts
            model_iterator = tqdm(
                model_paths_to_process, 
                desc=f"Iteration {test_itr + 1}/{args.test_iteration}",
                unit="model",
                position=1,
                leave=True,
                dynamic_ncols=True,
                postfix={'PASS': 0, 'FAIL': 0}
            )
        else:
            # Fallback to regular iteration
            model_iterator = model_paths_to_process

        for model_idx, model_input_path in enumerate(model_iterator):
            # Determine 'reg' flag based on whether --dir was used
            is_regression_mode = bool(args.dir)
            model_dir, model_name, dxnn, gt_dir, rt_dir = process_paths(
                model_input_path,
                args.gt_dir,
                args.rt_dir,
                args.performance,
                reg=1 if is_regression_mode else 0
            )

            # Skip model processing if process_paths returned invalid paths
            if dxnn is None or gt_dir is None:
                 print(f"Skipping model processing for '{model_input_path}' due to path errors.")
                 exit_code = 1 # Mark as failure if any path processing failed
                 continue # Move to the next model

            if TQDM_AVAILABLE:
                tqdm.write(f"\nProcessing model: {model_name} (Path: {model_dir})")
            else:
                print(f"\nProcessing model: {model_name} (Path: {model_dir})")

            # Check IPC resources before processing
            if model_idx % 20 == 0:  # Check every 20 models
                _check_ipc_resources()

            # Run the actual test logic for the model with IPC error recovery
            model_success = False
            max_retries = 2
            for retry in range(max_retries):
                try:
                    model_success = tester.process_model(model_dir, model_name, dxnn, gt_dir, str(rt_dir), args.loops, args.compile_type)
                    break  # Success, exit retry loop
                except Exception as e:
                    if _handle_ipc_error(model_name, str(e)) and retry < max_retries - 1:
                        print(f"[IPC] Retrying model '{model_name}' after IPC error recovery (attempt {retry + 2}/{max_retries})...")
                        # Recreate tester after IPC error
                        try:
                            tester.shutdown()
                        except:
                            pass
                        time.sleep(2)
                        # Create new tester instance
                        config_retry = TestConfig(
                            sync_mode=args.sync,
                            batch_mode=args.batch,
                            iterations=args.batch_iteration,
                            verbose=args.verbose,
                            log_enabled=not args.no_logging,
                            performance_mode=args.performance,
                            use_ort=args.use_ort,
                            npu_bound=InferenceOption.BOUND_OPTION(args.npu),
                            devices=[int(d) for d in parse_devices_option(args.devices)],
                            input_order=args.input_order
                        )
                        tester = BitMatchTester(config_retry)
                        continue
                    else:
                        # Not an IPC error or final retry failed
                        print(f"Error: Exception during model '{model_name}' processing: {e}")
                        raise
            
            # Check if the model processing failed (returns False)
            if not model_success:
                # If process_model returns False, the model test failed
                print(f"Error: Model '{model_name}' test failed.")
                failed_models.append({
                    'test_itr': test_itr,
                    'model': model_name,
                    'total_count': tester.stats.total_count,
                    'fail_count': tester.stats.total_count - tester.stats.pass_count,
                    'failed_jobs': tester.stats.failed_jobs
                })
                exit_code = 1 # Mark as failure
            
            # Always accumulate stats (even for failed models, they might have partial results)
            final_total_count += tester.stats.total_count
            final_pass_count += tester.stats.pass_count
            
            # Update tqdm postfix with current PASS/FAIL counts
            if TQDM_AVAILABLE and args.dir and hasattr(model_iterator, 'set_postfix'):
                model_iterator.set_postfix({
                    'PASS': final_pass_count, 
                    'FAIL': final_total_count - final_pass_count
                })

    print("\n---------------------------------------------------------------------------------------------------")
    print("Remember to use the `-p` option to measure maximum FPS performance excluding Bitmatch overhead.")
    print("---------------------------------------------------------------------------------------------------")

    # Log results if the tester has the necessary method
    if args.dir:
         try:
             tester.log_all_results(final_pass_count, final_total_count, failed_models)
         except Exception as e:
              print(f"Warning: Error during results logging: {e}")
    else:
         print(f"\nTotal tests: {final_total_count}, Passed: {final_pass_count}, Failed models: {len(failed_models)}")
         if failed_models:
             print("Failed model details:")
             for fail_info in failed_models:
                  print(f"  Iteration {fail_info.get('test_itr', 'N/A')}: Model '{fail_info.get('model', 'N/A')}' ({fail_info.get('fail_count', 'N/A')}/{fail_info.get('total_count', 'N/A')} jobs failed)")

    tester.shutdown()
    del tester
    gc.collect() 
    # Return the final exit code (0 for success, 1 for failure)
    return exit_code


def _process_single_model_task(model_input_path, args, test_iteration_index):
    """
    Task function for processing a single model for one test iteration in a separate process.

    Args:
        model_input_path: Path to the model directory or file.
        args: The argparse.Namespace object containing all arguments.
        test_iteration_index: The index of the current test iteration (0-based).

    Returns:
        A dictionary containing results for this specific model and iteration.
        {
            'model_name': str,
            'model_dir': str,
            'test_itr': int,
            'success': bool, # True if process_paths succeeded AND process_model returned True
            'total_count': int, # Stats from this model run (args.loops iterations)
            'pass_count': int,
            'failed_jobs': list, # Failed jobs from this model run
            'model_result_messages': [] # Initialize a list to store result messages
        }
    """

    config = TestConfig(
        batch_mode=args.batch,
        iterations=args.batch_iteration,
        verbose=args.verbose,
        log_enabled=False, 
        performance_mode=args.performance,
        debug_mode=args.debug,
        save_debug_report=args.save_debug_report,
        use_ort=args.use_ort,
        npu_bound=InferenceOption.BOUND_OPTION(args.npu),
        devices=[int(d) for d in parse_devices_option(args.devices)],
        input_order=args.input_order
    )

    tester = BitMatchTester(config)

    model_dir, model_name, dxnn, gt_dir, rt_dir = process_paths(
        model_input_path,
        args.gt_dir,
        args.rt_dir,
        args.performance,
        reg=1 if args.dir else 0
    )

    result_data = {
        'model_name': os.path.basename(model_input_path.rstrip('/')), 
        'model_dir': model_dir,
        'test_itr': test_iteration_index,
        'success': False, 
        'total_count': 0,
        'pass_count': 0,
        'failed_jobs': []
    }

    if dxnn is None or gt_dir is None:
         print(f"Worker (Iter {test_iteration_index}): Skipping model processing for '{model_input_path}' due to path errors.", file=sys.stderr)
         return result_data
    
    result_data['model_name'] = model_name
    result_data['model_dir'] = model_dir


    if TQDM_AVAILABLE:
        tqdm.write(f"Worker (Iter {test_iteration_index}): Processing model: {model_name} (Path: {model_dir})")
    else:
        print(f"Worker (Iter {test_iteration_index}): Processing model: {model_name} (Path: {model_dir})")

    # Run with IPC error recovery
    model_success = False
    max_retries = 2
    for retry in range(max_retries):
        try:
            model_success = tester.process_model(
                model_path=model_dir,
                model_name=model_name,
                dxnn=dxnn,
                gt_dir=gt_dir,
                rt_dir=str(rt_dir),
                loops=args.loops, 
                compile_type=args.compile_type
            )
            result_data['success'] = model_success
            result_data['total_count'] = tester.stats.total_count 
            result_data['pass_count'] = tester.stats.pass_count
            result_data['failed_jobs'] = tester.stats.failed_jobs 
            result_data['model_result_messages'] = tester.model_result_messages
            break  # Success, exit retry loop

        except Exception as e:
            if _handle_ipc_error(model_name, str(e)) and retry < max_retries - 1:
                print(f"Worker (Iter {test_iteration_index}): Retrying model '{model_name}' after IPC error recovery (attempt {retry + 2}/{max_retries})...")
                # Cleanup and recreate tester
                try:
                    tester.shutdown()
                except:
                    pass
                time.sleep(2)
                tester = BitMatchTester(config)
                continue
            else:
                # Not an IPC error or final retry failed
                print(f"Worker (Iter {test_iteration_index}): Exception during processing model '{model_name}': {e}", file=sys.stderr)
                result_data['success'] = False
                result_data['model_result_messages'] = tester.model_result_messages
                break

    tester.shutdown()
    del tester
    gc.collect() 
    return result_data


def run_bitmatch_test_multiprocessing(args: argparse.Namespace) -> int:
    """
    Runs the bitmatch tests based on parsed command-line arguments.
    Uses multiprocessing for --dir mode.

    Args:
        args: An argparse.Namespace object containing the parsed arguments.

    Returns:
        An integer representing the exit code (0 for success, 1 for failure).
    """
    # Debug mode setting:
    # args.debug >= 1: Enable RT binary generation via DXRT_DEBUG_DATA
    # args.debug == 2: Enable debug analyzer for intermediate file analysis
    debug_value = args.debug if hasattr(args, 'debug') else 0
    os.environ['DXRT_DEBUG_DATA'] = "1" if debug_value >= 1 else "0"
    debug_mode = (debug_value == 2)

    # Optional: Call pcie_rescan if needed before tests
    # if hasattr(args, 'rescan_pcie') and args.rescan_pcie:
    #     pcie_rescan()

    failed_models_cumulative = []
    final_total_count_cumulative = 0
    final_pass_count_cumulative = 0
    all_model_result_messages = [] 


    # Load model filter names
    filtered_model_names = None
    if args.model_filter:
        if os.path.exists(args.model_filter):
            try:
                with open(args.model_filter, "r", encoding="utf-8") as file:
                    filtered_model_names = {line.strip() for line in file if line.strip() and not line.strip().startswith('#')}
            except Exception as e:
                 print(f"Warning: Error reading model filter file '{args.model_filter}': {e}. Filter will not be applied.", file=sys.stderr)
                 filtered_model_names = None
                 return 0
        else:
             print(f"Warning: Model filter file '{args.model_filter}' not found. Filter will not be applied.", file=sys.stderr)

    model_paths_to_process = []

    if args.dir:
        potential_model_dirs = [d for d in glob.glob(os.path.join(args.dir, "*")) if os.path.isdir(d)]
        if filtered_model_names is not None:
            model_paths_to_process = [d for d in potential_model_dirs if os.path.basename(d) in filtered_model_names]
        else:
            model_paths_to_process = potential_model_dirs

        if not model_paths_to_process:
             print(f"Warning: No model directories found in '{args.dir}' (or matching filter) to test.")
             return 0 

    elif args.model_path:
        model_paths_to_process = [args.model_path]
        if filtered_model_names is not None:
             model_name_to_check = os.path.basename(args.model_path.rstrip('/'))
             if model_name_to_check not in filtered_model_names:
                 print(f"Model '{model_name_to_check}' is excluded by the filter.")
                 return 0 

    else:
        print("Error: Please specify either --model_path or --dir argument to run tests.", file=sys.stderr)
        return 1 

    print(f"Processing {len(model_paths_to_process)} models...")

    exit_code = 0

    use_multiprocessing = bool(args.dir)

    if use_multiprocessing:
        num_processes = min(multiprocessing.cpu_count() - 1, 4)
        print(f"Using multiprocessing with {num_processes} worker processes.")
        if sys.platform != 'win32':
            multiprocessing.set_start_method('fork', force=True)

    else:
        print("Running in sequential mode.")

    for test_itr in range(args.test_iteration):
        print(f"\n--- Test Iteration {test_itr + 1}/{args.test_iteration} ---")

        iteration_results = [] 

        if use_multiprocessing:
            try:
                with multiprocessing.Pool(processes=num_processes) as pool:
                    # [(model_input_path, args, test_itr), (model_input_path2, args, test_itr), ...]
                    tasks = [(model_input_path, args, test_itr) for model_input_path in model_paths_to_process]

                    if TQDM_AVAILABLE:
                        print(f"Submitting {len(tasks)} tasks to the pool for iteration {test_itr + 1}...")
                        # Use imap with tqdm for progress tracking with PASS/FAIL counts
                        mp_progress = tqdm(
                            total=len(tasks),
                            desc=f"MP Iteration {test_itr + 1}/{args.test_iteration}",
                            unit="model",
                            position=1,
                            leave=True,
                            dynamic_ncols=True,
                            postfix={'PASS': 0, 'FAIL': 0}
                        )
                        
                        iteration_results = []
                        mp_pass_count = 0
                        mp_total_count = 0
                        
                        for result in pool.starmap(_process_single_model_task, tasks):
                            iteration_results.append(result)
                            mp_total_count += result.get('total_count', 0)
                            mp_pass_count += result.get('pass_count', 0)
                            
                            mp_progress.update(1)
                            mp_progress.set_postfix({
                                'PASS': mp_pass_count,
                                'FAIL': mp_total_count - mp_pass_count
                            })
                        
                        mp_progress.close()
                    else:
                        print(f"Submitting {len(tasks)} tasks to the pool for iteration {test_itr + 1}...")
                        iteration_results = pool.starmap(_process_single_model_task, tasks)
                        print(f"All tasks completed for iteration {test_itr + 1}.")

            except Exception as e:
                 print(f"Fatal Error during multiprocessing pool execution in iteration {test_itr + 1}: {e}", file=sys.stderr)
                 exit_code = 1 
                 break

        else: 
            # Sequential processing with progress bar
            if TQDM_AVAILABLE:
                model_iterator = tqdm(
                    model_paths_to_process,
                    desc=f"Seq Iteration {test_itr + 1}/{args.test_iteration}",
                    unit="model",
                    position=1,
                    leave=True,
                    dynamic_ncols=True,
                    postfix={'PASS': 0, 'FAIL': 0}
                )
            else:
                model_iterator = model_paths_to_process
            
            seq_pass_count = 0
            seq_total_count = 0
                
            for model_input_path in model_iterator:
                if not TQDM_AVAILABLE:
                    print(f"\nSequential Processing (Iter {test_itr + 1}): {model_input_path}")
                else:
                    tqdm.write(f"\nSequential Processing (Iter {test_itr + 1}): {model_input_path}")
                result = _process_single_model_task(model_input_path, args, test_itr)
                iteration_results.append(result)
                
                # Update progress bar with PASS/FAIL counts
                if TQDM_AVAILABLE and hasattr(model_iterator, 'set_postfix'):
                    seq_total_count += result.get('total_count', 0)
                    seq_pass_count += result.get('pass_count', 0)
                    model_iterator.set_postfix({
                        'PASS': seq_pass_count,
                        'FAIL': seq_total_count - seq_pass_count
                    })

        current_iteration_failed_models = []
        current_iteration_total_count = 0
        current_iteration_pass_count = 0

        for result in iteration_results:
            current_iteration_total_count += result.get('total_count', 0)
            current_iteration_pass_count += result.get('pass_count', 0)

            if not result.get('success', False):
                current_iteration_failed_models.append({
                    'test_itr': result.get('test_itr', 'N/A'),
                    'model': result.get('model_name', 'N/A'),
                    'model_dir': result.get('model_dir', 'N/A'),
                    'total_count': result.get('total_count', 0),
                    'fail_count': result.get('total_count', 0) - result.get('pass_count', 0),
                    'failed_jobs': result.get('failed_jobs', [])
                })
                if not result.get('success', False): 
                     exit_code = 1 

            all_model_result_messages.extend(result.get('model_result_messages', []))

        final_total_count_cumulative += current_iteration_total_count
        final_pass_count_cumulative += current_iteration_pass_count

        failed_models_cumulative.extend(current_iteration_failed_models)

    print(f"\nTotal tests across all iterations: {final_total_count_cumulative}, Passed: {final_pass_count_cumulative}, Failed models: {len(failed_models_cumulative)}")
    if failed_models_cumulative:
        print("Failed model details (across all iterations):")
        for fail_info in failed_models_cumulative:
             print(f"  Iteration {fail_info.get('test_itr', 'N/A')}: Model '{fail_info.get('model', 'N/A')}' failed ({fail_info.get('fail_count', 'N/A')}/{fail_info.get('total_count', 'N/A')} jobs failed)")
             # if fail_info.get('failed_jobs'):
             #      print(f"    Failed jobs: {fail_info['failed_jobs']}")

    print("\n---------------------------------------------------------------------------------------------------")
    print("Remember to use the `-p` option to measure maximum FPS performance excluding Bitmatch overhead.")
    print("---------------------------------------------------------------------------------------------------")

    logging_config_for_main_process = TestConfig(
        batch_mode=args.batch, 
        iterations=args.batch_iteration, 
        verbose=args.verbose,
        log_enabled=not args.no_logging, 
        performance_mode=args.performance,
        debug_mode=args.debug,
        use_ort=args.use_ort,
        npu_bound=InferenceOption.BOUND_OPTION(args.npu),
        devices=[int(d) for d in parse_devices_option(args.devices)],
        input_order=args.input_order,
        save_debug_report=args.save_debug_report
    )
  
    main_process_logger = BitMatchTester(logging_config_for_main_process)

    main_process_logger.model_result_messages = all_model_result_messages


    try:
        if args.dir:
            main_process_logger.log_all_results(
                final_pass_count_cumulative, 
                final_total_count_cumulative,
                failed_models_cumulative 
            )
    except Exception as e:
         print(f"Warning: Error during final results logging: {e}", file=sys.stderr)

    return exit_code
