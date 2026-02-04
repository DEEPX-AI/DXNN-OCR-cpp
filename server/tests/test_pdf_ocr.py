#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PDF OCR 自动化测试脚本

功能：
- 自动扫描 pdf_file/ 目录中的所有 PDF 文件
- 将 PDF 转换为 Base64 编码
- 自动生成 JSON 请求并发送到 OCR 服务器
- 自动设置 maxPages 以完整识别所有页面
- 将 OCR 结果保存到 result/ 目录

使用方法：
    python3 pdf_ocr_test.py [options]

选项：
    --url URL           服务器地址 (默认: http://localhost:8080/ocr)
    --dpi DPI           PDF 渲染 DPI (默认: 150)
    --max-pages NUM     最大页数限制 (默认: 200, 设为 0 表示无限制)
    --pdf FILE          只测试指定的 PDF 文件
    --timeout SEC       请求超时时间 (默认: 600 秒)
    --verbose           详细输出
    --help              显示帮助信息
"""

import os
import sys
import json
import base64
import argparse
import time
import subprocess
from pathlib import Path
from datetime import datetime

# 尝试导入 requests，如果没有则使用 urllib
try:
    import requests
    HAS_REQUESTS = True
except ImportError:
    import urllib.request
    import urllib.error
    HAS_REQUESTS = False

# 脚本所在目录
SCRIPT_DIR = Path(__file__).parent.absolute()
SERVER_DIR = SCRIPT_DIR.parent  # server 目录
PDF_DIR = SERVER_DIR / "pdf_file"  # PDF 文件在 server/pdf_file/
RESULT_DIR = SCRIPT_DIR / "results"  # 结果保存在 server/tests/results/

# 默认配置
DEFAULT_CONFIG = {
    "url": "http://localhost:8080/ocr",
    "dpi": 150,
    "max_pages": 200,  # 最大页数，0 表示无限制
    "timeout": 600,    # 10 分钟超时
}


def print_banner():
    """打印启动横幅"""
    print("=" * 60)
    print("         PDF OCR 自动化测试脚本")
    print("=" * 60)
    print(f"PDF 目录: {PDF_DIR}")
    print(f"结果目录: {RESULT_DIR}")
    print("=" * 60)
    print()


def check_server(url: str) -> bool:
    """检查 OCR 服务是否运行"""
    health_url = url.replace("/ocr", "/health")
    
    try:
        if HAS_REQUESTS:
            response = requests.get(health_url, timeout=5)
            return response.status_code == 200
        else:
            req = urllib.request.Request(health_url)
            with urllib.request.urlopen(req, timeout=5) as response:
                return response.status == 200
    except Exception:
        return False


def print_server_instructions():
    """打印服务启动指令"""
    print("\n" + "=" * 60)
    print("❌ OCR 服务未启动！")
    print("=" * 60)
    print("\n请先启动 OCR 服务器：\n")
    print("  source ./set_env.sh 1 2 1 3 2 4")
    print("  cd server")
    print("  bash ./run_server.sh")
    print("\n" + "=" * 60)


def get_pdf_page_count(pdf_path: str) -> int:
    """
    获取 PDF 页数（使用 pdfinfo 或估算）
    """
    try:
        # 尝试使用 pdfinfo
        result = subprocess.run(
            ["pdfinfo", pdf_path],
            capture_output=True,
            text=True,
            timeout=10
        )
        if result.returncode == 0:
            for line in result.stdout.split("\n"):
                if line.startswith("Pages:"):
                    return int(line.split(":")[1].strip())
    except (subprocess.TimeoutExpired, FileNotFoundError, Exception):
        pass
    
    # 如果 pdfinfo 不可用，根据文件大小估算
    # 通常每页 PDF 约 10-50KB
    file_size_kb = os.path.getsize(pdf_path) / 1024
    estimated_pages = max(1, int(file_size_kb / 20))  # 假设每页约 20KB
    return estimated_pages


def encode_pdf_to_base64(pdf_path: str) -> str:
    """将 PDF 文件编码为 Base64"""
    with open(pdf_path, "rb") as f:
        return base64.b64encode(f.read()).decode("utf-8")


def send_ocr_request(url: str, pdf_base64: str, dpi: int, max_pages: int, 
                     timeout: int, verbose: bool = False) -> dict:
    """发送 OCR 请求"""
    request_data = {
        "file": pdf_base64,
        "fileType": 0,  # PDF
        "pdfDpi": dpi,
        "pdfMaxPages": max_pages,
        "useDocOrientationClassify": False,
        "useDocUnwarping": False,
        "textDetThresh": 0.3,
        "textDetBoxThresh": 0.6,
        "textDetUnclipRatio": 1.5,
        "textRecScoreThresh": 0.0,
        "visualize": False
    }
    
    headers = {
        "Content-Type": "application/json",
        "Authorization": "token test"
    }
    
    if verbose:
        print(f"  请求参数: dpi={dpi}, maxPages={max_pages}")
    
    if HAS_REQUESTS:
        response = requests.post(
            url,
            json=request_data,
            headers=headers,
            timeout=timeout
        )
        return response.json()
    else:
        data = json.dumps(request_data).encode("utf-8")
        req = urllib.request.Request(url, data=data, headers=headers, method="POST")
        with urllib.request.urlopen(req, timeout=timeout) as response:
            return json.loads(response.read().decode("utf-8"))


def process_pdf(pdf_path: Path, config: dict, verbose: bool = False) -> dict:
    """处理单个 PDF 文件"""
    pdf_name = pdf_path.stem
    result_path = RESULT_DIR / f"{pdf_name}_OCR_result.json"
    
    print(f"\n📄 处理: {pdf_path.name}")
    print("-" * 50)
    
    # 获取文件信息
    file_size_mb = pdf_path.stat().st_size / (1024 * 1024)
    print(f"  文件大小: {file_size_mb:.2f} MB")
    
    # 估算页数
    estimated_pages = get_pdf_page_count(str(pdf_path))
    print(f"  估算页数: ~{estimated_pages} 页")
    
    # 确定 maxPages (服务器限制最大为 100)
    SERVER_MAX_PAGES_LIMIT = 100
    if config["max_pages"] == 0:
        max_pages = min(max(estimated_pages + 10, 100), SERVER_MAX_PAGES_LIMIT)
    else:
        max_pages = min(config["max_pages"], max(estimated_pages + 10, 100), SERVER_MAX_PAGES_LIMIT)
    
    print(f"  设置 maxPages: {max_pages}")
    
    # Base64 编码
    print("  正在编码 PDF...")
    start_time = time.time()
    pdf_base64 = encode_pdf_to_base64(str(pdf_path))
    encode_time = time.time() - start_time
    print(f"  编码完成: {len(pdf_base64) / 1024 / 1024:.2f} MB ({encode_time:.2f}s)")
    
    # 发送请求
    print("  正在发送 OCR 请求...")
    start_time = time.time()
    
    try:
        response = send_ocr_request(
            config["url"],
            pdf_base64,
            config["dpi"],
            max_pages,
            config["timeout"],
            verbose
        )
        
        request_time = time.time() - start_time
        
        # 检查响应
        if response.get("errorCode") == 0:
            result = response.get("result", {})
            total_pages = result.get("totalPages", 0)
            rendered_pages = result.get("renderedPages", 0)
            pages_data = result.get("pages", [])
            
            # 统计识别的文本框数量
            total_boxes = sum(len(page.get("ocrResults", [])) for page in pages_data)
            
            print(f"  ✅ OCR 成功!")
            print(f"     总页数: {total_pages}")
            print(f"     处理页数: {rendered_pages}")
            print(f"     识别文本框: {total_boxes}")
            print(f"     耗时: {request_time:.2f}s")
            
            if result.get("warning"):
                print(f"     ⚠️ 警告: {result.get('warning')}")
            
            # 保存结果
            with open(result_path, "w", encoding="utf-8") as f:
                json.dump(response, f, ensure_ascii=False, indent=2)
            print(f"  📁 结果保存: {result_path.name}")
            
            return {
                "status": "success",
                "pdf_name": pdf_name,
                "total_pages": total_pages,
                "rendered_pages": rendered_pages,
                "total_boxes": total_boxes,
                "time_seconds": request_time,
                "result_file": str(result_path)
            }
        else:
            error_msg = response.get("errorMsg", "Unknown error")
            print(f"  ❌ OCR 失败: {error_msg}")
            
            # 保存错误响应
            with open(result_path, "w", encoding="utf-8") as f:
                json.dump(response, f, ensure_ascii=False, indent=2)
            
            return {
                "status": "error",
                "pdf_name": pdf_name,
                "error": error_msg,
                "time_seconds": request_time
            }
            
    except Exception as e:
        print(f"  ❌ 请求异常: {str(e)}")
        return {
            "status": "exception",
            "pdf_name": pdf_name,
            "error": str(e)
        }


def main():
    parser = argparse.ArgumentParser(
        description="PDF OCR 自动化测试脚本",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument("--url", default=DEFAULT_CONFIG["url"],
                        help=f"服务器地址 (默认: {DEFAULT_CONFIG['url']})")
    parser.add_argument("--dpi", type=int, default=DEFAULT_CONFIG["dpi"],
                        help=f"PDF 渲染 DPI (默认: {DEFAULT_CONFIG['dpi']})")
    parser.add_argument("--max-pages", type=int, default=DEFAULT_CONFIG["max_pages"],
                        help=f"最大页数限制 (默认: {DEFAULT_CONFIG['max_pages']}, 设为 0 表示无限制)")
    parser.add_argument("--pdf", type=str, default=None,
                        help="只测试指定的 PDF 文件")
    parser.add_argument("--timeout", type=int, default=DEFAULT_CONFIG["timeout"],
                        help=f"请求超时时间秒 (默认: {DEFAULT_CONFIG['timeout']})")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="详细输出")
    
    args = parser.parse_args()
    
    # 打印横幅
    print_banner()
    
    # 确保目录存在
    PDF_DIR.mkdir(parents=True, exist_ok=True)
    RESULT_DIR.mkdir(parents=True, exist_ok=True)
    
    # 检查服务
    print("检查 OCR 服务...")
    if not check_server(args.url):
        print_server_instructions()
        sys.exit(1)
    print("✅ OCR 服务运行中\n")
    
    # 获取 PDF 文件列表
    if args.pdf:
        pdf_path = Path(args.pdf)
        if not pdf_path.exists():
            pdf_path = PDF_DIR / args.pdf
        if not pdf_path.exists():
            print(f"❌ 找不到 PDF 文件: {args.pdf}")
            sys.exit(1)
        pdf_files = [pdf_path]
    else:
        pdf_files = list(PDF_DIR.glob("*.pdf"))
    
    if not pdf_files:
        print(f"❌ 在 {PDF_DIR} 中没有找到 PDF 文件")
        sys.exit(1)
    
    print(f"找到 {len(pdf_files)} 个 PDF 文件:")
    for pdf in pdf_files:
        print(f"  - {pdf.name}")
    
    # 配置
    config = {
        "url": args.url,
        "dpi": args.dpi,
        "max_pages": args.max_pages,
        "timeout": args.timeout
    }
    
    print(f"\n配置:")
    print(f"  URL: {config['url']}")
    print(f"  DPI: {config['dpi']}")
    print(f"  最大页数: {config['max_pages'] if config['max_pages'] > 0 else '无限制'}")
    print(f"  超时: {config['timeout']}s")
    
    # 处理每个 PDF
    results = []
    total_start = time.time()
    
    for pdf_path in pdf_files:
        result = process_pdf(pdf_path, config, args.verbose)
        results.append(result)
    
    total_time = time.time() - total_start
    
    # 打印汇总
    print("\n" + "=" * 60)
    print("                    测试汇总")
    print("=" * 60)
    
    success_count = sum(1 for r in results if r["status"] == "success")
    error_count = len(results) - success_count
    
    print(f"总文件数: {len(results)}")
    print(f"成功: {success_count}")
    print(f"失败: {error_count}")
    print(f"总耗时: {total_time:.2f}s")
    
    print("\n详细结果:")
    for r in results:
        if r["status"] == "success":
            print(f"  ✅ {r['pdf_name']}: {r['rendered_pages']}/{r['total_pages']} 页, "
                  f"{r['total_boxes']} 文本框, {r['time_seconds']:.2f}s")
        else:
            print(f"  ❌ {r['pdf_name']}: {r.get('error', 'Unknown error')}")
    
    # 保存测试报告
    report_path = RESULT_DIR / f"test_report_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
    report = {
        "timestamp": datetime.now().isoformat(),
        "config": config,
        "summary": {
            "total_files": len(results),
            "success_count": success_count,
            "error_count": error_count,
            "total_time_seconds": total_time
        },
        "results": results
    }
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
    print(f"\n📊 测试报告: {report_path}")
    
    print("\n" + "=" * 60)
    
    return 0 if error_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
