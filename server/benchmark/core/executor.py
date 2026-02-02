"""
测试执行器模块
实现不同的测试场景（延迟、吞吐量、压力、稳定性、容量规划）
"""

import json
import time
import asyncio
import base64
import itertools
from pathlib import Path
from typing import List, Dict, Optional, Tuple
import numpy as np
from .config import BenchmarkConfig, TestMode
from .client import AsyncHTTPClient
from .metrics import MetricsCollector, RequestMetrics, RequestStatus, ErrorCategory
from .monitor import ResourceMonitor


def _compute_stage_stats(requests: List[RequestMetrics], start_time: float, end_time: float) -> Dict:
    """根据某阶段的请求列表计算 QPS、P99、成功率等，用于 Stress/Capacity 每档汇总"""
    total = len(requests)
    successful = [r for r in requests if r.status == RequestStatus.SUCCESS]
    duration_s = max((end_time - start_time), 1e-6)
    qps = total / duration_s
    success_qps = len(successful) / duration_s
    success_rate = (len(successful) / total * 100.0) if total else 0.0
    p99 = float(np.percentile([r.latency_ms for r in successful], 99)) if successful else 0.0
    p95 = float(np.percentile([r.latency_ms for r in successful], 95)) if successful else 0.0
    return {
        "total_requests": total,
        "successful_requests": len(successful),
        "duration_seconds": round(duration_s, 2),
        "qps": round(qps, 2),
        "success_qps": round(success_qps, 2),
        "success_rate": round(success_rate, 2),
        "p95_ms": round(p95, 2),
        "p99_ms": round(p99, 2),
    }


class TestExecutor:
    """测试执行器基类"""
    
    def __init__(self, config: BenchmarkConfig):
        self.config = config
        self.samples: List[Dict] = []
        self.ground_truth: Dict[str, str] = {}
    
    def _ocr_params_for_sample(self, sample: Dict) -> Dict:
        """按样本类型生成 OCR 参数（fileType: 1=图像, 0=PDF），与 Server API 一致"""
        params = self.config.ocr_params.to_dict()
        params["fileType"] = 0 if sample.get("type") == "pdf" else 1
        return params

    def _build_body_by_name(self) -> Dict[str, bytes]:
        """预计算 Body 缓存"""
        return {
            s["name"]: json.dumps(
                {"file": s["base64"], **self._ocr_params_for_sample(s)},
                ensure_ascii=False,
            ).encode("utf-8")
            for s in self.samples
        }

    def load_samples(self):
        """加载测试样本"""
        if self.config.data.images_dir:
            self._load_images(self.config.data.images_dir)
        
        if self.config.data.pdfs_dir:
            self._load_pdfs(self.config.data.pdfs_dir)
        
        # 加载 ground truth
        if self.config.data.labels_file:
            self._load_ground_truth(self.config.data.labels_file)
        
        # 限制样本数量
        if self.config.data.max_samples > 0:
            self.samples = self.samples[:self.config.data.max_samples]
        
        # 打乱顺序
        if self.config.data.shuffle:
            import random
            random.shuffle(self.samples)
        
        print(f"[Executor] Loaded {len(self.samples)} samples")
    
    def _load_images(self, images_dir: str):
        """加载图片样本"""
        path = Path(images_dir)
        if not path.exists():
            print(f"Warning: Images directory not found: {images_dir}")
            return
        
        extensions = ['*.png', '*.jpg', '*.jpeg', '*.bmp']
        for ext in extensions:
            for img_file in sorted(path.glob(ext)):
                try:
                    with open(img_file, 'rb') as f:
                        img_data = f.read()
                        base64_data = base64.b64encode(img_data).decode('utf-8')
                        
                        self.samples.append({
                            'name': img_file.name,
                            'type': 'image',
                            'base64': base64_data,
                            'size_mb': len(img_data) / (1024 * 1024)
                        })
                except Exception as e:
                    print(f"Error loading {img_file}: {e}")
    
    def _load_pdfs(self, pdfs_dir: str):
        """加载 PDF 样本"""
        path = Path(pdfs_dir)
        if not path.exists():
            print(f"Warning: PDFs directory not found: {pdfs_dir}")
            return
        
        for pdf_file in sorted(path.glob('*.pdf')):
            try:
                with open(pdf_file, 'rb') as f:
                    pdf_data = f.read()
                    base64_data = base64.b64encode(pdf_data).decode('utf-8')
                    
                    self.samples.append({
                        'name': pdf_file.name,
                        'type': 'pdf',
                        'base64': base64_data,
                        'size_mb': len(pdf_data) / (1024 * 1024)
                    })
            except Exception as e:
                print(f"Error loading {pdf_file}: {e}")
    
    def _load_ground_truth(self, labels_file: str):
        """加载 ground truth 标签"""
        import json
        
        path = Path(labels_file)
        if not path.exists():
            print(f"Warning: Labels file not found: {labels_file}")
            return
        
        try:
            with open(path, 'r', encoding='utf-8') as f:
                raw_labels = json.load(f)
            
            for filename, value in raw_labels.items():
                if isinstance(value, list):
                    # 格式: [{"text": "..."}, ...]
                    texts = [box.get("text", "") for box in value if isinstance(box, dict)]
                    self.ground_truth[filename] = "".join(texts)
                elif isinstance(value, str):
                    # 格式: "text"
                    self.ground_truth[filename] = value
            
            print(f"[Executor] Loaded ground truth for {len(self.ground_truth)} samples")
        except Exception as e:
            print(f"Warning: Failed to load labels: {e}")
    
    async def run(self) -> Tuple[MetricsCollector, ResourceMonitor]:
        """运行测试（子类实现）"""
        raise NotImplementedError


