"""
OCR API Server Benchmark Framework - Core Module
"""

__version__ = "2.0.0"
__author__ = "DeepX Team"

from .config import BenchmarkConfig, TestScenario
from .metrics import MetricsCollector, PerformanceMetrics
from .client import AsyncHTTPClient, RequestResult
from .monitor import ResourceMonitor, SystemMetrics

__all__ = [
    "BenchmarkConfig",
    "TestScenario",
    "MetricsCollector",
    "PerformanceMetrics",
    "AsyncHTTPClient",
    "RequestResult",
    "ResourceMonitor",
    "SystemMetrics",
]

