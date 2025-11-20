#!/usr/bin/env python3
"""
Python validation test application for DXRT
Ported from C++ version
"""

import argparse
import sys
import os
from pathlib import Path

# Import custom modules
from generator import Generator
from test_manager import TestManager

# DXRT Python bindings
from dx_engine import InferenceEngine

def main():
    parser = argparse.ArgumentParser(description='DXRT Validation Test')
    parser.add_argument('-b', '--base-path', required=True,
                        help='Base path for test files')
    parser.add_argument('-j', '--json-file', required=True,
                        help='JSON configuration file path')
    parser.add_argument('-r', '--result-name', default=None,
                        help='Result file name/path (optional - if not provided, no report file will be generated)')
    parser.add_argument('-v', '--verbose', type=int, default=1,
                        help='Verbose level (0: silent, 1: normal, 2: detailed)')
    parser.add_argument('-l', '--log-level', type=int, default=0,
                        help='Log level: 0=failed only, 1=show progress, 2=include skipped, 3=all, 4=debug')
    parser.add_argument('--tmp-model', required=True,
                        help='Temporary model path for workaround, any model is acceptable')
    parser.add_argument('--random', action='store_true',
                        help='Randomize test case generation')

    try:
        args = parser.parse_args()
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

    # Validate required arguments
    if not args.base_path:
        print("Error: base-path is required")
        sys.exit(1)
    
    if not args.json_file:
        print("Error: json-file is required")
        sys.exit(1)
    
    if not args.tmp_model:
        print("Error: tmp-model is required")
        sys.exit(1)

    # Check if paths exist
    if not os.path.exists(args.base_path):
        print("Error: base path does not exist")
        sys.exit(1)

    if not os.path.exists(args.json_file):
        print("Error: json file does not exist")
        sys.exit(1)

    if not os.path.exists(args.tmp_model):
        print("Error: temporary model file does not exist")
        sys.exit(1)

    # Create generator
    generator = Generator(args.base_path, args.json_file, args.random)

    # Load and parse JSON file
    if not generator.load_json():
        print("Error: Failed to load or parse JSON file")
        sys.exit(1)

    # Temporal Error Handling: GetDeviceCount() -> IE Problem (equivalent to C++ version)
    try:
        ie2 = InferenceEngine(args.tmp_model)
        del ie2  # Immediately destroy the temporary IE
    except Exception as e:
        print(f"Error: Failed to create temporary IE: {e}")
        sys.exit(1)

    # Generate test cases
    generator.generate_test_cases()
    test_cases = generator.get_test_cases()
    generator.check_for_duplicates()

    # Create and run TestManager
    test_manager = TestManager(test_cases, args.verbose, args.log_level, args.result_name)
    test_manager.run()
    
    # Generate report only if result-name is provided
    if args.result_name:
        test_manager.make_report()

if __name__ == "__main__":
    main()