class LatencyTestExecutor(TestExecutor):
    """延迟测试执行器（串行模式）"""
    
    async def run(self) -> Tuple[MetricsCollector, ResourceMonitor]:
        """运行延迟测试"""
        print("\n" + "=" * 70)
        print("LATENCY TEST (Serial Mode)")
        print("=" * 70)
        print(f"Samples: {len(self.samples)}")
        print(f"Runs per sample: {self.config.scenario.runs_per_sample}")
        print("=" * 70 + "\n")
        
        collector = MetricsCollector(test_name=self.config.scenario.name)
        monitor = ResourceMonitor(interval=self.config.monitor.monitor_interval)
        
        if self.config.monitor.enable_system_monitor:
            await monitor.start()
        
        async with AsyncHTTPClient(
            base_url=self.config.server.url,
            token=self.config.server.token,
            timeout=self.config.server.timeout,
            max_connections=1,
            verify_ssl=self.config.server.verify_ssl
        ) as client:
            # 健康检查
            await client.health_check()
            
            # 预热
            if self.config.scenario.warmup_requests > 0 and self.samples:
                await client.warmup(
                    self.samples[0]['base64'],
                    self._ocr_params_for_sample(self.samples[0]),
                    self.config.scenario.warmup_requests,
                    concurrency=1
                )
            
            body_by_name = self._build_body_by_name()
            # 串行执行
            request_id = 0
            for idx, sample in enumerate(self.samples):
                print(f"[{idx+1}/{len(self.samples)}] Testing {sample['name']}...")
                
                for run_idx in range(self.config.scenario.runs_per_sample):
                    result = await client.send_ocr_request(
                        body_by_name[sample['name']],
                        request_id
                    )
                    
                    # 记录指标
                    metrics = RequestMetrics(
                        request_id=request_id,
                        sample_name=sample['name'],
                        status=RequestStatus.SUCCESS if result.success else RequestStatus.ERROR,
                        start_time=time.time() - result.latency_ms / 1000,
                        end_time=time.time(),
                        latency_ms=result.latency_ms,
                        http_code=result.status_code,
                        error_msg=result.error_msg,
                        error_category=result.error_category,
                        char_count=result.char_count,
                        page_count=result.page_count,
                        text=result.text,
                        run_index=run_idx
                    )
                    
                    await collector.add_request(metrics)
                    request_id += 1
                    
                    if result.success:
                        print(f"  Run {run_idx+1}: {result.latency_ms:.2f}ms, {result.char_count} chars ✓")
                    else:
                        print(f"  Run {run_idx+1}: {result.error_msg} ✗")
        
        if self.config.monitor.enable_system_monitor:
            await monitor.stop()
        
        collector.finalize()
        return collector, monitor


