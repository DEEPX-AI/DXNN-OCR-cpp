"""
指标收集和统计模块
提供详细的性能指标计算和分析
"""

import time
import numpy as np
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Any
from collections import defaultdict
from enum import Enum


class RequestStatus(Enum):
    """请求状态"""
    SUCCESS = "success"
    TIMEOUT = "timeout"
    ERROR = "error"
    CANCELLED = "cancelled"


class ErrorCategory(Enum):
    """错误分类"""
    NONE = "none"
    TIMEOUT = "timeout"
    CONNECTION = "connection"
    HTTP_4XX = "http_4xx"
    HTTP_5XX = "http_5xx"
    DECODE = "decode"
    VALIDATION = "validation"
    UNKNOWN = "unknown"


@dataclass
class RequestMetrics:
    """单个请求的指标"""
    request_id: int
    sample_name: str
    status: RequestStatus
    
    # 时间指标
    start_time: float
    end_time: float
    latency_ms: float
    
    # 响应信息
    http_code: int = 0
    error_msg: str = ""
    error_category: ErrorCategory = ErrorCategory.NONE
    
    # OCR 结果
    char_count: int = 0
    page_count: int = 0
    text: str = ""
    
    # 额外信息
    run_index: int = 0
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class LatencyStats:
    """延迟统计"""
    min: float = 0.0
    max: float = 0.0
    mean: float = 0.0
    median: float = 0.0
    std: float = 0.0
    p50: float = 0.0
    p90: float = 0.0
    p95: float = 0.0
    p99: float = 0.0
    p999: float = 0.0
    
    def to_dict(self) -> dict:
        return {
            "min": round(self.min, 2),
            "max": round(self.max, 2),
            "mean": round(self.mean, 2),
            "median": round(self.median, 2),
            "std": round(self.std, 2),
            "p50": round(self.p50, 2),
            "p90": round(self.p90, 2),
            "p95": round(self.p95, 2),
            "p99": round(self.p99, 2),
            "p999": round(self.p999, 2),
        }


@dataclass
class ThroughputStats:
    """吞吐量统计"""
    total_requests: int = 0
    successful_requests: int = 0
    failed_requests: int = 0
    timeout_requests: int = 0
    
    total_duration_ms: float = 0.0
    
    # QPS 指标
    qps: float = 0.0                    # 总体 QPS
    success_qps: float = 0.0            # 成功 QPS
    
    # 成功率
    success_rate: float = 0.0
    
    def to_dict(self) -> dict:
        return {
            "total_requests": self.total_requests,
            "successful_requests": self.successful_requests,
            "failed_requests": self.failed_requests,
            "timeout_requests": self.timeout_requests,
            "total_duration_ms": round(self.total_duration_ms, 2),
            "qps": round(self.qps, 2),
            "success_qps": round(self.success_qps, 2),
            "success_rate": round(self.success_rate, 2),
        }


@dataclass
class OCRStats:
    """OCR 特定统计"""
    total_chars: int = 0
    total_pages: int = 0
    
    # 字符处理速度
    cps: float = 0.0                    # Characters Per Second
    
    # 页面处理速度（PDF）
    pps: float = 0.0                    # Pages Per Second
    
    # 准确率（如果有 ground truth）
    accuracy: Optional[float] = None
    
    def to_dict(self) -> dict:
        d = {
            "total_chars": self.total_chars,
            "total_pages": self.total_pages,
            "cps": round(self.cps, 2),
            "pps": round(self.pps, 2),
        }
        if self.accuracy is not None:
            d["accuracy"] = round(self.accuracy, 2)
        return d


@dataclass
class ErrorStats:
    """错误统计"""
    error_breakdown: Dict[str, int] = field(default_factory=dict)
    error_rate: float = 0.0
    timeout_rate: float = 0.0
    
    def to_dict(self) -> dict:
        return {
            "error_breakdown": self.error_breakdown,
            "error_rate": round(self.error_rate, 2),
            "timeout_rate": round(self.timeout_rate, 2),
        }


