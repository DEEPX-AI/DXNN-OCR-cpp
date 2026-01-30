"""
配置管理模块
支持 YAML/JSON 配置文件和命令行参数
"""

import json
import yaml
from dataclasses import dataclass, field, asdict
from typing import Dict, List, Optional, Any
from pathlib import Path
from enum import Enum


class TestMode(Enum):
    """测试模式"""
    LATENCY = "latency"           # 延迟测试（串行）
    THROUGHPUT = "throughput"     # 吞吐量测试（并发）
    STRESS = "stress"             # 压力测试（逐步增加负载）
    STABILITY = "stability"       # 稳定性测试（长时间运行）
    CAPACITY = "capacity"         # 容量规划（找到最优并发数）


class FileType(Enum):
    """文件类型"""
    IMAGE = 1
    PDF = 0


@dataclass
class ServerConfig:
    """服务器配置"""
    url: str = "http://localhost:8080/ocr"
    token: str = "test_token"
    timeout: int = 60
    verify_ssl: bool = False
    
    def to_dict(self) -> dict:
        return asdict(self)


@dataclass
class OCRParams:
    """OCR 参数配置"""
    fileType: int = 1
    useDocOrientationClassify: bool = False
    useDocUnwarping: bool = False
    useTextlineOrientation: bool = False
    textDetThresh: float = 0.3
    textDetBoxThresh: float = 0.6
    textDetUnclipRatio: float = 1.5
    textRecScoreThresh: float = 0.0
    visualize: bool = False
    
    # PDF 专用参数
    pdfDpi: int = 150
    pdfMaxPages: int = 10
    
    def to_dict(self) -> dict:
        return asdict(self)


@dataclass
class TestScenario:
    """测试场景配置"""
    name: str
    mode: TestMode
    concurrency: int = 1
    duration_seconds: int = 0          # 0 表示运行完所有样本
    runs_per_sample: int = 1
    warmup_requests: int = 5
    ramp_up_seconds: int = 0           # 压力测试的爬坡时间
    max_concurrency: int = 100         # 容量规划的最大并发数
    concurrency_step: int = 5          # 容量规划的并发步长
    
    def to_dict(self) -> dict:
        d = asdict(self)
        d['mode'] = self.mode.value
        return d
    
    @classmethod
    def from_dict(cls, data: dict) -> 'TestScenario':
        if 'mode' in data and isinstance(data['mode'], str):
            data['mode'] = TestMode(data['mode'])
        return cls(**data)


@dataclass
class DataConfig:
    """数据配置"""
    images_dir: Optional[str] = None
    pdfs_dir: Optional[str] = None
    labels_file: Optional[str] = None
    file_pattern: str = "*.*"
    max_samples: int = 0               # 0 表示使用所有样本
    shuffle: bool = False
    
    def to_dict(self) -> dict:
        return asdict(self)


@dataclass
class MonitorConfig:
    """监控配置"""
    enable_system_monitor: bool = True
    enable_npu_monitor: bool = True
    monitor_interval: float = 1.0      # 监控采样间隔（秒）
    
    def to_dict(self) -> dict:
        return asdict(self)


@dataclass
class ReportConfig:
    """报告配置"""
    output_dir: str = "results"
    formats: List[str] = field(default_factory=lambda: ["markdown", "json", "html"])
    save_timeline: bool = True
    save_raw_results: bool = False
    generate_charts: bool = True
    
    def to_dict(self) -> dict:
        return asdict(self)


@dataclass
class BenchmarkConfig:
    """基准测试总配置"""
    server: ServerConfig = field(default_factory=ServerConfig)
    ocr_params: OCRParams = field(default_factory=OCRParams)
    scenario: TestScenario = field(default_factory=lambda: TestScenario(
        name="default", mode=TestMode.THROUGHPUT, concurrency=10
    ))
    data: DataConfig = field(default_factory=DataConfig)
    monitor: MonitorConfig = field(default_factory=MonitorConfig)
    report: ReportConfig = field(default_factory=ReportConfig)
    
    def to_dict(self) -> dict:
        return {
            "server": self.server.to_dict(),
            "ocr_params": self.ocr_params.to_dict(),
            "scenario": self.scenario.to_dict(),
            "data": self.data.to_dict(),
            "monitor": self.monitor.to_dict(),
            "report": self.report.to_dict(),
        }
    
    def to_json(self, filepath: str):
        """保存为 JSON 文件"""
        with open(filepath, 'w', encoding='utf-8') as f:
            json.dump(self.to_dict(), f, indent=2, ensure_ascii=False)
    
    def to_yaml(self, filepath: str):
        """保存为 YAML 文件"""
        with open(filepath, 'w', encoding='utf-8') as f:
            yaml.dump(self.to_dict(), f, default_flow_style=False, allow_unicode=True)
    
    @classmethod
    def from_dict(cls, data: dict) -> 'BenchmarkConfig':
        """从字典创建配置"""
        return cls(
            server=ServerConfig(**data.get("server", {})),
            ocr_params=OCRParams(**data.get("ocr_params", {})),
            scenario=TestScenario.from_dict(data.get("scenario", {})),
            data=DataConfig(**data.get("data", {})),
            monitor=MonitorConfig(**data.get("monitor", {})),
            report=ReportConfig(**data.get("report", {})),
        )
    
    @classmethod
    def from_json(cls, filepath: str) -> 'BenchmarkConfig':
        """从 JSON 文件加载配置"""
        with open(filepath, 'r', encoding='utf-8') as f:
            data = json.load(f)
        return cls.from_dict(data)
    
    @classmethod
    def from_yaml(cls, filepath: str) -> 'BenchmarkConfig':
        """从 YAML 文件加载配置"""
        with open(filepath, 'r', encoding='utf-8') as f:
            data = yaml.safe_load(f)
        return cls.from_dict(data)
    
    def validate(self) -> tuple[bool, str]:
        """验证配置有效性"""
        # 验证服务器配置
        if not self.server.url:
            return False, "Server URL is required"
        
        # 验证数据配置
        if not self.data.images_dir and not self.data.pdfs_dir:
            return False, "At least one of images_dir or pdfs_dir must be specified"
        
        # 验证测试场景
        if self.scenario.concurrency < 1:
            return False, "Concurrency must be >= 1"
        
        if self.scenario.mode in (TestMode.CAPACITY, TestMode.STRESS):
            if self.scenario.max_concurrency < self.scenario.concurrency:
                return False, "max_concurrency must be >= concurrency"
            if self.scenario.concurrency_step < 1:
                return False, "concurrency_step must be >= 1"
        
        if self.scenario.mode == TestMode.STABILITY and self.scenario.duration_seconds <= 0:
            # 允许 0，执行器内会使用默认 600s
            pass
        
        # 验证输出目录
        if not self.report.output_dir:
            return False, "Output directory is required"
        
        return True, "Configuration is valid"


def create_default_config(output_path: str = "benchmark_config.yaml"):
    """创建默认配置文件"""
    config = BenchmarkConfig()
    
    # 设置一些示例值
    config.data.images_dir = "../../images"
    config.scenario.name = "throughput_test"
    config.scenario.mode = TestMode.THROUGHPUT
    config.scenario.concurrency = 10
    
    if output_path.endswith('.json'):
        config.to_json(output_path)
    else:
        config.to_yaml(output_path)
    
    print(f"✓ Default configuration saved to: {output_path}")
    return config


if __name__ == "__main__":
    # 生成示例配置文件
    create_default_config("benchmark_config.yaml")
    create_default_config("benchmark_config.json")

