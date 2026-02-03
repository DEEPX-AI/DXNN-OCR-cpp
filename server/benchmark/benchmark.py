#!/usr/bin/env python3
"""
OCR API Server Benchmark Framework v2.0
工业级基准测试框架 - 主入口

功能特性:
- 多种测试模式: 延迟、吞吐量、压力、稳定性、容量规划
- 资源监控: CPU、内存、NPU
- 多格式报告: Markdown、HTML、JSON、CSV
- 可视化图表: 延迟分布、时间序列、资源使用
- 配置文件支持: YAML/JSON
- 灵活的命令行接口
"""

import sys
import asyncio
import argparse
from pathlib import Path

# 添加 core 模块到路径
sys.path.insert(0, str(Path(__file__).parent))

from core.config import BenchmarkConfig, TestMode, create_default_config
from core.executor import create_executor
from core.reporter import generate_reports
from core.visualizer import ChartGenerator


def parse_args():
    """解析命令行参数"""
    parser = argparse.ArgumentParser(
        description="OCR API Server Benchmark Framework v2.0",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # 使用配置文件运行
  python benchmark.py --config benchmark_config.yaml
  
  # 快速测试（延迟模式）
  python benchmark.py --mode latency --images ../../images --runs 3
  
  # 吞吐量测试
  python benchmark.py --mode throughput --images ../../images --concurrency 10
  
  # 生成默认配置文件
  python benchmark.py --generate-config
  
Test Modes:
  - latency:     串行测试，测量单请求延迟
  - throughput:  并发测试，测量系统吞吐量
  - stress:      压力测试，逐步增加负载
  - stability:   稳定性测试，长时间运行
  - capacity:    容量规划，找到最优并发数
        """
    )
    
    # 配置文件
    parser.add_argument("--config", "-c", type=str,
                        help="配置文件路径 (YAML/JSON)")
    
    parser.add_argument("--generate-config", action="store_true",
                        help="生成默认配置文件并退出")
    
    # 测试模式
    parser.add_argument("--mode", type=str, choices=['latency', 'throughput', 'stress', 'stability', 'capacity'],
                        help="测试模式")
    
    # 服务器配置
    parser.add_argument("--url", type=str,
                        help="服务器 URL")
    parser.add_argument("--token", type=str,
                        help="认证 token")
    parser.add_argument("--timeout", type=int,
                        help="请求超时时间（秒）")
    
    # 数据配置
    parser.add_argument("--images", type=str,
                        help="图片目录（默认在此目录下查找 labels.json 作为 ground truth）")
    parser.add_argument("--pdfs", type=str,
                        help="PDF 目录")
    parser.add_argument("--max-samples", type=int,
                        help="最大样本数量")
    
    # 测试场景
    parser.add_argument("--concurrency", type=int,
                        help="并发数")
    parser.add_argument("--runs", type=int,
                        help="每个样本运行次数")
    parser.add_argument("--warmup", type=int,
                        help="预热请求数量")
    parser.add_argument("--duration", type=int,
                        help="测试持续时间（秒，0表示运行完所有样本；stability 默认 600）")
    parser.add_argument("--max-concurrency", type=int,
                        help="最大并发数（stress/capacity）")
    parser.add_argument("--ramp-up", type=int,
                        help="压力测试每档运行时长（秒）；0 表示每档一轮样本")
    parser.add_argument("--concurrency-step", type=int,
                        help="并发步长（stress/capacity）")
    
    # OCR 参数
    parser.add_argument("--pdf-dpi", type=int,
                        help="PDF 渲染 DPI")
    parser.add_argument("--pdf-max-pages", type=int,
                        help="PDF 最大页数")
    
    # 监控配置
    parser.add_argument("--no-monitor", action="store_true",
                        help="禁用资源监控")
    
    # 报告配置
    parser.add_argument("--output-dir", type=str,
                        help="输出目录")
    parser.add_argument("--formats", type=str, nargs='+',
                        choices=['markdown', 'json', 'html', 'csv'],
                        help="报告格式")
    parser.add_argument("--no-charts", action="store_true",
                        help="不生成图表")
    
    return parser.parse_args()


def merge_config_with_args(config: BenchmarkConfig, args) -> BenchmarkConfig:
    """合并配置文件和命令行参数"""
    # 服务器配置
    if args.url:
        config.server.url = args.url
    if args.token:
        config.server.token = args.token
    if args.timeout:
        config.server.timeout = args.timeout
    
    # 数据配置
    if args.images:
        config.data.images_dir = args.images
    if args.pdfs:
        config.data.pdfs_dir = args.pdfs
    if args.max_samples:
        config.data.max_samples = args.max_samples
    
    # 测试场景
    if args.mode:
        config.scenario.mode = TestMode(args.mode)
    if args.concurrency:
        config.scenario.concurrency = args.concurrency
    if args.runs:
        config.scenario.runs_per_sample = args.runs
    if args.warmup is not None:
        config.scenario.warmup_requests = args.warmup
    if args.duration is not None:
        config.scenario.duration_seconds = args.duration
    if args.max_concurrency is not None:
        config.scenario.max_concurrency = args.max_concurrency
    if args.ramp_up is not None:
        config.scenario.ramp_up_seconds = args.ramp_up
    if args.concurrency_step is not None:
        config.scenario.concurrency_step = args.concurrency_step
    
    # OCR 参数
    if args.pdf_dpi:
        config.ocr_params.pdfDpi = args.pdf_dpi
    if args.pdf_max_pages:
        config.ocr_params.pdfMaxPages = args.pdf_max_pages
    
    # 监控配置
    if args.no_monitor:
        config.monitor.enable_system_monitor = False
    
    # 报告配置
    if args.output_dir:
        config.report.output_dir = args.output_dir
    if args.formats:
        config.report.formats = args.formats
    if args.no_charts:
        config.report.generate_charts = False
    
    return config


async def main():
    """主函数"""
    args = parse_args()
    
    # 生成默认配置文件
    if args.generate_config:
        create_default_config("benchmark_config.yaml")
        create_default_config("benchmark_config.json")
        return
    
    # 加载配置
    if args.config:
        config_path = Path(args.config)
        if not config_path.exists():
            print(f"Error: Config file not found: {args.config}")
            sys.exit(1)
        
        if config_path.suffix == '.yaml' or config_path.suffix == '.yml':
            config = BenchmarkConfig.from_yaml(args.config)
        elif config_path.suffix == '.json':
            config = BenchmarkConfig.from_json(args.config)
        else:
            print(f"Error: Unsupported config file format: {config_path.suffix}")
            sys.exit(1)
        
        print(f"✓ Loaded config from: {args.config}")
    else:
        # 使用默认配置
        config = BenchmarkConfig()
    
    # 合并命令行参数
    config = merge_config_with_args(config, args)
    
    # 验证配置
    valid, error_msg = config.validate()
    if not valid:
        print(f"Error: Invalid configuration: {error_msg}")
        sys.exit(1)
    
    # 打印配置摘要
    print("\n" + "=" * 70)
    print("OCR API Server Benchmark Framework v2.0")
    print("=" * 70)
    print(f"Test Mode:     {config.scenario.mode.value}")
    print(f"Server URL:    {config.server.url}")
    print(f"Concurrency:   {config.scenario.concurrency}")
    print(f"Runs/Sample:   {config.scenario.runs_per_sample}")
    print(f"Output Dir:    {config.report.output_dir}")
    print(f"Report Formats: {', '.join(config.report.formats)}")
    print("=" * 70 + "\n")
    
    # 创建执行器
    executor = create_executor(config)
    
    # 加载样本
    print("[1/4] Loading test samples...")
    executor.load_samples()
    
    if not executor.samples:
        print("Error: No test samples loaded")
        sys.exit(1)
    
    # 运行测试
    print("\n[2/4] Running benchmark test...")
    collector, monitor = await executor.run()
    
    # 计算指标
    print("\n[3/4] Computing metrics...")
    metrics = collector.compute_metrics(executor.ground_truth)
    
    print("\n" + collector.get_summary())
    
    # 生成报告
    print("\n[4/4] Generating reports...")
    report_files = generate_reports(
        metrics,
        monitor,
        config.to_dict(),
        config.report.output_dir,
        config.report.formats
    )
    
    # 生成图表
    if config.report.generate_charts:
        print("\n[Bonus] Generating charts...")
        chart_gen = ChartGenerator(config.report.output_dir)
        chart_files = chart_gen.generate_all(metrics, monitor, config.scenario.name)
        print(f"✓ Generated {len(chart_files)} charts")
    
    # 完成
    print("\n" + "=" * 70)
    print("✓ Benchmark completed successfully!")
    print("=" * 70)
    print(f"\nResults saved to: {config.report.output_dir}")
    print("\nGenerated files:")
    for f in report_files:
        print(f"  - {f}")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n\nBenchmark interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n\nError: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