class ThroughputTestExecutor(TestExecutor):
    """吞吐量测试执行器（并发模式）"""
    
    async def run(self) -> Tuple[MetricsCollector, ResourceMonitor]:
        """运行吞吐量测试"""
        print("\n" + "=" * 70)
        print("THROUGHPUT TEST (Concurrent Mode)")
        print("=" * 70)
        print(f"Samples: {len(self.samples)}")
        print(f"Runs per sample: {self.config.scenario.runs_per_sample}")
        print(f"Concurrency: {self.config.scenario.concurrency}")
        print("=" * 70 + "\n")
        
        collector = MetricsCollector(test_name=self.config.scenario.name)
        monitor = ResourceMonitor(interval=self.config.monitor.monitor_interval)
        
        if self.config.monitor.enable_system_monitor:
            await monitor.start()
        
        async with AsyncHTTPClient(
            base_url=self.config.server.url,
            token=self.config.server.token,
            timeout=self.config.server.timeout,
            max_connections=self.config.scenario.concurrency * 2,
            verify_ssl=self.config.server.verify_ssl
        ) as client:
            # 健康检查
            await client.health_check()
            
            # 预热
            if self.config.scenario.warmup_requests > 0 and self.samples:
                await client.warmup(
                    self.samples[0]['base64'],
                    self._ocr_params_for_sample(self.samples[0]),
                    self.config.scenario.warmup_requests,
                    concurrency=self.config.scenario.concurrency
                )
            
            body_by_name = self._build_body_by_name()
            # 构建任务列表
            tasks = []
            request_id = 0
            for run_idx in range(self.config.scenario.runs_per_sample):
                for sample in self.samples:
                    tasks.append({
                        'request_id': request_id,
                        'sample': sample,
                        'run_idx': run_idx,
                        'body': body_by_name[sample['name']]
                    })
                    request_id += 1
            
            total_tasks = len(tasks)
            print(f"[Executor] Total tasks: {total_tasks}\n")
            
            # 使用信号量控制并发
            semaphore = asyncio.Semaphore(self.config.scenario.concurrency)
            completed = [0]
            
            async def bounded_request(task_info):
                """带并发限制的请求"""
                async with semaphore:
                    start_time = time.time()
                    result = await client.send_ocr_request(
                        task_info['body'],
                        task_info['request_id']
                    )
                    sample = task_info['sample']
                    
                    end_time = time.time()
                    
                    # 记录指标
                    metrics = RequestMetrics(
                        request_id=task_info['request_id'],
                        sample_name=task_info['sample']['name'],
                        status=RequestStatus.SUCCESS if result.success else RequestStatus.ERROR,
                        start_time=start_time,
                        end_time=end_time,
                        latency_ms=result.latency_ms,
                        http_code=result.status_code,
                        error_msg=result.error_msg,
                        error_category=result.error_category,
                        char_count=result.char_count,
                        page_count=result.page_count,
                        text=result.text,
                        run_index=task_info['run_idx']
                    )
                    
                    await collector.add_request(metrics)
                    
                    completed[0] += 1
                    if completed[0] % 10 == 0 or completed[0] == total_tasks:
                        progress = completed[0] * 100 // total_tasks
                        print(f"\rProgress: {completed[0]}/{total_tasks} ({progress}%)", end="", flush=True)
            
            # 并发执行所有任务
            await asyncio.gather(*[bounded_request(task) for task in tasks])
            print()
        
        if self.config.monitor.enable_system_monitor:
            await monitor.stop()
        
        collector.finalize()
        return collector, monitor


