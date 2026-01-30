"""
资源监控模块
监控系统资源使用情况（CPU、内存、NPU）
"""

import time
import psutil
import asyncio
from dataclasses import dataclass, field
from typing import List, Optional, Dict
from pathlib import Path


@dataclass
class SystemMetrics:
    """系统指标快照"""
    timestamp: float
    
    # CPU
    cpu_percent: float = 0.0
    cpu_count: int = 0
    
    # 内存
    memory_used_mb: float = 0.0
    memory_total_mb: float = 0.0
    memory_percent: float = 0.0
    
    # NPU（如果可用）
    npu_utilization: float = 0.0
    npu_memory_used_mb: float = 0.0
    npu_memory_total_mb: float = 0.0
    
    def to_dict(self) -> dict:
        return {
            "timestamp": self.timestamp,
            "cpu_percent": round(self.cpu_percent, 2),
            "cpu_count": self.cpu_count,
            "memory_used_mb": round(self.memory_used_mb, 2),
            "memory_total_mb": round(self.memory_total_mb, 2),
            "memory_percent": round(self.memory_percent, 2),
            "npu_utilization": round(self.npu_utilization, 2),
            "npu_memory_used_mb": round(self.npu_memory_used_mb, 2),
            "npu_memory_total_mb": round(self.npu_memory_total_mb, 2),
        }


class ResourceMonitor:
    """资源监控器"""
    
    def __init__(self, interval: float = 1.0, enable_npu: bool = True):
        """
        初始化监控器
        
        Args:
            interval: 采样间隔（秒）
            enable_npu: 是否启用 NPU 监控
        """
        self.interval = interval
        self.enable_npu = enable_npu
        
        self.metrics_history: List[SystemMetrics] = []
        self._running = False
        self._task: Optional[asyncio.Task] = None
        
        # NPU 监控相关
        self._npu_available = False
        if enable_npu:
            self._check_npu_availability()
    
    def _check_npu_availability(self):
        """检查 NPU 是否可用"""
        # 检查 DXRT 相关的 sysfs 接口
        npu_paths = [
            "/sys/class/dxrt/dxrt0/utilization",
            "/sys/class/dxrt/dxrt0/memory_used",
            "/sys/class/dxrt/dxrt0/memory_total",
        ]
        
        self._npu_available = all(Path(p).exists() for p in npu_paths)
        
        if self._npu_available:
            print("[Monitor] NPU monitoring enabled (DXRT detected)")
        else:
            print("[Monitor] NPU monitoring disabled (DXRT not detected)")
    
    def _read_npu_metrics(self) -> tuple[float, float, float]:
        """
        读取 NPU 指标
        
        Returns:
            (utilization, memory_used_mb, memory_total_mb)
        """
        if not self._npu_available:
            return 0.0, 0.0, 0.0
        
        try:
            # 读取利用率
            with open("/sys/class/dxrt/dxrt0/utilization", "r") as f:
                utilization = float(f.read().strip())
            
            # 读取内存使用
            with open("/sys/class/dxrt/dxrt0/memory_used", "r") as f:
                memory_used = int(f.read().strip()) / (1024 * 1024)  # 转换为 MB
            
            # 读取总内存
            with open("/sys/class/dxrt/dxrt0/memory_total", "r") as f:
                memory_total = int(f.read().strip()) / (1024 * 1024)  # 转换为 MB
            
            return utilization, memory_used, memory_total
        
        except Exception as e:
            print(f"[Monitor] Failed to read NPU metrics: {e}")
            return 0.0, 0.0, 0.0
    
    def _collect_metrics(self) -> SystemMetrics:
        """收集当前系统指标"""
        metrics = SystemMetrics(timestamp=time.time())
        
        # CPU
        metrics.cpu_percent = psutil.cpu_percent(interval=0.1)
        metrics.cpu_count = psutil.cpu_count()
        
        # 内存
        mem = psutil.virtual_memory()
        metrics.memory_used_mb = mem.used / (1024 * 1024)
        metrics.memory_total_mb = mem.total / (1024 * 1024)
        metrics.memory_percent = mem.percent
        
        # NPU
        if self.enable_npu and self._npu_available:
            npu_util, npu_mem_used, npu_mem_total = self._read_npu_metrics()
            metrics.npu_utilization = npu_util
            metrics.npu_memory_used_mb = npu_mem_used
            metrics.npu_memory_total_mb = npu_mem_total
        
        return metrics
    
    async def _monitor_loop(self):
        """监控循环"""
        while self._running:
            try:
                metrics = self._collect_metrics()
                self.metrics_history.append(metrics)
                await asyncio.sleep(self.interval)
            except asyncio.CancelledError:
                break
            except Exception as e:
                print(f"[Monitor] Error in monitor loop: {e}")
                await asyncio.sleep(self.interval)
    
    async def start(self):
        """启动监控"""
        if self._running:
            return
        
        self._running = True
        self._task = asyncio.create_task(self._monitor_loop())
        print(f"[Monitor] Started (interval={self.interval}s)")
    
    async def stop(self):
        """停止监控"""
        if not self._running:
            return
        
        self._running = False
        if self._task:
            self._task.cancel()
            try:
                await self._task
            except asyncio.CancelledError:
                pass
        
        print(f"[Monitor] Stopped (collected {len(self.metrics_history)} samples)")
    
    def get_summary(self) -> Dict:
        """获取监控摘要"""
        if not self.metrics_history:
            return {}
        
        import numpy as np
        
        cpu_values = [m.cpu_percent for m in self.metrics_history]
        mem_values = [m.memory_percent for m in self.metrics_history]
        
        summary = {
            "cpu": {
                "avg": round(np.mean(cpu_values), 2),
                "max": round(np.max(cpu_values), 2),
                "min": round(np.min(cpu_values), 2),
            },
            "memory": {
                "avg": round(np.mean(mem_values), 2),
                "max": round(np.max(mem_values), 2),
                "min": round(np.min(mem_values), 2),
            },
            "samples": len(self.metrics_history),
        }
        
        # NPU 统计
        if self.enable_npu and self._npu_available:
            npu_util_values = [m.npu_utilization for m in self.metrics_history]
            npu_mem_values = [m.npu_memory_used_mb for m in self.metrics_history]
            
            summary["npu"] = {
                "utilization": {
                    "avg": round(np.mean(npu_util_values), 2),
                    "max": round(np.max(npu_util_values), 2),
                    "min": round(np.min(npu_util_values), 2),
                },
                "memory_mb": {
                    "avg": round(np.mean(npu_mem_values), 2),
                    "max": round(np.max(npu_mem_values), 2),
                    "min": round(np.min(npu_mem_values), 2),
                }
            }
        
        return summary
    
    def get_timeline(self) -> List[Dict]:
        """获取时间序列数据"""
        return [m.to_dict() for m in self.metrics_history]
    
    def clear(self):
        """清空历史数据"""
        self.metrics_history.clear()

