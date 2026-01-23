# DeepX OCR Server

<p align="center">
  <img src="https://img.shields.io/badge/Framework-Crow-blue.svg" alt="Crow">
  <img src="https://img.shields.io/badge/PDF-Poppler-orange.svg" alt="Poppler">
  <img src="https://img.shields.io/badge/WebUI-Gradio-green.svg" alt="Gradio">
</p>

基于 **Crow** 框架的高性能 OCR HTTP 服务，支持并发请求处理，支持图像和 PDF 文件输入。

---

## 📖 目录

- [快速开始](#-快速开始)
- [命令行参数](#-命令行参数)
- [API 接口](#-api-接口)
- [Web UI](#-web-ui)
- [基准测试](#-基准测试)
- [单元测试](#-单元测试)
- [目录结构](#-目录结构)

---

## ⚡ 快速开始

### 1. 编译项目

```bash
bash build.sh
```

### 2. 启动服务

```bash
cd server

# 使用默认配置启动（端口 8080，Server 模型）
./run_server.sh
```

### 3. 验证服务

```bash
curl http://localhost:8080/health
# 响应: {"status": "healthy", "service": "DeepX OCR Server", "version": "1.0.0"}
```

---

## 🛠️ 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-p, --port` | 服务端口 | 8080 |
| `-t, --threads` | HTTP 线程数 | 4 |
| `-v, --vis-dir` | 可视化输出目录 | output/vis |
| `-m, --model` | 模型类型：`server` 或 `mobile` | server |
| `-h, --help` | 显示帮助 | - |

**示例**:

```bash
# 使用 mobile 模型，端口 9090
./run_server.sh -p 9090 -m mobile

# 使用 8 个 HTTP 线程
./run_server.sh -t 8
```

---

## 📡 API 接口

### POST /ocr

OCR 识别接口，支持 Base64 编码图像/PDF 和 URL 两种输入方式。

**请求头**

```
Content-Type: application/json
Authorization: token <任意字符串>
```

**请求参数**

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| file | string | ✅ | - | Base64 编码的图像/PDF 或文件 URL |
| fileType | int | | 1 | 文件类型：1=图像，0=PDF |
| useDocOrientationClassify | bool | | false | 启用文档方向分类 |
| useDocUnwarping | bool | | false | 启用文档扭曲矫正 |
| useTextlineOrientation | bool | | false | 启用文本行方向矫正 |
| textDetThresh | float | | 0.3 | 检测像素阈值 [0.0-1.0] |
| textDetBoxThresh | float | | 0.6 | 检测框阈值 [0.0-1.0] |
| textDetUnclipRatio | float | | 1.5 | 检测框扩张系数 [1.0-3.0] |
| textRecScoreThresh | float | | 0.0 | 识别置信度阈值 [0.0-1.0] |
| visualize | bool | | false | 生成可视化结果图像 |
| pdfDpi | int | | 150 | PDF 渲染 DPI（仅 fileType=0，范围 72-300） |
| pdfMaxPages | int | | 10 | PDF 最大处理页数（仅 fileType=0，范围 1-100） |

<details>
<summary><b>📋 响应示例</b></summary>

**图像 OCR 响应 (fileType=1)**

```json
{
    "logId": "uuid-string",
    "errorCode": 0,
    "errorMsg": "Success",
    "result": {
        "ocrResults": [
            {
                "prunedResult": "识别的文字",
                "score": 0.98,
                "points": [
                    {"x": 100, "y": 50},
                    {"x": 300, "y": 50},
                    {"x": 300, "y": 80},
                    {"x": 100, "y": 80}
                ]
            }
        ],
        "ocrImage": "/static/vis/ocr_vis_xxx.jpg"
    }
}
```

**PDF OCR 响应 (fileType=0)**

```json
{
    "logId": "uuid-string",
    "errorCode": 0,
    "errorMsg": "Success",
    "result": {
        "totalPages": 4,
        "renderedPages": 2,
        "warning": "Only first 2 of 4 pages were processed due to page limit",
        "pages": [
            {
                "pageIndex": 0,
                "ocrResults": [
                    {
                        "prunedResult": "第一页的文字",
                        "score": 0.95,
                        "points": [...]
                    }
                ]
            },
            {
                "pageIndex": 1,
                "ocrResults": [
                    {
                        "prunedResult": "第二页的文字",
                        "score": 0.92,
                        "points": [...]
                    }
                ]
            }
        ]
    }
}
```

</details>

<details>
<summary><b>⚠️ 错误码</b></summary>

| errorCode | HTTP 状态码 | 说明 |
|-----------|-------------|------|
| 0 | 200 | 成功 |
| 1001 | 400 | 参数错误 |
| 1002 | 400 | PDF 文件无法打开 |
| 1003 | 400 | PDF 格式无效或文件损坏 |
| 1004 | 401 | PDF 需要密码 |
| 1005 | 403 | PDF 安全策略不支持 |
| 1006 | 400 | PDF 页面不存在 |
| 1007 | 400 | PDF 页面尺寸异常 |
| 1008 | 400 | PDF 页数超出限制 |
| 1009 | 400 | PDF DPI 超出限制 |
| 2001 | 500 | 服务内部错误 |
| 2002 | 503 | 内存分配失败 |
| 2003 | 504 | PDF 渲染超时 |
| 3001 | 401 | 认证失败 |

</details>

### PDF 处理说明

- **内存控制**：PDF 渲染会消耗较多内存，建议使用默认参数（DPI=150，最大 10 页）
- **内存估算**：A4 页面 @ 150 DPI 约 8.7MB/页，10 页约 87MB
- **并行处理**：多页 PDF 采用并行渲染和并行 OCR 处理
- **页数限制**：超出 `pdfMaxPages` 的页面不会被处理，响应中会包含 `warning` 字段

---

## 🌐 Web UI

基于 **Gradio** 的可视化 Web 界面，支持图像和 PDF 的 OCR 在线演示。

### ✨ 功能特性

- 🖼️ **多格式支持**: JPG, PNG, PDF
- 🔄 **图像处理**: 方向矫正、扭曲矫正、文本行方向矫正
- ⚙️ **参数调整**: OCR 检测/识别阈值实时调整
- 📄 **PDF 处理**: 可配置 DPI (72-300) 和最大页数 (1-100)
- 📊 **结果展示**: 可视化图像 + JSON 数据 + ZIP 下载
- 📱 **响应式 UI**: 侧边栏折叠、移动端适配

### 🚀 快速启动

```bash
cd server/webui

# 创建虚拟环境（首次使用）
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

# 启动 Web UI（确保 OCR Server 已运行）
python app.py
```

### 📍 访问地址

```
http://localhost:7860
```


---

## 🧪 基准测试

使用 `benchmark/run.sh` 统一入口进行性能测试。

### 📊 测试模式

| 模式 | 说明 | 命令 |
|------|------|------|
| `image` | Image OCR 测试 | `./run.sh --mode image` |
| `pdf` | PDF OCR 测试 | `./run.sh --mode pdf` |

### 🛠️ 通用参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-p, --port` | 服务器端口 | 8080 |
| `-m, --model` | 模型类型: `server` / `mobile` | server |
| `-r, --runs` | 每个测试项运行次数 | 1 |
| `-c, --concurrency` | 并发数 | 1 |
| `-s, --skip-server` | 跳过启动服务器（使用已运行的服务） | - |
| `-k, --keep-server` | 测试完成后保持服务器运行 | - |
| `-i, --images` | 测试图片目录 | `../../images` |
| `--pdfs` | 测试 PDF 目录 | `../pdf_file` |
| `--dpi` | PDF 渲染 DPI | 150 |
| `--max-pages` | PDF 最大处理页数 | 100 |
| `-h, --help` | 显示帮助 | - |

### 📝 使用示例

```bash
cd server/benchmark

# Image OCR 测试（默认模式）
./run.sh

# Image OCR 测试，4 并发
./run.sh --mode image -c 4

# PDF OCR 测试，指定 DPI
./run.sh --mode pdf --dpi 200 --max-pages 50
```

### 🔀 并发模式说明

| 模式 | 参数 | 说明 |
|------|------|------|
| 串行模式 | `-c 1` | 逐个请求，测量单请求延迟 (Latency) |
| 异步模式 | `-c N` (N>1) | 先发后收，测量系统吞吐量 (QPS) |

> **💡 提示**: 异步模式使用 `aiohttp` 实现先发后收，充分利用服务器 Pipeline 并行处理能力。

### 📄 测试结果输出

```
benchmark/results/
├── API_benchmark_report.md          # Image OCR 报告
├── api_benchmark_results.json       # Image OCR 结果
├── PDF_benchmark_report.md          # PDF OCR 报告
└── pdf_benchmark_results.json       # PDF OCR 结果
```

<details>
<summary><b>🔄 单独运行 Python 脚本</b></summary>

如果需要更精细的控制，可以直接运行 Python 脚本：

```bash
cd server/benchmark

# Image OCR 测试（4 并发，每张图片运行 3 次）
python3 run_api_benchmark.py -i "../../images" -r 3 -c 4

# PDF OCR 测试（DPI 150，最多处理 10 页）
python3 run_pdf_benchmark.py -p "../pdf_file" --dpi 150 --max-pages 10
```

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-i, --images` | 测试图片目录 | ../../images |
| `-p, --pdfs` | 测试 PDF 目录 | ../pdf_file |
| `-r, --runs` | 每项运行次数 | 1 |
| `-c, --concurrency` | 并发数量 | 10 (image) / 1 (pdf) |
| `--dpi` | PDF 渲染 DPI | 150 |
| `--max-pages` | PDF 最大处理页数 | 100 |

</details>

---

## ✅ 单元测试

### 运行所有测试

```bash
bash build.sh test
```

### PDF OCR 功能测试

```bash
cd server/tests

# 确保服务器已运行，然后执行
./run_pdf_ocr_test.sh
```

---

## 📁 目录结构

```
server/
├── 📜 server_main.cpp        # 服务入口
├── 📜 ocr_handler.cpp/h      # OCR 请求处理器
├── 📜 pdf_handler.cpp/h      # PDF 渲染处理器（基于 Poppler）
├── 📜 file_handler.cpp/h     # 文件处理（Base64/URL）
├── 📜 json_response.cpp/h    # JSON 响应构建器
├── 📂 webui/                 # Gradio Web UI
│   ├── 📜 app.py             # 主应用
│   ├── 📜 requirements.txt   # Python 依赖
│   ├── 📂 examples/          # 图片示例 (8 个)
│   ├── 📂 examples_pdf/      # PDF 示例 (10 个)
│   └── 📂 res/               # 资源文件 (Banner 等)
├── 📂 benchmark/             # 基准测试工具
│   ├── 📜 run.sh             # 统一测试入口
│   ├── 📜 run_api_benchmark.py   # Image API 测试
│   ├── 📜 run_pdf_benchmark.py   # PDF API 测试
│   └── 📂 results/           # 测试结果输出
├── 📂 pdf_file/              # 测试 PDF 文件
└── 📂 tests/                 # 单元测试
    ├── 📜 run_pdf_ocr_test.sh    # PDF 测试启动脚本
    ├── 📜 test_pdf_ocr.py        # PDF OCR 测试
    ├── 📜 test_*.cpp             # C++ 单元测试
    └── 📂 results/               # 测试结果
```