@dataclass
class PerformanceMetrics:
    """完整的性能指标"""
    test_name: str
    start_time: float
    end_time: float
    
    latency: LatencyStats = field(default_factory=LatencyStats)
    throughput: ThroughputStats = field(default_factory=ThroughputStats)
    ocr: OCRStats = field(default_factory=OCRStats)
    errors: ErrorStats = field(default_factory=ErrorStats)
    
    # 每个样本的统计
    per_sample_stats: Dict[str, Dict] = field(default_factory=dict)
    
    # 时间序列数据（用于绘图）
    timeline: List[Dict] = field(default_factory=list)
    
    # Stress 测试：每档并发的指标列表 [{"concurrency": n, "qps": ..., "p99": ..., ...}, ...]
    stress_stage_results: Optional[List[Dict]] = None
    # Capacity 测试：每档并发的指标列表 [{"concurrency": n, "qps": ..., "p99": ..., ...}, ...]
    capacity_stage_results: Optional[List[Dict]] = None
    
    def to_dict(self) -> dict:
        d = {
            "test_name": self.test_name,
            "start_time": self.start_time,
            "end_time": self.end_time,
            "duration_ms": round((self.end_time - self.start_time) * 1000, 2),
            "latency": self.latency.to_dict(),
            "throughput": self.throughput.to_dict(),
            "ocr": self.ocr.to_dict(),
            "errors": self.errors.to_dict(),
            "per_sample_stats": self.per_sample_stats,
        }
        if self.stress_stage_results is not None:
            d["stress_stage_results"] = self.stress_stage_results
        if self.capacity_stage_results is not None:
            d["capacity_stage_results"] = self.capacity_stage_results
        return d


