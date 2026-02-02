"""
报告生成模块
支持多种格式：Markdown, HTML, JSON, CSV
"""

import json
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Optional
from .metrics import PerformanceMetrics
from .monitor import ResourceMonitor


class ReportGenerator:
    """报告生成器基类"""
    
    def __init__(self, output_dir: str):
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
    
    def generate(self, metrics: PerformanceMetrics, monitor: ResourceMonitor,
                 config: Dict) -> str:
        """生成报告（子类实现）"""
        raise NotImplementedError


class MarkdownReportGenerator(ReportGenerator):
    """Markdown 报告生成器"""
    
    def generate(self, metrics: PerformanceMetrics, monitor: ResourceMonitor,
                 config: Dict) -> str:
        """生成 Markdown 报告"""
        output_file = self.output_dir / f"{metrics.test_name}_report.md"
        
        lines = []
        lines.append(f"# OCR API Benchmark Report: {metrics.test_name}\n")
        lines.append(f"**Generated at:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        
        # Test Configuration
        lines.append("## 📋 Test Configuration\n")
        lines.append("| Parameter | Value |")
        lines.append("|-----------|-------|")
        lines.append(f"| Test Name | {metrics.test_name} |")
        lines.append(f"| Test Mode | {config.get('scenario', {}).get('mode', 'N/A')} |")
        lines.append(f"| Concurrency | {config.get('scenario', {}).get('concurrency', 1)} |")
        lines.append(f"| Runs per Sample | {config.get('scenario', {}).get('runs_per_sample', 1)} |")
        lines.append(f"| Server URL | {config.get('server', {}).get('url', 'N/A')} |")
        lines.append(f"| Timeout | {config.get('server', {}).get('timeout', 60)}s |")
        lines.append("")
        
        # Performance Summary
        lines.append("## 🚀 Performance Summary\n")
        lines.append("| Metric | Value |")
        lines.append("|--------|-------|")
        lines.append(f"| **Total Requests** | {metrics.throughput.total_requests} |")
        lines.append(f"| **Success Rate** | {metrics.throughput.success_rate:.2f}% |")
        lines.append(f"| **QPS** | **{metrics.throughput.qps:.2f}** |")
        lines.append(f"| **Success QPS** | {metrics.throughput.success_qps:.2f} |")
        lines.append(f"| **Total Duration (ms)** | {metrics.throughput.total_duration_ms:.2f} |")
        lines.append("")
        
        # Latency Statistics
        lines.append("## ⏱️ Latency Statistics\n")
        lines.append("| Percentile | Latency (ms) |")
        lines.append("|------------|--------------|")
        lines.append(f"| Min | {metrics.latency.min:.2f} |")
        lines.append(f"| P50 (Median) | {metrics.latency.p50:.2f} |")
        lines.append(f"| P90 | {metrics.latency.p90:.2f} |")
        lines.append(f"| P95 | {metrics.latency.p95:.2f} |")
        lines.append(f"| P99 | {metrics.latency.p99:.2f} |")
        lines.append(f"| P99.9 | {metrics.latency.p999:.2f} |")
        lines.append(f"| Max | {metrics.latency.max:.2f} |")
        lines.append(f"| Mean | {metrics.latency.mean:.2f} |")
        lines.append(f"| Std Dev | {metrics.latency.std:.2f} |")
        lines.append("")
        
        # OCR Statistics
        lines.append("## 📝 OCR Statistics\n")
        lines.append("| Metric | Value |")
        lines.append("|--------|-------|")
        lines.append(f"| Total Characters | {metrics.ocr.total_chars} |")
        lines.append(f"| Total Pages (pdf)| {metrics.ocr.total_pages} |")
        lines.append(f"| CPS (Chars/sec) | {metrics.ocr.cps:.2f} |")
        if metrics.ocr.total_pages > 0:
            lines.append(f"| PPS (Pages/sec) | {metrics.ocr.pps:.2f} |")
        if metrics.ocr.accuracy is not None:
            lines.append(f"| Accuracy | {metrics.ocr.accuracy:.2f}% |")
        lines.append("")
        
        # Error Statistics
        if metrics.errors.error_breakdown:
            lines.append("## ⚠️ Error Statistics\n")
            lines.append("| Error Type | Count |")
            lines.append("|------------|-------|")
            for error_type, count in metrics.errors.error_breakdown.items():
                if count > 0 and error_type != 'none':
                    lines.append(f"| {error_type} | {count} |")
            lines.append(f"\n**Error Rate:** {metrics.errors.error_rate:.2f}%")
            lines.append(f"**Timeout Rate:** {metrics.errors.timeout_rate:.2f}%\n")
        
        # Resource Monitoring
        monitor_summary = monitor.get_summary()
        if monitor_summary:
            lines.append("## 💻 Resource Monitoring\n")
            
            if 'cpu' in monitor_summary:
                lines.append("### CPU")
                lines.append("| Metric | Value |")
                lines.append("|--------|-------|")
                lines.append(f"| Average | {monitor_summary['cpu']['avg']:.2f}% |")
                lines.append(f"| Max | {monitor_summary['cpu']['max']:.2f}% |")
                lines.append(f"| Min | {monitor_summary['cpu']['min']:.2f}% |")
                lines.append("")
            
            if 'memory' in monitor_summary:
                lines.append("### Memory")
                lines.append("| Metric | Value |")
                lines.append("|--------|-------|")
                lines.append(f"| Average | {monitor_summary['memory']['avg']:.2f}% |")
                lines.append(f"| Max | {monitor_summary['memory']['max']:.2f}% |")
                lines.append(f"| Min | {monitor_summary['memory']['min']:.2f}% |")
                lines.append("")
        
        # Stress 测试：每档并发结果
        stress_stages = getattr(metrics, "stress_stage_results", None)
        if stress_stages:
            lines.append("## 📈 Stress Test: Per-Stage Results\n")
            lines.append("| Concurrency | QPS | Success QPS | Success Rate (%) | P95 (ms) | P99 (ms) | Requests |")
            lines.append("|-------------|-----|-------------|-------------------|----------|----------|----------|")
            for s in stress_stages:
                lines.append(
                    f"| {s.get('concurrency', '')} | {s.get('qps', 0):.2f} | {s.get('success_qps', 0):.2f} | "
                    f"{s.get('success_rate', 0):.2f} | {s.get('p95_ms', 0):.2f} | {s.get('p99_ms', 0):.2f} | {s.get('total_requests', 0)} |"
                )
            lines.append("")
        
        # Capacity 测试：每档并发结果
        capacity_stages = getattr(metrics, "capacity_stage_results", None)
        if capacity_stages:
            lines.append("## 📊 Capacity Planning: Per-Stage Results\n")
            lines.append("| Concurrency | QPS | Success QPS | Success Rate (%) | P95 (ms) | P99 (ms) | Requests |")
            lines.append("|-------------|-----|-------------|-------------------|----------|----------|----------|")
            for s in capacity_stages:
                lines.append(
                    f"| {s.get('concurrency', '')} | {s.get('qps', 0):.2f} | {s.get('success_qps', 0):.2f} | "
                    f"{s.get('success_rate', 0):.2f} | {s.get('p95_ms', 0):.2f} | {s.get('p99_ms', 0):.2f} | {s.get('total_requests', 0)} |"
                )
            lines.append("")
        
        # Per-Sample Results
        if metrics.per_sample_stats:
            lines.append("## 📊 Per-Sample Results\n")
            lines.append("| Sample Name | Runs | Success | Avg Latency (ms) | Chars | Pages |")
            lines.append("|-------------|------|---------|------------------|-------|-------|")
            
            for sample_name, stats in sorted(metrics.per_sample_stats.items()):
                if 'error' not in stats:
                    lines.append(
                        f"| `{sample_name}` | {stats['total_runs']} | {stats['successful_runs']} | "
                        f"{stats['avg_latency_ms']:.2f} | {stats['char_count']} | {stats['page_count']} |"
                    )
            lines.append("")
        
        # Footer
        lines.append("---")
        lines.append(f"\n*Report generated by OCR Benchmark Framework v2.0*")
        
        report_content = "\n".join(lines)
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(report_content)
        
        print(f"✓ Markdown report saved to: {output_file}")
        return str(output_file)


class JSONReportGenerator(ReportGenerator):
    """JSON 报告生成器"""
    
    def generate(self, metrics: PerformanceMetrics, monitor: ResourceMonitor,
                 config: Dict) -> str:
        """生成 JSON 报告"""
        output_file = self.output_dir / f"{metrics.test_name}_results.json"
        
        report = {
            "test_name": metrics.test_name,
            "timestamp": datetime.now().isoformat(),
            "config": config,
            "metrics": metrics.to_dict(),
            "resource_monitoring": monitor.get_summary(),
        }
        
        with open(output_file, 'w', encoding='utf-8') as f:
            json.dump(report, f, indent=2, ensure_ascii=False)
        
        print(f"✓ JSON report saved to: {output_file}")
        return str(output_file)


class HTMLReportGenerator(ReportGenerator):
    """HTML 报告生成器"""
    
    def generate(self, metrics: PerformanceMetrics, monitor: ResourceMonitor,
                 config: Dict) -> str:
        """生成 HTML 报告"""
        output_file = self.output_dir / f"{metrics.test_name}_report.html"
        
        html = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OCR Benchmark Report: {metrics.test_name}</title>
    <style>
        * {{ margin: 0; padding: 0; box-sizing: border-box; }}
        body {{
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            padding: 20px;
            line-height: 1.6;
        }}
        .container {{
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            border-radius: 12px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            overflow: hidden;
        }}
        .header {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 40px;
            text-align: center;
        }}
        .header h1 {{
            font-size: 2.5em;
            margin-bottom: 10px;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.2);
        }}
        .header .subtitle {{
            font-size: 1.1em;
            opacity: 0.9;
        }}
        .content {{
            padding: 40px;
        }}
        .section {{
            margin-bottom: 40px;
        }}
        .section h2 {{
            color: #667eea;
            border-bottom: 3px solid #667eea;
            padding-bottom: 10px;
            margin-bottom: 20px;
            font-size: 1.8em;
        }}
        .metrics-grid {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }}
        .metric-card {{
            background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);
            padding: 25px;
            border-radius: 10px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
            transition: transform 0.3s;
        }}
        .metric-card:hover {{
            transform: translateY(-5px);
            box-shadow: 0 6px 12px rgba(0,0,0,0.15);
        }}
        .metric-card .label {{
            font-size: 0.9em;
            color: #666;
            margin-bottom: 8px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }}
        .metric-card .value {{
            font-size: 2em;
            font-weight: bold;
            color: #667eea;
        }}
        .metric-card .unit {{
            font-size: 0.8em;
            color: #888;
        }}
        table {{
            width: 100%;
            border-collapse: collapse;
            margin-top: 20px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }}
        th, td {{
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }}
        th {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            font-weight: 600;
        }}
        tr:hover {{
            background-color: #f5f5f5;
        }}
        .footer {{
            background: #f8f9fa;
            padding: 20px;
            text-align: center;
            color: #666;
            font-size: 0.9em;
        }}
        .badge {{
            display: inline-block;
            padding: 4px 12px;
            border-radius: 20px;
            font-size: 0.85em;
            font-weight: 600;
        }}
        .badge-success {{ background: #d4edda; color: #155724; }}
        .badge-warning {{ background: #fff3cd; color: #856404; }}
        .badge-danger {{ background: #f8d7da; color: #721c24; }}
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🚀 OCR Benchmark Report</h1>
            <div class="subtitle">{metrics.test_name}</div>
            <div class="subtitle">{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</div>
        </div>
        
        <div class="content">
            <div class="section">
                <h2>📊 Performance Summary</h2>
                <div class="metrics-grid">
                    <div class="metric-card">
                        <div class="label">QPS</div>
                        <div class="value">{metrics.throughput.qps:.2f}</div>
                        <div class="unit">requests/sec</div>
                    </div>
                    <div class="metric-card">
                        <div class="label">Success Rate</div>
                        <div class="value">{metrics.throughput.success_rate:.1f}<span class="unit">%</span></div>
                    </div>
                    <div class="metric-card">
                        <div class="label">Avg Latency</div>
                        <div class="value">{metrics.latency.mean:.2f}</div>
                        <div class="unit">ms</div>
                    </div>
                    <div class="metric-card">
                        <div class="label">P99 Latency</div>
                        <div class="value">{metrics.latency.p99:.2f}</div>
                        <div class="unit">ms</div>
                    </div>
                    <div class="metric-card">
                        <div class="label">Total Duration (ms)</div>
                        <div class="value">{metrics.throughput.total_duration_ms:.2f}</div>
                    </div>
                    <div class="metric-card">
                        <div class="label">Total Requests</div>
                        <div class="value">{metrics.throughput.total_requests}</div>
                    </div>
                    <div class="metric-card">
                        <div class="label">Total Chars</div>
                        <div class="value">{metrics.ocr.total_chars}</div>
                    </div>
                </div>
            </div>
            
            <div class="section">
                <h2>⏱️ Latency Distribution</h2>
                <table>
                    <tr>
                        <th>Percentile</th>
                        <th>Latency (ms)</th>
                    </tr>
                    <tr><td>Min</td><td>{metrics.latency.min:.2f}</td></tr>
                    <tr><td>P50 (Median)</td><td>{metrics.latency.p50:.2f}</td></tr>
                    <tr><td>P90</td><td>{metrics.latency.p90:.2f}</td></tr>
                    <tr><td>P95</td><td>{metrics.latency.p95:.2f}</td></tr>
                    <tr><td>P99</td><td>{metrics.latency.p99:.2f}</td></tr>
                    <tr><td>P99.9</td><td>{metrics.latency.p999:.2f}</td></tr>
                    <tr><td>Max</td><td>{metrics.latency.max:.2f}</td></tr>
                </table>
            </div>
        </div>
        
        <div class="footer">
            <p>Generated by OCR Benchmark Framework v2.0</p>
        </div>
    </div>
</body>
</html>"""
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(html)
        
        print(f"✓ HTML report saved to: {output_file}")
        return str(output_file)


class CSVReportGenerator(ReportGenerator):
    """CSV 报告生成器"""
    
    def generate(self, metrics: PerformanceMetrics, monitor: ResourceMonitor,
                 config: Dict) -> str:
        """生成 CSV 报告"""
        import csv
        
        output_file = self.output_dir / f"{metrics.test_name}_results.csv"
        
        with open(output_file, 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            
            # 写入汇总指标
            writer.writerow(['Metric', 'Value'])
            writer.writerow(['Test Name', metrics.test_name])
            writer.writerow(['Total Requests', metrics.throughput.total_requests])
            writer.writerow(['Success Rate (%)', f"{metrics.throughput.success_rate:.2f}"])
            writer.writerow(['QPS', f"{metrics.throughput.qps:.2f}"])
            writer.writerow(['Avg Latency (ms)', f"{metrics.latency.mean:.2f}"])
            writer.writerow(['P99 Latency (ms)', f"{metrics.latency.p99:.2f}"])
            writer.writerow([])
            
            # 写入每个样本的结果
            writer.writerow(['Sample Name', 'Total Runs', 'Successful Runs', 'Avg Latency (ms)', 'Chars', 'Pages'])
            for sample_name, stats in sorted(metrics.per_sample_stats.items()):
                if 'error' not in stats:
                    writer.writerow([
                        sample_name,
                        stats['total_runs'],
                        stats['successful_runs'],
                        f"{stats['avg_latency_ms']:.2f}",
                        stats['char_count'],
                        stats['page_count']
                    ])
        
        print(f"✓ CSV report saved to: {output_file}")
        return str(output_file)


def generate_reports(metrics: PerformanceMetrics, monitor: ResourceMonitor,
                     config: Dict, output_dir: str, formats: List[str]) -> List[str]:
    """
    生成多种格式的报告
    
    Args:
        metrics: 性能指标
        monitor: 资源监控器
        config: 配置信息
        output_dir: 输出目录
        formats: 报告格式列表 ['markdown', 'json', 'html', 'csv']
    
    Returns:
        生成的报告文件路径列表
    """
    generators = {
        'markdown': MarkdownReportGenerator,
        'json': JSONReportGenerator,
        'html': HTMLReportGenerator,
        'csv': CSVReportGenerator,
    }
    
    report_files = []
    
    for fmt in formats:
        if fmt in generators:
            generator = generators[fmt](output_dir)
            report_file = generator.generate(metrics, monitor, config)
            report_files.append(report_file)
        else:
            print(f"Warning: Unknown report format: {fmt}")
    
    return report_files

