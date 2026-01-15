#!/usr/bin/env python3
"""
OCR API Server Benchmark Runner
运行 API 基准测试并生成 Markdown 报告

参考 benchmark/run_benchmark.py 的输出格式，包含:
- 每张图片的推理时间、FPS、CPS
- 准确率计算（如果提供 labels.json）
"""

import sys
import json
import argparse
import time
import requests
import base64
import concurrent.futures
from pathlib import Path
from datetime import datetime
from statistics import mean, stdev
import threading
import difflib

# 默认测试图片 (1x1 红色 PNG)
DEFAULT_TEST_IMAGE = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8DwHwAFBQIAX8jx0gAAAABJRU5ErkJggg=="


def load_images_from_directory(images_dir: str) -> list:
    """从目录加载图片为 Base64"""
    images = []
    path = Path(images_dir)
    
    if not path.exists():
        print(f"Warning: Directory not found: {images_dir}")
        return images
    
    for image_file in sorted(path.glob("*.png")) + sorted(path.glob("*.jpg")) + sorted(path.glob("*.jpeg")):
        try:
            with open(image_file, "rb") as f:
                image_data = f.read()
                base64_data = base64.b64encode(image_data).decode("utf-8")
                images.append({
                    "filename": image_file.name,
                    "base64": base64_data
                })
                print(f"Loaded: {image_file.name}")
        except Exception as e:
            print(f"Error loading {image_file}: {e}")
    
    return images


def load_ground_truth(images_dir: str) -> dict:
    """加载 ground truth 标签"""
    labels_path = Path(images_dir) / "labels.json"
    if not labels_path.exists():
        return {}
    
    try:
        with open(labels_path, "r", encoding="utf-8") as f:
            raw_labels = json.load(f)
        
        # 将列表格式转换为合并文本
        # 格式: {"image.png": [{"text": "...", "bbox": [...]}, ...]}
        ground_truth = {}
        for filename, boxes in raw_labels.items():
            if isinstance(boxes, list):
                # 合并所有文本框的文本
                texts = [box.get("text", "") for box in boxes if isinstance(box, dict)]
                ground_truth[filename] = "".join(texts)
            elif isinstance(boxes, str):
                ground_truth[filename] = boxes
        
        return ground_truth
    except Exception as e:
        print(f"Warning: Failed to load labels.json: {e}")
        return {}


def calculate_char_accuracy(predicted: str, ground_truth: str) -> float:
    """计算字符级准确率"""
    if not ground_truth:
        return None
    if not predicted:
        return 0.0
    
    # 使用 difflib 计算相似度
    matcher = difflib.SequenceMatcher(None, predicted, ground_truth)
    return matcher.ratio() * 100


def send_ocr_request(url: str, token: str, image_base64: str, params: dict = None) -> dict:
    """发送 OCR 请求，返回详细结果"""
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"token {token}"
    }
    
    payload = {
        "file": image_base64,
        "fileType": 1,
        "useDocOrientationClassify": params.get("useDocOrientationClassify", False) if params else False,
        "useDocUnwarping": params.get("useDocUnwarping", False) if params else False,
        "textDetThresh": params.get("textDetThresh", 0.3) if params else 0.3,
        "textDetBoxThresh": params.get("textDetBoxThresh", 0.6) if params else 0.6,
        "textDetUnclipRatio": params.get("textDetUnclipRatio", 1.5) if params else 1.5,
        "textRecScoreThresh": params.get("textRecScoreThresh", 0.0) if params else 0.0,
        "visualize": params.get("visualize", False) if params else False,
    }
    
    start_time = time.time()
    
    try:
        response = requests.post(url, headers=headers, json=payload, timeout=60)
        latency = (time.time() - start_time) * 1000  # ms
        
        result = {
            "success": response.status_code == 200,
            "http_code": response.status_code,
            "latency_ms": latency,
            "error_msg": "",
            "text": "",
            "char_count": 0,
            "ocr_results": []
        }
        
        if response.status_code == 200:
            try:
                json_response = response.json()
                if json_response.get("errorCode", -1) != 0:
                    result["success"] = False
                    result["error_msg"] = json_response.get("errorMsg", "Unknown error")
                else:
                    # 提取 OCR 结果
                    ocr_results = json_response.get("result", {}).get("ocrResults", [])
                    result["ocr_results"] = ocr_results
                    
                    # 合并所有识别文本
                    texts = [r.get("prunedResult", "") for r in ocr_results]
                    result["text"] = "".join(texts)
                    result["char_count"] = len(result["text"])
                    
            except json.JSONDecodeError:
                result["success"] = False
                result["error_msg"] = "Invalid JSON response"
        else:
            result["error_msg"] = f"HTTP {response.status_code}"
        
        return result
        
    except requests.exceptions.Timeout:
        return {
            "success": False,
            "http_code": 0,
            "latency_ms": 60000,
            "error_msg": "Timeout",
            "text": "",
            "char_count": 0,
            "ocr_results": []
        }
    except requests.exceptions.RequestException as e:
        return {
            "success": False,
            "http_code": 0,
            "latency_ms": 0,
            "error_msg": str(e),
            "text": "",
            "char_count": 0,
            "ocr_results": []
        }


