import subprocess
import re
import numpy as np
import argparse
import time
from datetime import datetime
import os
import sys

def run_model_and_get_fps(model_dir, model_name, loop_count):
    """Runs the run_model command and extracts FPS, NPU time, and Latency."""
    model_path = os.path.join(model_dir, model_name, "*.dxnn")
    command = f"run_model -m {model_path} -l {loop_count} -v"
    print(f"Running command: {command}")
    try:
        process = subprocess.run(command, shell=True, capture_output=True, text=True, check=True)
        log = process.stdout
        #log_error = process.stderr

        fps_match = re.search(r"FPS\s*:\s*(\d+\.?\d*)", log)
        npu_time_match = re.search(r"NPU\s+Processing\s+Time\s+Average\s*:\s*(\d+\.?\d*)\s*ms", log)
        latency_match = re.search(r"Latency\s+Average\s*:\s*(\d+\.?\d*)\s*ms", log)

        if fps_match and npu_time_match and latency_match:
            return {
                'fps': float(fps_match.group(1)),
                'npu_time': float(npu_time_match.group(1)),
                'latency': float(latency_match.group(1))
            }, log

        elif fps_match:
            return {
                'fps': float(fps_match.group(1)),
                'npu_time': float(0),
                'latency': float(0)
            }, log
    except subprocess.CalledProcessError as e:
        print(f"Command failed with exit code {e.returncode}")
        print(f"Stderr: {e.stderr}")
        return None, f"Command failed: {command}\nStderr: {e.stderr}"
    except FileNotFoundError:
        print("Error: 'run_model' not found.")
        return None, f"Error: 'run_model' not found for command: {command}"
    except Exception as e:
        print(f"Unexpected error: {e}")
        return None, f"Unexpected error: {e} for command: {command}"

    return None, log


def calculate_target_loop(initial_fps, target_duration):
    if initial_fps is None or initial_fps <= 0:
        print("Warning: Initial FPS is zero or None. Using default loop count 100.")
        return 100
    return max(1, int(initial_fps * target_duration))

def analyze_logs(log_file_path):
    model_fps_values = {}
    model_loop_values = {}
    model_npu_values = {}
    model_latency_values = {}

    if not os.path.exists(log_file_path):
        print(f"Error: Log file '{log_file_path}' not found.")
        return {}

    with open(log_file_path, 'r') as f:
        for line in f:
            if not line.strip() or line.startswith("#"):
                continue
            parts = line.strip().split(', ')
            if len(parts) == 6:
                _, model_name, fps_str, npu_str, latency_str, loop_str = parts
                try:
                    fps = float(fps_str)
                    npu = float(npu_str)
                    latency = float(latency_str)
                    loop = int(loop_str)

                    model_fps_values.setdefault(model_name, []).append(fps)
                    model_npu_values.setdefault(model_name, []).append(npu)
                    model_latency_values.setdefault(model_name, []).append(latency)
                    model_loop_values.setdefault(model_name, []).append(loop)
                except ValueError:
                    continue

    results = {}
    for model in model_fps_values:
        results[model] = {
            'fps_mean': np.mean(model_fps_values[model]),
            'fps_std': np.std(model_fps_values[model]) if len(model_fps_values[model]) > 1 else 0,
            'npu_mean': np.mean(model_npu_values[model]),
            'npu_std': np.std(model_npu_values[model]) if len(model_npu_values[model]) > 1 else 0,
            'latency_mean': np.mean(model_latency_values[model]),
            'latency_std': np.std(model_latency_values[model]) if len(model_latency_values[model]) > 1 else 0,
            'loop_mean': np.mean(model_loop_values[model]),
            'loop_std': np.std(model_loop_values[model]) if len(model_loop_values[model]) > 1 else 0
        }
    return results

