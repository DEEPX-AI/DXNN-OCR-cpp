"""
异步 HTTP 客户端模块
提供高性能的异步请求能力
"""

import json
import time
import asyncio
import aiohttp
from typing import Optional, Dict, Any
from dataclasses import dataclass
from .metrics import RequestStatus, ErrorCategory, RequestMetrics


@dataclass
class RequestResult:
    """请求结果"""
    success: bool
    status_code: int
    latency_ms: float
    error_msg: str = ""
    error_category: ErrorCategory = ErrorCategory.NONE
    
    # OCR 结果
    text: str = ""
    char_count: int = 0
    page_count: int = 0
    ocr_results: list = None
    
    # 原始响应
    response_json: Optional[Dict] = None


class AsyncHTTPClient:
    """异步 HTTP 客户端"""
    
    def __init__(self, base_url: str, token: str, timeout: int = 60,
                 max_connections: int = 100, verify_ssl: bool = False):
        """
        初始化客户端
        
        Args:
            base_url: 服务器 URL
            token: 认证 token
            timeout: 请求超时时间（秒）
            max_connections: 最大连接数
            verify_ssl: 是否验证 SSL 证书
        """
        self.base_url = base_url
        self.token = token
        self.timeout = timeout
        self.verify_ssl = verify_ssl
        
        # 连接器配置
        self.connector = aiohttp.TCPConnector(
            limit=max_connections,
            limit_per_host=max_connections,
            keepalive_timeout=30,
            force_close=False,
            ssl=verify_ssl
        )
        
        self.session: Optional[aiohttp.ClientSession] = None
    
    async def __aenter__(self):
        """异步上下文管理器入口"""
        self.session = aiohttp.ClientSession(
            connector=self.connector,
            timeout=aiohttp.ClientTimeout(total=self.timeout)
        )
        return self
    
    async def __aexit__(self, exc_type, exc_val, exc_tb):
        """异步上下文管理器退出"""
        if self.session:
            await self.session.close()
    
    async def _parse_ocr_response(self, response: aiohttp.ClientResponse,
                                   latency_ms: float) -> RequestResult:
        """解析 OCR 响应体并组装 RequestResult"""
        try:
            response_json = await response.json()
        except Exception as e:
            return RequestResult(
                success=False,
                status_code=response.status,
                latency_ms=latency_ms,
                error_msg=f"JSON decode error: {e}",
                error_category=ErrorCategory.DECODE
            )

        if response.status != 200:
            error_cat = ErrorCategory.HTTP_4XX if 400 <= response.status < 500 else ErrorCategory.HTTP_5XX
            return RequestResult(
                success=False,
                status_code=response.status,
                latency_ms=latency_ms,
                error_msg=f"HTTP {response.status}",
                error_category=error_cat,
                response_json=response_json
            )
        error_code = response_json.get("errorCode", -1)

        if error_code != 0:
            return RequestResult(
                success=False,
                status_code=response.status,
                latency_ms=latency_ms,
                error_msg=response_json.get("errorMsg", "Unknown error"),
                error_category=ErrorCategory.VALIDATION,
                response_json=response_json
            )
        result = response_json.get("result", {})
        
        if "pages" in result:
            pages = result.get("pages", [])
            total_chars = 0
            all_texts = []
            for page in pages:
                ocr_results = page.get("ocrResults", [])
                for ocr in ocr_results:
                    text = ocr.get("prunedResult", "")
                    all_texts.append(text)
                    total_chars += len(text)
            return RequestResult(
                success=True,
                status_code=response.status,
                latency_ms=latency_ms,
                text="".join(all_texts),
                char_count=total_chars,
                page_count=result.get("renderedPages", 0),
                ocr_results=pages,
                response_json=response_json
            )
        ocr_results = result.get("ocrResults", [])
        texts = [r.get("prunedResult", "") for r in ocr_results]
        text = "".join(texts)
        return RequestResult(
            success=True,
            status_code=response.status,
            latency_ms=latency_ms,
            text=text,
            char_count=len(text),
            page_count=0,
            ocr_results=ocr_results,
            response_json=response_json
        )
    
    async def send_ocr_request(self, body: bytes, request_id: int = 0) -> RequestResult:
        """
        发送 OCR 请求
        
        Args:
            body: UTF-8 编码的 JSON 字符串（已序列化的 payload）
            request_id: 请求 ID（用于日志）
        
        Returns:
            RequestResult 对象
        """
        headers = {
            "Content-Type": "application/json",
            "Authorization": f"token {self.token}"
        }
        start_time = time.time()
        try:
            async with self.session.post(
                self.base_url,
                headers=headers,
                data=body
            ) as response:
                latency_ms = (time.time() - start_time) * 1000
                return await self._parse_ocr_response(response, latency_ms)

        except asyncio.TimeoutError:
            return RequestResult(
                success=False,
                status_code=0,
                latency_ms=(time.time() - start_time) * 1000,
                error_msg="Request timeout",
                error_category=ErrorCategory.TIMEOUT
            )

        except asyncio.CancelledError:
            return RequestResult(
                success=False,
                status_code=0,
                latency_ms=(time.time() - start_time) * 1000,
                error_msg="Request cancelled",
                error_category=ErrorCategory.TIMEOUT
            )

        except aiohttp.ClientError as e:
            return RequestResult(
                success=False,
                status_code=0,
                latency_ms=(time.time() - start_time) * 1000,
                error_msg=f"Connection error: {e}",
                error_category=ErrorCategory.CONNECTION
            )

        except Exception as e:
            return RequestResult(
                success=False,
                status_code=0,
                latency_ms=(time.time() - start_time) * 1000,
                error_msg=f"Unexpected error: {e}",
                error_category=ErrorCategory.UNKNOWN
            )
    
    async def warmup(self, file_base64: str, ocr_params: Dict[str, Any],
                     warmup_count: int = 5, concurrency: int = 1) -> tuple[int, int]:
        """
        预热阶段
        
        Args:
            file_base64: 预热用的文件
            ocr_params: OCR 参数
            warmup_count: 预热请求数量
            concurrency: 并发数
        
        Returns:
            (成功数, 总数)
        """
        body = json.dumps({"file": file_base64, **ocr_params}, ensure_ascii=False).encode("utf-8")
        if concurrency > 1:
            print(f"\n[Warmup] Concurrent warmup ({concurrency}) with {warmup_count} requests...")
        else:
            print(f"\n[Warmup] Starting warmup with {warmup_count} requests...")
        
        if concurrency <= 1:
            success_count = 0
            for i in range(warmup_count):
                result = await self.send_ocr_request(body, request_id=-i-1)
                if result.success:
                    success_count += 1
                    print(f"  Warmup {i+1}/{warmup_count}: {result.latency_ms:.2f}ms ✓")
                else:
                    print(f"  Warmup {i+1}/{warmup_count}: {result.error_msg} ✗")
        else:
            semaphore = asyncio.Semaphore(concurrency)
            async def _one_warmup(i: int):
                async with semaphore:
                    return await self.send_ocr_request(body, request_id=-i-1)
            results = await asyncio.gather(*[_one_warmup(i) for i in range(warmup_count)])
            success_count = sum(1 for r in results if r.success)
            for i, r in enumerate(results):
                status = "✓" if r.success else "✗"
                msg = f"{r.latency_ms:.2f}ms" if r.success else r.error_msg
                print(f"  Warmup {i+1}/{warmup_count}: {msg} {status}")
        
        success_rate = success_count / warmup_count * 100
        print(f"[Warmup] Completed: {success_count}/{warmup_count} ({success_rate:.0f}% success)\n")

        return success_count, warmup_count
    
    async def health_check(self, health_url: Optional[str] = None) -> bool:
        """
        健康检查
        
        Args:
            health_url: 健康检查 URL（默认使用 base_url 的 /health）
        
        Returns:
            是否健康
        """
        if health_url is None:
            # 从 base_url 推导 health_url
            from urllib.parse import urlparse, urlunparse
            parsed = urlparse(self.base_url)
            health_url = urlunparse((parsed.scheme, parsed.netloc, '/health', '', '', ''))
        
        try:
            async with self.session.get(health_url, timeout=aiohttp.ClientTimeout(total=5)) as response:
                if response.status == 200:
                    data = await response.json()
                    print(f"[Health Check] Server is healthy: {data}")
                    return True
                else:
                    print(f"[Health Check] Server returned {response.status}")
                    return False
        except Exception as e:
            print(f"[Health Check] Failed: {e}")
            return False