class StressTestExecutor(TestExecutor):
    """压力测试执行器：从低并发逐步加压到 max_concurrency，每档运行 ramp_up_seconds（或一整轮样本），记录每档 QPS/延迟/错误率"""
    
    async def run(self) -> Tuple[MetricsCollector, ResourceMonitor]:
        """运行压力测试"""
        step = max(1, self.config.scenario.concurrency_step)
        ramp = max(0, self.config.scenario.ramp_up_seconds)
        per_stage_duration = ramp if ramp > 0 else None
        
        print("\n" + "=" * 70)
        print("STRESS TEST (Gradual Load Increase)")
        print("=" * 70)
        print(f"Samples: {len(self.samples)}")
        print(f"Concurrency: {self.config.scenario.concurrency} -> {self.config.scenario.max_concurrency} (step {step})")
        print(f"Per-stage: {per_stage_duration}s" if per_stage_duration else "Per-stage: one full pass over samples")
        print("=" * 70 + "\n")
        
        collector = MetricsCollector(test_name=self.config.scenario.name)
        collector.stress_stage_results = []
        monitor = ResourceMonitor(interval=self.config.monitor.monitor_interval)
        
        if self.config.monitor.enable_system_monitor:
            await monitor.start()
        
        async with AsyncHTTPClient(
            base_url=self.config.server.url,
            token=self.config.server.token,
            timeout=self.config.server.timeout,
            max_connections=self.config.scenario.max_concurrency * 2,
            verify_ssl=self.config.server.verify_ssl
        ) as client:
            await client.health_check()
            if self.config.scenario.warmup_requests > 0 and self.samples:
                await client.warmup(
                    self.samples[0]["base64"],
                    self._ocr_params_for_sample(self.samples[0]),
                    self.config.scenario.warmup_requests,
                    concurrency=1,
                )
            
            body_by_name = self._build_body_by_name()
            request_id_gen = [0]
            samples_cycle = itertools.cycle(self.samples)
            
            for concurrency in range(
                self.config.scenario.concurrency,
                self.config.scenario.max_concurrency + 1,
                step
            ):
                stage_start = time.time()
                semaphore = asyncio.Semaphore(concurrency)
                
                if per_stage_duration and per_stage_duration > 0:
                    # 固定时长：多个 worker 在 stage_end 前持续发请求
                    stage_end = stage_start + per_stage_duration
                    
                    async def worker(stage_end_ts: float, cur_concurrency: int):
                        while time.time() < stage_end_ts:
                            async with semaphore:
                                sample = next(samples_cycle)
                                rid = request_id_gen[0]
                                request_id_gen[0] += 1
                                start = time.time()
                                result = await client.send_ocr_request(
                                    body_by_name[sample["name"]],
                                    rid,
                                )
                                end = time.time()
                                m = RequestMetrics(
                                    request_id=rid,
                                    sample_name=sample["name"],
                                    status=RequestStatus.SUCCESS if result.success else RequestStatus.ERROR,
                                    start_time=start,
                                    end_time=end,
                                    latency_ms=result.latency_ms,
                                    http_code=result.status_code,
                                    error_msg=result.error_msg,
                                    error_category=result.error_category,
                                    char_count=result.char_count,
                                    page_count=result.page_count,
                                    text=result.text,
                                    run_index=0,
                                    metadata={"stress_stage": cur_concurrency},
                                )
                                await collector.add_request(m)
                    
                    workers = [asyncio.create_task(worker(stage_end, concurrency)) for _ in range(concurrency)]
                    await asyncio.gather(*workers)
                else:
                    # 每档一轮：固定任务数（与 throughput 单轮等价）
                    tasks = []
                    for _ in range(self.config.scenario.runs_per_sample):
                        for s in self.samples:
                            tasks.append((s, request_id_gen[0]))
                            request_id_gen[0] += 1
                    for sample, rid in tasks:
                        async with semaphore:
                            start = time.time()
                            result = await client.send_ocr_request(
                                body_by_name[sample["name"]],
                                rid,
                            )
                            end = time.time()
                            m = RequestMetrics(
                                request_id=rid,
                                sample_name=sample["name"],
                                status=RequestStatus.SUCCESS if result.success else RequestStatus.ERROR,
                                start_time=start,
                                end_time=end,
                                latency_ms=result.latency_ms,
                                http_code=result.status_code,
                                error_msg=result.error_msg,
                                error_category=result.error_category,
                                char_count=result.char_count,
                                page_count=result.page_count,
                                text=result.text,
                                run_index=0,
                                metadata={"stress_stage": concurrency},
                            )
                            await collector.add_request(m)
                
                stage_end_time = time.time()
                stage_requests = [r for r in collector.requests if r.metadata.get("stress_stage") == concurrency]
                stage_stats = _compute_stage_stats(stage_requests, stage_start, stage_end_time)
                stage_stats["concurrency"] = concurrency
                collector.stress_stage_results.append(stage_stats)
                print(f"  Concurrency {concurrency:3d}: QPS={stage_stats['qps']:.2f}, P99={stage_stats['p99_ms']:.2f}ms, Success={stage_stats['success_rate']:.2f}%")
        
        if self.config.monitor.enable_system_monitor:
            await monitor.stop()
        
        collector.finalize()
        return collector, monitor


