#
# Copyright (C) 2018- DEEPX Ltd.
# All rights reserved.
#
# This software is the property of DEEPX and is provided exclusively to customers 
# who are supplied with DEEPX NPU (Neural Processing Unit). 
# Unauthorized sharing or usage is strictly prohibited by law.
#

import argparse
import sys

try:
    from dx_engine.bitmatch.core import run_bitmatch_test, run_bitmatch_test_multiprocessing
except ImportError as e:
    print(e)
    print("Error: 'dx_engine.bitmatch' subpackage not found.")
    print("Please install the package first using 'pip install .' from the project root.")
    sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="Bitmatch Test Script")
    parser.add_argument("--model_path", "-m", "--singlefile", "--model_file", "--model", dest="model_path",type=str, help=".dxnn file or Model directory with .dxnn and gt")
    parser.add_argument("--gt_dir", type=str, default="gt", help="directory of ground truth binary data")
    parser.add_argument("--rt_dir", type=str, default="rt", help="directory of dx_rt output binary data")
    parser.add_argument("--loops", "-l", type=int, default=1, help="Number of test loops")
    parser.add_argument("--reg_f", "-f", "--dir", "-d", type=str, dest="dir",
                        help="Regression directory with model subdirectories (ex: dxnn_regr_data/M1A/1000)")
    parser.add_argument("--sync", "-s", action='store_true', help="Enable sync mode")
    parser.add_argument("--batch", "-b", action='store_true', help="Enable batch mode")
    parser.add_argument("--batch_iteration", "-bi", type=int, default=1, help="Batch iterations")
    parser.add_argument("--test_iteration", "-ti", type=int, default=1, help="Test iterations")
    parser.add_argument("--verbose", "-v", action='store_true', help="Enable verbose output")
    parser.add_argument("--no-logging", action='store_true', help="Disable file logging")
    parser.add_argument("--debug", type=int, required=False, 
                        help="RT debug options:\n"
                             "  0 (default): Do not generate RT output binaries\n"
                             "  1: Generate RT output binaries only (set DXRT_DEBUG_DATA=1)\n"
                             "  2: Generate RT output binaries + run debug analyzer for intermediate analysis", 
                        default=0, choices=[0, 1, 2])
    parser.add_argument("--model_filter", type=str, required=False, help="Path to model_filter.txt")  
    parser.add_argument("--compile_type", type=str, choices=["RELEASE", "DEBUG"], required=False, help=".dxnn compile type (RELEASE, DEBUG)")
    parser.add_argument("--performance", "-p", action='store_true', help="Enable performance mode")
    parser.add_argument("--multi_processing", "-mp", action='store_true', help="Enable multi_processing mode")
    parser.add_argument("--use-ort", action='store_true', help="use ONNX Runtime")
    parser.add_argument("--npu", "-n", type=int, default=0, help="NPU bounding (default:0)\n"
                                                                " - Bounding value 0 : inference with all NPU\n"
                                                                " - Bounding value 1 : inference with NPU0\n"
                                                                " - Bounding value 2 : inference with NPU1\n"
                                                                " - Bounding value 3 : inference with NPU2\n"
                                                                " - Bounding value 4 : inference with NPU0/1\n"
                                                                " - Bounding value 5 : inference with NPU1/2\n"
                                                                " - Bounding value 6 : inference with NPU0/2",)
    parser.add_argument("--devices", type=str, default="all", help="Specify target NPU devices (default: 'all'). Examples:\n"
                                                                "  'all': Use all available/bound NPUs.\n"
                                                                "  '0': Use NPU0 only.\n"
                                                                "  '0,1,2': Use NPU0, NPU1, and NPU2.\n"
                                                                "  'count:N': Use the first N NPUs (e.g., 'count:2' for NPU0, NPU1).")
    parser.add_argument("--input-order", type=str, choices=["random", "sequential"], default="sequential",
                        help="Input index selection order (default: sequential)\n"
                            "  - random: Select input indices randomly\n"
                            "  - sequential: Select input indices sequentially in order")
    parser.add_argument("--save_debug_report", action='store_true', help="Save detailed debug analysis report to JSON file (default: False, only show summary)")
    args = parser.parse_args()
    
    if args.multi_processing:
        result_code = run_bitmatch_test_multiprocessing(args)
    else:
        result_code = run_bitmatch_test(args)


    sys.exit(result_code)

if __name__ == "__main__":
    main()