def run_benchmark(url: str, token: str, images: list, ground_truth: dict, runs_per_image: int = 1, concurrency: int = 1) -> dict:
    """运行基准测试，返回每张图片的详细结果
    
    Args:
        concurrency: 并发数
            - 1: 串行模式，逐张图片测试（测量单请求延迟）
            - >1: 并发模式，同时发送多个请求（测量吞吐量）
    """
    print("\n" + "=" * 60)
    print("Starting API Benchmark (Python)")
    print("=" * 60)
    print(f"Server URL: {url}")
    print(f"Total Images: {len(images)}")
    print(f"Runs per Image: {runs_per_image}")
    print(f"Concurrency: {concurrency}")
    print(f"Ground Truth: {'Available' if ground_truth else 'Not available'}")
    print("=" * 60 + "\n")
    
    image_results = []
    total_chars = 0
    total_time = 0
    successful = 0
    
    if concurrency > 1:
        # ========== 并发模式 ==========
        print(f"[并发模式] 同时发送 {concurrency} 个请求...\n")
        
        all_results = []
        lock = threading.Lock()
        completed = [0]
        total_tasks = len(images) * runs_per_image
        
        def worker(image, run_idx):
            filename = image["filename"] if isinstance(image, dict) else "unknown"
            image_base64 = image["base64"] if isinstance(image, dict) else image
            result = send_ocr_request(url, token, image_base64)
            result["filename"] = filename
            result["run_idx"] = run_idx
            
            with lock:
                all_results.append(result)
                completed[0] += 1
                if completed[0] % 10 == 0 or completed[0] == total_tasks:
                    print(f"\rProgress: {completed[0]}/{total_tasks} ({completed[0] * 100 // total_tasks}%)", end="", flush=True)
            
            return result
        
        # 构建任务列表
        tasks = []
        for image in images:
            for run_idx in range(runs_per_image):
                tasks.append((image, run_idx))
        
        # 并发执行
        benchmark_start = time.time()
        with concurrent.futures.ThreadPoolExecutor(max_workers=concurrency) as executor:
            futures = [executor.submit(worker, img, run) for img, run in tasks]
            concurrent.futures.wait(futures)
        benchmark_duration = (time.time() - benchmark_start) * 1000
        print()
        
        # 按图片聚合结果
        results_by_image = {}
        for r in all_results:
            fn = r["filename"]
            if fn not in results_by_image:
                results_by_image[fn] = []
            results_by_image[fn].append(r)
        
        for image in images:
            filename = image["filename"] if isinstance(image, dict) else "unknown"
            img_results = results_by_image.get(filename, [])
            
            latencies = [r["latency_ms"] for r in img_results if r["success"]]
            if latencies:
                avg_latency = mean(latencies)
                char_count = img_results[0]["char_count"] if img_results else 0
                text = img_results[0]["text"] if img_results else ""
                fps = 1000.0 / avg_latency if avg_latency > 0 else 0
                cps = char_count * 1000.0 / avg_latency if avg_latency > 0 else 0
                
                accuracy = None
                if filename in ground_truth:
                    accuracy = calculate_char_accuracy(text, ground_truth[filename])
                
                image_results.append({
                    "filename": filename,
                    "latency_ms": avg_latency,
                    "fps": fps,
                    "cps": cps,
                    "char_count": char_count,
                    "accuracy": accuracy,
                    "text": text
                })
                
                total_chars += char_count
                total_time += avg_latency
                successful += 1
                
                acc_str = f"{accuracy:.2f}%" if accuracy is not None else "N/A"
                print(f"  {filename}: {avg_latency:.2f}ms, {char_count} chars, CPS={cps:.2f}, Acc={acc_str}")
            else:
                print(f"  {filename}: FAILED")
                image_results.append({
                    "filename": filename, "latency_ms": 0, "fps": 0, "cps": 0,
                    "char_count": 0, "accuracy": None, "text": ""
                })
        
        # 计算并发 QPS
        total_successful = sum(1 for r in all_results if r["success"])
        concurrent_qps = total_successful * 1000.0 / benchmark_duration if benchmark_duration > 0 else 0
        print(f"\n[并发统计] 总耗时: {benchmark_duration:.2f}ms, 成功: {total_successful}, QPS: {concurrent_qps:.2f}")
        
    else:
        # ========== 串行模式 ==========
        for idx, image in enumerate(images):
            filename = image["filename"] if isinstance(image, dict) else f"image_{idx}.png"
            image_base64 = image["base64"] if isinstance(image, dict) else image
            
            # 多次运行取平均
            latencies = []
            char_count = 0
            text = ""
            
            for run in range(runs_per_image):
                result = send_ocr_request(url, token, image_base64)
                if result["success"]:
                    latencies.append(result["latency_ms"])
                    char_count = result["char_count"]
                    text = result["text"]
            
            if latencies:
                avg_latency = mean(latencies)
                fps = 1000.0 / avg_latency if avg_latency > 0 else 0
                cps = char_count * 1000.0 / avg_latency if avg_latency > 0 else 0
                
                # 计算准确率
                accuracy = None
                if filename in ground_truth:
                    gt_text = ground_truth[filename]
                    accuracy = calculate_char_accuracy(text, gt_text)
                
                image_results.append({
                    "filename": filename,
                    "latency_ms": avg_latency,
                    "fps": fps,
                    "cps": cps,
                    "char_count": char_count,
                    "accuracy": accuracy,
                    "text": text
                })
                
                total_chars += char_count
                total_time += avg_latency
                successful += 1
                
                # 打印进度
                acc_str = f"{accuracy:.2f}%" if accuracy is not None else "N/A"
                print(f"[{idx+1}/{len(images)}] {filename}: {avg_latency:.2f}ms, {char_count} chars, CPS={cps:.2f}, Acc={acc_str}")
            else:
                print(f"[{idx+1}/{len(images)}] {filename}: FAILED")
                image_results.append({
                    "filename": filename,
                    "latency_ms": 0,
                    "fps": 0,
                    "cps": 0,
                    "char_count": 0,
                    "accuracy": None,
                    "text": ""
                })
    
    # 计算汇总统计
    valid_results = [r for r in image_results if r["latency_ms"] > 0]
    
    if valid_results:
        avg_latency = mean([r["latency_ms"] for r in valid_results])
        avg_fps = 1000.0 / avg_latency if avg_latency > 0 else 0
        avg_cps = total_chars * 1000.0 / total_time if total_time > 0 else 0
        
        accuracies = [r["accuracy"] for r in valid_results if r["accuracy"] is not None]
        avg_accuracy = mean(accuracies) if accuracies else None
    else:
        avg_latency = avg_fps = avg_cps = 0
        avg_accuracy = None
    
    stats = {
        "total_images": len(images),
        "successful_images": successful,
        "failed_images": len(images) - successful,
        "success_rate": successful * 100.0 / len(images) if images else 0,
        "total_time_ms": total_time,
        "total_chars": total_chars,
        "avg_latency_ms": avg_latency,
        "avg_fps": avg_fps,
        "avg_cps": avg_cps,
        "avg_accuracy": avg_accuracy,
        "image_results": image_results,
        "runs_per_image": runs_per_image,
        "concurrency": concurrency
    }
    
    # 计算延迟分布
    if valid_results:
        latencies = sorted([r["latency_ms"] for r in valid_results])
        stats["min_latency_ms"] = min(latencies)
        stats["max_latency_ms"] = max(latencies)
        stats["p50_latency_ms"] = latencies[len(latencies) * 50 // 100] if latencies else 0
        stats["p90_latency_ms"] = latencies[len(latencies) * 90 // 100] if latencies else 0
        stats["p99_latency_ms"] = latencies[min(len(latencies) - 1, len(latencies) * 99 // 100)] if latencies else 0
        stats["qps"] = 1000.0 / avg_latency if avg_latency > 0 else 0
    
    return stats


def print_results(stats: dict):
    """打印结果"""
    print("\n" + "=" * 60)
    print("Benchmark Results Summary")
    print("=" * 60)
    print(f"Total Images:      {stats['total_images']}")
    print(f"Successful:        {stats['successful_images']}")
    print(f"Failed:            {stats['failed_images']}")
    print(f"Success Rate:      {stats['success_rate']:.2f}%")
    print("-" * 40)
    print(f"Total Time:        {stats['total_time_ms']:.2f} ms")
    print(f"Total Characters:  {stats['total_chars']}")
    print("-" * 40)
    print(f"Avg Latency:       {stats['avg_latency_ms']:.2f} ms")
    print(f"Avg FPS:           {stats['avg_fps']:.2f}")
    print(f"Avg CPS:           {stats['avg_cps']:.2f} chars/s")
    if stats.get('avg_accuracy') is not None:
        print(f"Avg Accuracy:      {stats['avg_accuracy']:.2f}%")
    print("=" * 60)


def generate_markdown_report(stats: dict, output_dir: str):
    """生成 Markdown 报告并打印到终端（参考原 benchmark 格式）"""
    report_path = Path(output_dir) / "API_benchmark_report.md"
    
    image_results = stats.get("image_results", [])
    
    # 构建报告内容
    lines = []
    lines.append("# DXNN-OCR API Server Benchmark Report\n")
    
    # Test Configuration
    lines.append("**Test Configuration**:")
    lines.append("- Model: PP-OCR v5 (DEEPX NPU acceleration)")
    lines.append(f"- Total Images Tested: {stats['total_images']}")
    lines.append(f"- Runs per Image: {stats.get('runs_per_image', 1)}")
    lines.append(f"- Success Rate: {stats['success_rate']:.1f}%\n")
    
    # Test Results Table
    lines.append("**Test Results**:")
    lines.append("| Filename | Inference Time (ms) | FPS | CPS (chars/s) | Accuracy (%) |")
    lines.append("|---|---|---|---|---|")
    
    for r in image_results:
        acc_str = f"**{r['accuracy']:.2f}**" if r['accuracy'] is not None else "N/A"
        lines.append(f"| `{r['filename']}` | {r['latency_ms']:.2f} | {r['fps']:.2f} | **{r['cps']:.2f}** | {acc_str} |")
    
    # Average row
    avg_acc_str = f"**{stats['avg_accuracy']:.2f}**" if stats.get('avg_accuracy') is not None else "N/A"
    lines.append(f"| **Average** | **{stats['avg_latency_ms']:.2f}** | **{stats['avg_fps']:.2f}** | **{stats['avg_cps']:.2f}** | {avg_acc_str} |\n")
    
    # Performance Summary
    lines.append("**Performance Summary**:")
    lines.append(f"- Average Inference Time: **{stats['avg_latency_ms']:.2f} ms**")
    lines.append(f"- Average FPS: **{stats['avg_fps']:.2f}**")
    lines.append(f"- Average CPS: **{stats['avg_cps']:.2f} chars/s**")
    lines.append(f"- Total Characters Detected: **{stats['total_chars']}**")
    lines.append(f"- Total Processing Time: **{stats['total_time_ms']:.2f} ms**")
    if stats.get('avg_accuracy') is not None:
        lines.append(f"- Average Character Accuracy: **{stats['avg_accuracy']:.2f}%**")
    lines.append(f"- Success Rate: **{stats['success_rate']:.1f}%** ({stats['successful_images']}/{stats['total_images']} images)")
    
    report_content = "\n".join(lines)
    
    # 打印到终端
    print("\n" + "=" * 60)
    print("MARKDOWN REPORT")
    print("=" * 60)
    print(report_content)
    print("=" * 60)
    
    # 保存到文件
    with open(report_path, "w", encoding="utf-8") as f:
        f.write(report_content)
    
    print(f"\n✓ Markdown report saved to: {report_path}")


def save_json_results(stats: dict, output_file: str):
    """保存 JSON 结果"""
    # 移除 text 字段以减小文件大小
    stats_copy = stats.copy()
    if "image_results" in stats_copy:
        stats_copy["image_results"] = [
            {k: v for k, v in r.items() if k != "text"}
            for r in stats_copy["image_results"]
        ]
    
    with open(output_file, "w") as f:
        json.dump(stats_copy, f, indent=4, ensure_ascii=False)
    print(f"✓ JSON results saved to: {output_file}")


def main():
    parser = argparse.ArgumentParser(description="OCR API Server Benchmark")
    parser.add_argument("-u", "--url", default="http://localhost:8080/ocr",
                        help="Server URL (default: http://localhost:8080/ocr)")
    parser.add_argument("-t", "--token", default="test_token",
                        help="Authorization token (default: test_token)")
    parser.add_argument("-r", "--runs", type=int, default=1,
                        help="Number of runs per image (default: 1)")
    parser.add_argument("-c", "--concurrency", type=int, default=1,
                        help="Number of concurrent workers (default: 1)")
    parser.add_argument("-i", "--images", default="",
                        help="Directory containing test images (with optional labels.json)")
    parser.add_argument("-o", "--output", default="api_benchmark_results.json",
                        help="Output JSON file (default: api_benchmark_results.json)")
    parser.add_argument("--report-dir", default=".",
                        help="Directory for Markdown report (default: current directory)")
    parser.add_argument("--no-report", action="store_true",
                        help="Skip Markdown report generation")
    
    # 兼容旧参数
    parser.add_argument("-n", "--requests", type=int, default=0,
                        help="(Deprecated) Use -r/--runs instead")
    
    args = parser.parse_args()
    
    # 兼容旧的 -n 参数
    runs_per_image = args.runs
    if args.requests > 0 and args.runs == 1:
        # 如果用户使用了 -n 但没有用 -r，假设他们想测试多次
        runs_per_image = 1  # 每张图只测一次，但可能有多张图
    
    # 加载图片
    if args.images:
        images = load_images_from_directory(args.images)
        ground_truth = load_ground_truth(args.images)
    else:
        images = [{"filename": "default.png", "base64": DEFAULT_TEST_IMAGE}]
        ground_truth = {}
    
    if not images:
        print("Error: No images loaded")
        sys.exit(1)
    
    # 运行基准测试
    stats = run_benchmark(args.url, args.token, images, ground_truth, runs_per_image, args.concurrency)
    
    # 输出结果
    print_results(stats)
    save_json_results(stats, args.output)
    
    if not args.no_report:
        Path(args.report_dir).mkdir(parents=True, exist_ok=True)
        generate_markdown_report(stats, args.report_dir)
    
    print("\n" + "=" * 60)
    print("✓ Benchmark completed!")
    print("=" * 60)


if __name__ == "__main__":
    main()
