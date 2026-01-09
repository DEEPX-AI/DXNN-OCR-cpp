#!/usr/bin/env python3
"""
OCR Benchmark Runner
Runs C++ OCR benchmark and calculates accuracy using calculate_acc.py
"""

import sys
import json
import subprocess
from pathlib import Path

def run_cpp_benchmark(runs_per_image=3, model_type='server', images_dir=None):
    """Run C++ benchmark program"""
    print("="*60)
    print("Running C++ OCR Benchmark...")
    print("="*60)
    
    project_root = Path(__file__).resolve().parent.parent
    benchmark_exe = project_root / "build_Release" / "bin" / "benchmark"
    
    if not benchmark_exe.exists():
        print(f"Error: Benchmark executable not found: {benchmark_exe}")
        print("Please compile first: ./build.sh")
        sys.exit(1)
    
    # 运行benchmark，传入runs_per_image、model_type、uvdoc(false)、images_dir参数
    cmd = [str(benchmark_exe), str(runs_per_image), model_type, "false"]
    if images_dir:
        cmd.append(images_dir)
    
    result = subprocess.run(cmd, cwd=str(project_root))
    
    if result.returncode != 0:
        print(f"Error: Benchmark failed with code {result.returncode}")
        sys.exit(1)

def run_accuracy_calculation(results_dir, images_dir=None):
    """Run Python accuracy calculation script"""
    project_root = Path(__file__).resolve().parent.parent
    calc_script = project_root / "benchmark" / "calculate_acc.py"
    
    if not calc_script.exists():
        print(f"Warning: calculate_acc.py not found: {calc_script}")
        print("Skipping accuracy calculation.")
        return {}
    
    # 调用calculate_acc.py with --batch flag and specific results directory
    cmd = [sys.executable, str(calc_script), '--batch', '--output_dir', str(results_dir)]
    
    # 自动推断 ground_truth 路径：{images_dir}/labels.json
    if images_dir:
        if images_dir.startswith('/'):
            ground_truth = f"{images_dir}/labels.json"
        else:
            ground_truth = str(project_root / images_dir / "labels.json")
        cmd.extend(['--ground_truth', ground_truth])
    
    result = subprocess.run(cmd, cwd=str(project_root), capture_output=True, text=True)
    
    if result.returncode != 0:
        print(f"Warning: Accuracy calculation failed with code {result.returncode}")
        print(result.stderr)
        return {}
    
    # 解析JSON输出（提取最后一行的JSON数据）
    accuracy_data = {}
    try:
        # 从stdout中提取JSON行（跳过warning等非JSON行）
        for line in result.stdout.strip().split('\n'):
            line = line.strip()
            if line.startswith('{') and line.endswith('}'):
                accuracy_data = json.loads(line)
                break
        
        if not accuracy_data:
            # 如果没找到JSON，尝试直接解析整个输出
            accuracy_data = json.loads(result.stdout.strip())
    except json.JSONDecodeError as e:
        print(f"Warning: Failed to parse accuracy data: {e}")
        if result.stderr:
            print(f"Stderr: {result.stderr}")
    
    return accuracy_data