class MetricsCollector:
    """指标收集器"""
    
    def __init__(self, test_name: str = "benchmark"):
        self.test_name = test_name
        self.start_time = time.time()
        self.end_time = 0.0
        
        self.requests: List[RequestMetrics] = []
        self._lock = None  # 异步锁，在异步环境中初始化
        
    async def add_request(self, metrics: RequestMetrics):
        """添加请求指标（异步安全）"""
        if self._lock is None:
            import asyncio
            self._lock = asyncio.Lock()
        
        async with self._lock:
            self.requests.append(metrics)
    
    def add_request_sync(self, metrics: RequestMetrics):
        """添加请求指标（同步版本）"""
        self.requests.append(metrics)
    
    def finalize(self):
        """完成收集"""
        self.end_time = time.time()
    
    def compute_metrics(self, ground_truth: Optional[Dict[str, str]] = None) -> PerformanceMetrics:
        """计算所有指标"""
        if self.end_time == 0:
            self.finalize()
        
        metrics = PerformanceMetrics(
            test_name=self.test_name,
            start_time=self.start_time,
            end_time=self.end_time
        )
        
        if not self.requests:
            metrics.stress_stage_results = getattr(self, "stress_stage_results", None)
            metrics.capacity_stage_results = getattr(self, "capacity_stage_results", None)
            return metrics
        
        # 计算延迟统计
        metrics.latency = self._compute_latency_stats()
        
        # 计算吞吐量统计
        metrics.throughput = self._compute_throughput_stats()
        
        # 计算 OCR 统计
        metrics.ocr = self._compute_ocr_stats(ground_truth)
        
        # 计算错误统计
        metrics.errors = self._compute_error_stats()
        
        # 计算每个样本的统计
        metrics.per_sample_stats = self._compute_per_sample_stats()
        
        # 生成时间序列数据
        metrics.timeline = self._generate_timeline()
        
        # Stress/Capacity 每档结果（由对应 Executor 设置）
        metrics.stress_stage_results = getattr(self, "stress_stage_results", None)
        metrics.capacity_stage_results = getattr(self, "capacity_stage_results", None)
        
        return metrics
    
    def _compute_latency_stats(self) -> LatencyStats:
        """计算延迟统计"""
        successful = [r for r in self.requests if r.status == RequestStatus.SUCCESS]
        
        if not successful:
            return LatencyStats()
        
        latencies = np.array([r.latency_ms for r in successful])
        
        return LatencyStats(
            min=float(np.min(latencies)),
            max=float(np.max(latencies)),
            mean=float(np.mean(latencies)),
            median=float(np.median(latencies)),
            std=float(np.std(latencies)),
            p50=float(np.percentile(latencies, 50)),
            p90=float(np.percentile(latencies, 90)),
            p95=float(np.percentile(latencies, 95)),
            p99=float(np.percentile(latencies, 99)),
            p999=float(np.percentile(latencies, 99.9)),
        )
    
    def _compute_throughput_stats(self) -> ThroughputStats:
        """计算吞吐量统计"""
        stats = ThroughputStats()
        
        stats.total_requests = len(self.requests)
        stats.successful_requests = sum(1 for r in self.requests if r.status == RequestStatus.SUCCESS)
        stats.failed_requests = sum(1 for r in self.requests if r.status == RequestStatus.ERROR)
        stats.timeout_requests = sum(1 for r in self.requests if r.status == RequestStatus.TIMEOUT)
        
        stats.total_duration_ms = (self.end_time - self.start_time) * 1000
        
        if stats.total_duration_ms > 0:
            stats.qps = stats.total_requests * 1000.0 / stats.total_duration_ms
            stats.success_qps = stats.successful_requests * 1000.0 / stats.total_duration_ms
        
        if stats.total_requests > 0:
            stats.success_rate = stats.successful_requests * 100.0 / stats.total_requests
        
        return stats
    
    def _compute_ocr_stats(self, ground_truth: Optional[Dict[str, str]] = None) -> OCRStats:
        """计算 OCR 统计"""
        stats = OCRStats()
        
        successful = [r for r in self.requests if r.status == RequestStatus.SUCCESS]
        
        if not successful:
            return stats
        
        stats.total_chars = sum(r.char_count for r in successful)
        stats.total_pages = sum(r.page_count for r in successful)
        
        duration_s = (self.end_time - self.start_time)
        if duration_s > 0:
            stats.cps = stats.total_chars / duration_s
            stats.pps = stats.total_pages / duration_s
        
        # 计算准确率
        if ground_truth:
            stats.accuracy = self._compute_accuracy(successful, ground_truth)
        
        return stats
    
    def _compute_accuracy(self, requests: List[RequestMetrics], 
                         ground_truth: Dict[str, str]) -> float:
        """计算字符级准确率"""
        import difflib
        
        total_ratio = 0.0
        count = 0
        
        for req in requests:
            if req.sample_name in ground_truth:
                predicted = req.text
                expected = ground_truth[req.sample_name]
                
                matcher = difflib.SequenceMatcher(None, predicted, expected)
                total_ratio += matcher.ratio()
                count += 1
        
        return (total_ratio / count * 100) if count > 0 else 0.0
    
    def _compute_error_stats(self) -> ErrorStats:
        """计算错误统计"""
        stats = ErrorStats()
        
        # 错误分类统计
        error_breakdown = defaultdict(int)
        for req in self.requests:
            error_breakdown[req.error_category.value] += 1
        
        stats.error_breakdown = dict(error_breakdown)
        
        total = len(self.requests)
        if total > 0:
            failed = sum(1 for r in self.requests if r.status == RequestStatus.ERROR)
            timeout = sum(1 for r in self.requests if r.status == RequestStatus.TIMEOUT)
            
            stats.error_rate = failed * 100.0 / total
            stats.timeout_rate = timeout * 100.0 / total
        
        return stats
    
    def _compute_per_sample_stats(self) -> Dict[str, Dict]:
        """计算每个样本的统计"""
        samples = defaultdict(list)
        
        for req in self.requests:
            samples[req.sample_name].append(req)
        
        per_sample = {}
        for sample_name, reqs in samples.items():
            successful = [r for r in reqs if r.status == RequestStatus.SUCCESS]
            
            if successful:
                latencies = [r.latency_ms for r in successful]
                per_sample[sample_name] = {
                    "total_runs": len(reqs),
                    "successful_runs": len(successful),
                    "avg_latency_ms": round(np.mean(latencies), 2),
                    "min_latency_ms": round(np.min(latencies), 2),
                    "max_latency_ms": round(np.max(latencies), 2),
                    "char_count": successful[0].char_count,
                    "page_count": successful[0].page_count,
                }
            else:
                per_sample[sample_name] = {
                    "total_runs": len(reqs),
                    "successful_runs": 0,
                    "error": "All runs failed"
                }
        
        return per_sample
    
    def _generate_timeline(self) -> List[Dict]:
        """生成时间序列数据"""
        timeline = []
        
        for req in self.requests:
            timeline.append({
                "timestamp": req.start_time,
                "relative_time_ms": (req.start_time - self.start_time) * 1000,
                "latency_ms": req.latency_ms,
                "status": req.status.value,
                "sample_name": req.sample_name,
            })
        
        return sorted(timeline, key=lambda x: x["timestamp"])
    
    def get_summary(self) -> str:
        """获取简要摘要"""
        metrics = self.compute_metrics()
        
        lines = [
            f"Test: {self.test_name}",
            f"Duration: {metrics.throughput.total_duration_ms:.2f} ms",
            f"Total Requests: {metrics.throughput.total_requests}",
            f"Success Rate: {metrics.throughput.success_rate:.2f}%",
            f"QPS: {metrics.throughput.qps:.2f}",
            f"Avg Latency: {metrics.latency.mean:.2f} ms",
            f"P99 Latency: {metrics.latency.p99:.2f} ms",
        ]
        
        return "\n".join(lines)