class StabilityTestExecutor(TestExecutor):
    """稳定性测试执行器：固定并发下长时间运行 duration_seconds，观察 QPS/延迟/错误率是否退化"""
    
    async def run(self) -> Tuple[MetricsCollector, ResourceMonitor]:
        """运行稳定性测试"""
        duration = self.config.scenario.duration_seconds
        if duration <= 0:
            duration = 600  # 默认 10 分钟
        concurrency = max(1, self.config.scenario.concurrency)
        
        print("\n" + "=" * 70)
        print("STABILITY TEST (Fixed Load, Long Duration)")
        print("=" * 70)
        print(f"Samples: {len(self.samples)} (round-robin)")
        print(f"Concurrency: {concurrency}")
        print(f"Duration: {duration}s")
        print("=" * 70 + "\n")
        
        collector = MetricsCollector(test_name=self.config.scenario.name)
        monitor = ResourceMonitor(interval=self.config.monitor.monitor_interval)
        
        if self.config.monitor.enable_system_monitor:
            await monitor.start()
        
        async with AsyncHTTPClient(
            base_url=self.config.server.url,
            token=self.config.server.token,
            timeout=self.config.server.timeout,
            max_connections=concurrency * 2,
            verify_ssl=self.config.server.verify_ssl
        ) as client:
            await client.health_check()
            if self.config.scenario.warmup_requests > 0 and self.samples:
                await client.warmup(
                    self.samples[0]["base64"],
                    self._ocr_params_for_sample(self.samples[0]),
                    self.config.scenario.warmup_requests,
                    concurrency=1,
                )
            
            body_by_name = self._build_body_by_name()
            samples_cycle = itertools.cycle(self.samples)
            request_id_gen = [0]
            stage_end = time.time() + duration
            semaphore = asyncio.Semaphore(concurrency)
            
            async def worker():
                while time.time() < stage_end:
                    async with semaphore:
                        sample = next(samples_cycle)
                        rid = request_id_gen[0]
                        request_id_gen[0] += 1
                        start = time.time()
                        result = await client.send_ocr_request(
                            body_by_name[sample["name"]],
                            rid,
                        )
                        end = time.time()
                        m = RequestMetrics(
                            request_id=rid,
                            sample_name=sample["name"],
                            status=RequestStatus.SUCCESS if result.success else RequestStatus.ERROR,
                            start_time=start,
                            end_time=end,
                            latency_ms=result.latency_ms,
                            http_code=result.status_code,
                            error_msg=result.error_msg,
                            error_category=result.error_category,
                            char_count=result.char_count,
                            page_count=result.page_count,
                            text=result.text,
                            run_index=0,
                        )
                        await collector.add_request(m)
            
            workers = [asyncio.create_task(worker()) for _ in range(concurrency)]
            await asyncio.gather(*workers)
            print(f"\n  Completed: {len(collector.requests)} requests in {duration}s")
        
        if self.config.monitor.enable_system_monitor:
            await monitor.stop()
        
        collector.finalize()
        return collector, monitor