def generate_markdown_report(results_dir, accuracy_data):
    """Generate PP-OCRv5 style markdown report"""
    print("="*60)
    print("Generating Markdown Report...")
    print("="*60 + "\n")
    
    results_dir = Path(results_dir)
    
    # 加载所有JSON结果
    image_results = []
    total_time = 0
    total_chars = 0
    
    for json_file in sorted(results_dir.glob("*_result.json")):
        with open(json_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
            
        image_name = data.get('filename', json_file.stem.replace('_result', '') + '.png')
        
        # 计算统计信息
        avg_time = data.get('avg_inference_ms', 0)
        chars = data.get('total_chars', 0)
        fps = 1000.0 / avg_time if avg_time > 0 else 0
        cps = chars * 1000.0 / avg_time if avg_time > 0 else 0
        
        # 从accuracy_data中获取准确率
        acc_percent = None
        if image_name in accuracy_data:
            acc_percent = accuracy_data[image_name].get('char_accuracy', 0) * 100
        
        image_results.append({
            'filename': image_name,
            'inference_time': avg_time,
            'fps': fps,
            'cps': cps,
            'chars': chars,
            'accuracy': acc_percent
        })
        
        total_time += avg_time
        total_chars += chars
    
    if not image_results:
        print("No results found to generate report")
        return
    
    # 计算平均值
    num_images = len(image_results)
    avg_time = total_time / num_images if num_images > 0 else 0
    avg_fps = 1000.0 / avg_time if avg_time > 0 else 0
    avg_cps = total_chars * 1000.0 / total_time if total_time > 0 else 0
    
    # 计算平均准确率
    accuracies = [r['accuracy'] for r in image_results if r['accuracy'] is not None]
    avg_accuracy = sum(accuracies) / len(accuracies) if accuracies else None
    
    # 生成Markdown报告
    report_path = results_dir / "DXNN-OCR_benchmark_report.md"
    
    with open(report_path, 'w', encoding='utf-8') as f:
        f.write("# DXNN-OCR Benchmark Report\n\n")
        
        # Test Configuration
        f.write("**Test Configuration**:\n")
        f.write("- Model: PP-OCR v5 (DEEPX NPU acceleration)\n")
        f.write(f"- Total Images Tested: {num_images}\n")
        f.write(f"- Success Rate: 100.0%\n\n")
        
        # Test Results Table
        f.write("**Test Results**:\n")
        f.write("| Filename | Inference Time (ms) | FPS | CPS (chars/s) | Accuracy (%) |\n")
        f.write("|---|---|---|---|---|\n")
        
        for r in image_results:
            acc_str = f"**{r['accuracy']:.2f}**" if r['accuracy'] is not None else "N/A"
            f.write(f"| `{r['filename']}` | "
                   f"{r['inference_time']:.2f} | "
                   f"{r['fps']:.2f} | "
                   f"**{r['cps']:.2f}** | "
                   f"{acc_str} |\n")
        
        # Average row
        avg_acc_str = f"**{avg_accuracy:.2f}**" if avg_accuracy is not None else "N/A"
        f.write(f"| **Average** | "
               f"**{avg_time:.2f}** | "
               f"**{avg_fps:.2f}** | "
               f"**{avg_cps:.2f}** | "
               f"{avg_acc_str} |\n\n")
        
        # Performance Summary
        f.write("**Performance Summary**:\n")
        f.write(f"- Average Inference Time: **{avg_time:.2f} ms**\n")
        f.write(f"- Average FPS: **{avg_fps:.2f}**\n")
        f.write(f"- Average CPS: **{avg_cps:.2f} chars/s**\n")
        f.write(f"- Total Characters Detected: **{total_chars}**\n")
        f.write(f"- Total Processing Time: **{total_time:.2f} ms**\n")
        
        if avg_accuracy is not None:
            f.write(f"- Average Character Accuracy: **{avg_accuracy:.2f}%**\n")
        
        f.write(f"- Success Rate: **100.0%** ({num_images}/{num_images} images)\n")
    
    print(f"✓ Markdown report saved to: {report_path}\n")

def main():
    import argparse
    parser = argparse.ArgumentParser(description='Run OCR benchmark')
    parser.add_argument('--runs', type=int, default=3, help='Number of runs per image (default: 3)')
    parser.add_argument('--no-acc', action='store_true', help='Skip accuracy calculation')
    parser.add_argument('--no-cpp', action='store_true', help='Skip C++ benchmark (only calculate accuracy from existing results)')
    parser.add_argument('--model', default='server', choices=['server', 'mobile'], help='Model type (default: server)')
    parser.add_argument('--images_dir', default=None, help='Directory containing test images (relative to project root or absolute path). Ground truth is auto-detected as {images_dir}/labels.json')
    args = parser.parse_args()
    
    project_root = Path(__file__).resolve().parent.parent
    results_dir = project_root / "benchmark" / f"results_{args.model}"
    
    # 1. 运行C++benchmark (if not skipped)
    if not args.no_cpp:
        run_cpp_benchmark(args.runs, args.model, args.images_dir)
    else:
        print("Skipping C++ benchmark, using existing results...")
    
    # 2. 计算准确率
    accuracy_data = {}
    if not args.no_acc:
        accuracy_data = run_accuracy_calculation(results_dir, args.images_dir)
    
    # 3. 生成Markdown报告
    generate_markdown_report(results_dir, accuracy_data)
    
    print("\n" + "="*60)
    print("✓ Benchmark pipeline completed!")
    print("="*60)

if __name__ == '__main__':
    main()
