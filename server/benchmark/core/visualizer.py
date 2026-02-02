"""
可视化图表生成模块
生成延迟分布、时间序列、资源使用等图表
"""

import numpy as np
from pathlib import Path
from typing import List, Dict, Optional
from .metrics import PerformanceMetrics
from .monitor import ResourceMonitor

try:
    import matplotlib
    matplotlib.use('Agg')  # 非交互式后端
    import matplotlib.pyplot as plt
    import seaborn as sns
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False
    print("Warning: matplotlib not available, charts will not be generated")


class ChartGenerator:
    """图表生成器"""
    
    def __init__(self, output_dir: str):
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        if MATPLOTLIB_AVAILABLE:
            # 设置样式
            sns.set_style("whitegrid")
            plt.rcParams['figure.figsize'] = (12, 6)
            plt.rcParams['font.size'] = 10
    
    def generate_all(self, metrics: PerformanceMetrics, monitor: ResourceMonitor,
                     test_name: str) -> List[str]:
        """生成所有图表"""
        if not MATPLOTLIB_AVAILABLE:
            return []
        
        chart_files = []
        
        # 1. 延迟分布直方图
        chart_files.append(self.plot_latency_distribution(metrics, test_name))
        
        # 2. 延迟百分位图
        chart_files.append(self.plot_latency_percentiles(metrics, test_name))
        
        # 3. 时间序列图
        if metrics.timeline:
            chart_files.append(self.plot_timeline(metrics, test_name))
        
        # 4. 资源使用图
        if monitor.metrics_history:
            chart_files.append(self.plot_resource_usage(monitor, test_name))
        
        # 5. 每个样本的性能对比
        if metrics.per_sample_stats:
            chart_files.append(self.plot_per_sample_comparison(metrics, test_name))
        
        return [f for f in chart_files if f]
    
    def plot_latency_distribution(self, metrics: PerformanceMetrics, test_name: str) -> Optional[str]:
        """绘制延迟分布直方图"""
        try:
            # 提取成功请求的延迟
            latencies = []
            for req in metrics.timeline:
                if req.get('status') == 'success':
                    latencies.append(req['latency_ms'])
            
            if not latencies:
                return None
            
            fig, ax = plt.subplots(figsize=(12, 6))
            
            # 绘制直方图
            n, bins, patches = ax.hist(latencies, bins=50, alpha=0.7, color='#667eea', edgecolor='black')
            
            # 添加统计线
            ax.axvline(metrics.latency.mean, color='red', linestyle='--', linewidth=2, label=f'Mean: {metrics.latency.mean:.2f}ms')
            ax.axvline(metrics.latency.p50, color='green', linestyle='--', linewidth=2, label=f'P50: {metrics.latency.p50:.2f}ms')
            ax.axvline(metrics.latency.p99, color='orange', linestyle='--', linewidth=2, label=f'P99: {metrics.latency.p99:.2f}ms')
            
            ax.set_xlabel('Latency (ms)', fontsize=12, fontweight='bold')
            ax.set_ylabel('Frequency', fontsize=12, fontweight='bold')
            ax.set_title(f'Latency Distribution - {test_name}', fontsize=14, fontweight='bold')
            ax.legend(fontsize=10)
            ax.grid(True, alpha=0.3)
            
            output_file = self.output_dir / f"{test_name}_latency_distribution.png"
            plt.tight_layout()
            plt.savefig(output_file, dpi=150, bbox_inches='tight')
            plt.close()
            
            print(f"✓ Chart saved: {output_file}")
            return str(output_file)
        
        except Exception as e:
            print(f"Error generating latency distribution chart: {e}")
            return None
    
    def plot_latency_percentiles(self, metrics: PerformanceMetrics, test_name: str) -> Optional[str]:
        """绘制延迟百分位图"""
        try:
            percentiles = [50, 90, 95, 99, 99.9]
            values = [
                metrics.latency.p50,
                metrics.latency.p90,
                metrics.latency.p95,
                metrics.latency.p99,
                metrics.latency.p999,
            ]
            
            fig, ax = plt.subplots(figsize=(10, 6))
            
            bars = ax.bar(range(len(percentiles)), values, color=['#667eea', '#764ba2', '#f093fb', '#4facfe', '#00f2fe'])
            
            # 添加数值标签
            for i, (bar, val) in enumerate(zip(bars, values)):
                height = bar.get_height()
                ax.text(bar.get_x() + bar.get_width()/2., height,
                       f'{val:.2f}ms',
                       ha='center', va='bottom', fontweight='bold')
            
            ax.set_xticks(range(len(percentiles)))
            ax.set_xticklabels([f'P{p}' for p in percentiles])
            ax.set_ylabel('Latency (ms)', fontsize=12, fontweight='bold')
            ax.set_title(f'Latency Percentiles - {test_name}', fontsize=14, fontweight='bold')
            ax.grid(True, alpha=0.3, axis='y')
            
            output_file = self.output_dir / f"{test_name}_latency_percentiles.png"
            plt.tight_layout()
            plt.savefig(output_file, dpi=150, bbox_inches='tight')
            plt.close()
            
            print(f"✓ Chart saved: {output_file}")
            return str(output_file)
        
        except Exception as e:
            print(f"Error generating latency percentiles chart: {e}")
            return None
    
    def plot_timeline(self, metrics: PerformanceMetrics, test_name: str) -> Optional[str]:
        """绘制时间序列图"""
        try:
            if not metrics.timeline:
                return None
            
            times = [t['relative_time_ms'] / 1000 for t in metrics.timeline]  # 转换为秒
            latencies = [t['latency_ms'] for t in metrics.timeline]
            
            fig, ax = plt.subplots(figsize=(14, 6))
            
            # 绘制散点图
            ax.scatter(times, latencies, alpha=0.5, s=20, color='#667eea')
            
            # 添加移动平均线
            window_size = max(1, len(latencies) // 50)
            if len(latencies) >= window_size:
                moving_avg = np.convolve(latencies, np.ones(window_size)/window_size, mode='valid')
                moving_avg_times = times[window_size-1:]
                ax.plot(moving_avg_times, moving_avg, color='red', linewidth=2, label=f'Moving Avg (window={window_size})')
            
            ax.set_xlabel('Time (seconds)', fontsize=12, fontweight='bold')
            ax.set_ylabel('Latency (ms)', fontsize=12, fontweight='bold')
            ax.set_title(f'Latency Over Time - {test_name}', fontsize=14, fontweight='bold')
            ax.legend(fontsize=10)
            ax.grid(True, alpha=0.3)
            
            output_file = self.output_dir / f"{test_name}_timeline.png"
            plt.tight_layout()
            plt.savefig(output_file, dpi=150, bbox_inches='tight')
            plt.close()
            
            print(f"✓ Chart saved: {output_file}")
            return str(output_file)
        
        except Exception as e:
            print(f"Error generating timeline chart: {e}")
            return None
    
    def plot_resource_usage(self, monitor: ResourceMonitor, test_name: str) -> Optional[str]:
        """绘制资源使用图"""
        try:
            if not monitor.metrics_history:
                return None
            
            timeline = monitor.get_timeline()
            
            # 提取数据
            timestamps = [(m['timestamp'] - timeline[0]['timestamp']) for m in timeline]
            cpu_usage = [m['cpu_percent'] for m in timeline]
            memory_usage = [m['memory_percent'] for m in timeline]
            
            fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10))
            
            # CPU 使用率
            ax1.plot(timestamps, cpu_usage, color='#667eea', linewidth=2, label='CPU Usage')
            ax1.fill_between(timestamps, cpu_usage, alpha=0.3, color='#667eea')
            ax1.set_ylabel('CPU Usage (%)', fontsize=12, fontweight='bold')
            ax1.set_title(f'CPU Usage Over Time - {test_name}', fontsize=14, fontweight='bold')
            ax1.legend(fontsize=10)
            ax1.grid(True, alpha=0.3)
            ax1.set_ylim(0, 100)
            
            # 内存使用率
            ax2.plot(timestamps, memory_usage, color='#764ba2', linewidth=2, label='Memory Usage')
            ax2.fill_between(timestamps, memory_usage, alpha=0.3, color='#764ba2')
            ax2.set_xlabel('Time (seconds)', fontsize=12, fontweight='bold')
            ax2.set_ylabel('Memory Usage (%)', fontsize=12, fontweight='bold')
            ax2.set_title(f'Memory Usage Over Time - {test_name}', fontsize=14, fontweight='bold')
            ax2.legend(fontsize=10)
            ax2.grid(True, alpha=0.3)
            ax2.set_ylim(0, 100)
            
            output_file = self.output_dir / f"{test_name}_resource_usage.png"
            plt.tight_layout()
            plt.savefig(output_file, dpi=150, bbox_inches='tight')
            plt.close()
            
            print(f"✓ Chart saved: {output_file}")
            return str(output_file)
        
        except Exception as e:
            print(f"Error generating resource usage chart: {e}")
            return None
    
    def plot_per_sample_comparison(self, metrics: PerformanceMetrics, test_name: str) -> Optional[str]:
        """绘制每个样本的性能对比"""
        try:
            if not metrics.per_sample_stats:
                return None
            
            # 提取数据
            samples = []
            latencies = []
            
            for sample_name, stats in sorted(metrics.per_sample_stats.items()):
                if 'error' not in stats and stats['successful_runs'] > 0:
                    samples.append(sample_name[:20])  # 截断长文件名
                    latencies.append(stats['avg_latency_ms'])
            
            if not samples:
                return None
            
            fig, ax = plt.subplots(figsize=(max(12, len(samples) * 0.5), 6))
            
            bars = ax.barh(range(len(samples)), latencies, color='#667eea')
            
            # 添加数值标签
            for i, (bar, val) in enumerate(zip(bars, latencies)):
                width = bar.get_width()
                ax.text(width, bar.get_y() + bar.get_height()/2.,
                       f'{val:.2f}ms',
                       ha='left', va='center', fontweight='bold', fontsize=9)
            
            ax.set_yticks(range(len(samples)))
            ax.set_yticklabels(samples, fontsize=9)
            ax.set_xlabel('Average Latency (ms)', fontsize=12, fontweight='bold')
            ax.set_title(f'Per-Sample Performance Comparison - {test_name}', fontsize=14, fontweight='bold')
            ax.grid(True, alpha=0.3, axis='x')
            
            output_file = self.output_dir / f"{test_name}_per_sample_comparison.png"
            plt.tight_layout()
            plt.savefig(output_file, dpi=150, bbox_inches='tight')
            plt.close()
            
            print(f"✓ Chart saved: {output_file}")
            return str(output_file)
        
        except Exception as e:
            print(f"Error generating per-sample comparison chart: {e}")
            return None