class CapacityTestExecutor(TestExecutor):
    """容量规划测试执行器：多档并发各跑一轮，记录每档 QPS/P99/成功率，便于选最优并发"""
    
    async def run(self) -> Tuple[MetricsCollector, ResourceMonitor]:
        """运行容量规划测试"""
        step = max(1, self.config.scenario.concurrency_step)
        
        print("\n" + "=" * 70)
        print("CAPACITY PLANNING TEST")
        print("=" * 70)
        print(f"Samples: {len(self.samples)}")
        print(f"Concurrency: {self.config.scenario.concurrency} -> {self.config.scenario.max_concurrency} (step {step})")
        print("Per-stage: one full pass (runs_per_sample × samples)")
        print("=" * 70 + "\n")
        
        collector = MetricsCollector(test_name=self.config.scenario.name)
        collector.capacity_stage_results = []
        monitor = ResourceMonitor(interval=self.config.monitor.monitor_interval)
        
        if self.config.monitor.enable_system_monitor:
            await monitor.start()
        
        async with AsyncHTTPClient(
            base_url=self.config.server.url,
            token=self.config.server.token,
            timeout=self.config.server.timeout,
            max_connections=self.config.scenario.max_concurrency * 2,
            verify_ssl=self.config.server.verify_ssl
        ) as client:
            await client.health_check()
            if self.config.scenario.warmup_requests > 0 and self.samples:
                await client.warmup(
                    self.samples[0]["base64"],
                    self._ocr_params_for_sample(self.samples[0]),
                    self.config.scenario.warmup_requests,
                    concurrency=1,
                )
            
            body_by_name = self._build_body_by_name()
            request_id_base = [0]
            
            for concurrency in range(
                self.config.scenario.concurrency,
                self.config.scenario.max_concurrency + 1,
                step
            ):
                stage_start = time.time()
                semaphore = asyncio.Semaphore(concurrency)
                tasks = []
                for _ in range(self.config.scenario.runs_per_sample):
                    for s in self.samples:
                        tasks.append((s, request_id_base[0]))
                        request_id_base[0] += 1
                
                for sample, rid in tasks:
                    async with semaphore:
                        start = time.time()
                        result = await client.send_ocr_request(
                            body_by_name[sample["name"]],
                            rid,
                        )
                        end = time.time()
                        m = RequestMetrics(
                            request_id=rid,
                            sample_name=sample["name"],
                            status=RequestStatus.SUCCESS if result.success else RequestStatus.ERROR,
                            start_time=start,
                            end_time=end,
                            latency_ms=result.latency_ms,
                            http_code=result.status_code,
                            error_msg=result.error_msg,
                            error_category=result.error_category,
                            char_count=result.char_count,
                            page_count=result.page_count,
                            text=result.text,
                            run_index=0,
                            metadata={"capacity_stage": concurrency},
                        )
                        await collector.add_request(m)
                
                stage_end_time = time.time()
                stage_requests = [r for r in collector.requests if r.metadata.get("capacity_stage") == concurrency]
                stage_stats = _compute_stage_stats(stage_requests, stage_start, stage_end_time)
                stage_stats["concurrency"] = concurrency
                collector.capacity_stage_results.append(stage_stats)
                print(f"  Concurrency {concurrency:3d}: QPS={stage_stats['qps']:.2f}, P99={stage_stats['p99_ms']:.2f}ms, Success={stage_stats['success_rate']:.2f}%")
        
        if self.config.monitor.enable_system_monitor:
            await monitor.stop()
        
        collector.finalize()
        return collector, monitor


def create_executor(config: BenchmarkConfig) -> TestExecutor:
    """创建测试执行器"""
    mode = config.scenario.mode
    
    if mode == TestMode.LATENCY:
        return LatencyTestExecutor(config)
    elif mode == TestMode.THROUGHPUT:
        return ThroughputTestExecutor(config)
    elif mode == TestMode.STRESS:
        return StressTestExecutor(config)
    elif mode == TestMode.STABILITY:
        return StabilityTestExecutor(config)
    elif mode == TestMode.CAPACITY:
        return CapacityTestExecutor(config)
    else:
        raise ValueError(f"Unknown test mode: {mode}")