def get_dxrt_status():
    try:
        result = subprocess.run(['dxrt-cli', '-s'], capture_output=True, text=True, check=True)
        return result.stdout
    except Exception as e:
        return f"dxrt-cli error: {e}"

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-d", "--model_dir")
    parser.add_argument("-m", "--model_list_file")
    parser.add_argument("-t", "--target_duration", type=float, default=1.0)
    parser.add_argument("-n", "--num_iterations", type=int, default=1)
    parser.add_argument("-s", "--sleep_duration", type=float, default=0.0)
    parser.add_argument("-r", "--num_runs", type=int, default=5)
    parser.add_argument("--analyze_only")
    args = parser.parse_args()

    output_log_file = "fps_log.txt"
    analysis_log_file = "fps_analysis.csv"
    dxrt_s_log = get_dxrt_status()

    if args.analyze_only:
        results = analyze_logs(args.analyze_only)
        with open(analysis_log_file, 'w') as out:
            out.write("# dxrt-cli -s Output:\n")
            out.write(dxrt_s_log + "\n# Analysis Results:\n")
            out.write("Model,FPS Mean,FPS Std,NPU Time Mean,NPU Time Std,Latency Mean,Latency Std,Loop Mean,Loop Std\n")
            for model, stats in results.items():
                out.write(f"{model},{stats['fps_mean']:.6f},{stats['fps_std']:.6f},"
                          f"{stats['npu_mean']:.3f},{stats['npu_std']:.3f},"
                          f"{stats['latency_mean']:.3f},{stats['latency_std']:.3f},"
                          f"{stats['loop_mean']:.2f},{stats['loop_std']:.2f}\n")
        print(f"Analysis complete. Saved to {analysis_log_file}")
        sys.exit(0)

    if not args.model_dir:
        parser.error("-d is required unless --analyze_only is used.")

    if args.model_list_file:
        with open(args.model_list_file) as f:
            model_names = [line.strip() for line in f if line.strip() and not line.startswith("#")]
    else:
        model_names = [d for d in os.listdir(args.model_dir)
                       if os.path.isdir(os.path.join(args.model_dir, d))]

    if not model_names:
        print("No models found.")
        sys.exit(0)

    with open(output_log_file, 'w') as log_file:
        log_file.write(f"# Performance Measurement Log - {datetime.now()}\n")
        log_file.write("# Iteration, Model, FPS, NPU Time (ms), Latency (ms), Target Loop\n")
        for iteration in range(1, args.num_iterations + 1):
            for model in model_names:
                print(f"[{model}] Iteration {iteration}: Estimating FPS...")
                init_metrics, _ = run_model_and_get_fps(args.model_dir, model, 30)
                if not init_metrics:
                    log_file.write(f"{iteration}, {model}, Error, Error, Error, N/A\n")
                    continue
                target_loop = calculate_target_loop(init_metrics['fps'], args.target_duration)
                for run in range(args.num_runs):
                    metrics, _ = run_model_and_get_fps(args.model_dir, model, target_loop)
                    if metrics:
                        log_file.write(f"{iteration}, {model}, {metrics['fps']:.6f}, {metrics['npu_time']:.3f}, {metrics['latency']:.3f}, {target_loop}\n")
                    else:
                        log_file.write(f"{iteration}, {model}, Error, Error, Error, {target_loop}\n")
                    log_file.flush()
                if args.sleep_duration > 0:
                    time.sleep(args.sleep_duration)

    print(f"Finished logging to {output_log_file}")

    # Final analysis
    results = analyze_logs(output_log_file)
    with open(analysis_log_file, 'w') as out:
        out.write("# dxrt-cli -s Output:\n")
        out.write(dxrt_s_log + "\n# Analysis Results:\n")
        out.write("Model,FPS Mean,FPS Std,NPU Time Mean,NPU Time Std,Latency Mean,Latency Std,Loop Mean,Loop Std\n")
        for model, stats in results.items():
            out.write(f"{model},{stats['fps_mean']:.6f},{stats['fps_std']:.6f},"
                      f"{stats['npu_mean']:.3f},{stats['npu_std']:.3f},"
                      f"{stats['latency_mean']:.3f},{stats['latency_std']:.3f},"
                      f"{stats['loop_mean']:.2f},{stats['loop_std']:.2f}\n")

    print(f"Analysis complete. Saved to {analysis_log_file}")

### How to use:

#1. Put your models in folders inside the model directory.
#2. Run the script with:
#   ```bash
#   python3 your_script.py -d /path/to/model_dir -t 3.0 -n 2 -s 1.0 -r 5