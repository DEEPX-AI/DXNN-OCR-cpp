# DeepX OCR - 高性能 C++ OCR 推理引擎

<p align="center">
  <a href="README.md">English</a> •
  <img src="https://img.shields.io/badge/C++-17-blue.svg" alt="C++">
  <img src="https://img.shields.io/badge/Platform-Linux-green.svg" alt="Platform">
  <img src="https://img.shields.io/badge/Build-Passing-brightgreen.svg" alt="Build Status">
</p>

**DeepX OCR** 是一个基于 **PP-OCRv5** 的高性能、多线程异步 OCR 推理引擎，专为 **DeepX NPU** 加速优化。

---

## 📖 文档

- **[系统架构文档](docs/architecture.md)** - 详细的架构图、数据流和模型配置。

---

## ✨ 特性

- **🚀 高性能**：针对 DeepX NPU 优化的异步流水线。
- **🔄 多线程**：高效的线程池管理，支持并发处理。
- **🛠️ 模块化设计**：解耦的检测、分类和识别模块。
- **🌍 多语言支持**：内置 `freetype` 支持，完美渲染多语言文本。
- **📊 全面的基准测试**：集成了性能分析工具。

---

## ⚡ 快速开始

### 1. 克隆与初始化
```bash
# 克隆项目并初始化子模块
git clone --recursive https://github.com/Chris-godz/ocr_demo.git
git checkout cppinfer
cd ocr_demo
```

### 2. 安装依赖
```bash
# 安装 freetype 依赖（用于多语言文字渲染）
sudo apt-get install libfreetype6-dev libharfbuzz-dev
```

### 3. 编译与设置
```bash
# 编译项目
./build.sh

# 下载/设置模型
./setup.sh

# 设置 DXRT 环境变量（示例）
source ./set_env.sh 1 2 1 3 2 4
```

### 4. 运行测试
```bash
# 运行交互式测试菜单
./run.sh
```

---

## 🛠️ 构建配置

本项目使用 **Git Submodules** 管理依赖（`nlohmann/json`, `Clipper2`, `spdlog`, `OpenCV`, `opencv_contrib`）。

### 选项 1：从源码编译 OpenCV（推荐）
*包含 `opencv_contrib` 以获得更好的文本渲染支持。*

```bash
# 更新子模块
git submodule update --init 3rd-party/opencv
git submodule update --init 3rd-party/opencv_contrib

# 编译
./build.sh
```

### 选项 2：使用系统 OpenCV
*如果已安装 OpenCV，构建速度更快。*

```bash
# 设置环境变量
export BUILD_OPENCV_FROM_SOURCE=OFF

# 编译
./build.sh
```

---

## 📁 项目结构

```
OCR/
├── 📂 src/                    # 源代码
│   ├── 📂 common/             # 公共工具 (geometry, visualizer, logger)
│   ├── 📂 preprocessing/      # 预处理 (uvdoc, image_ops)
│   ├── 📂 detection/          # 文本检测模块
│   ├── 📂 classification/     # 方向分类模块
│   ├── 📂 recognition/        # 文本识别模块
│   └── 📂 pipeline/           # OCR 主流水线
├── 📂 3rd-party/              # 依赖库 (Git Submodules)
│   ├── 📦 json                # nlohmann/json
│   ├── 📦 clipper2            # 多边形裁剪
│   ├── 📦 spdlog              # 日志库
│   ├── 📦 opencv              # 计算机视觉
│   └── 📦 opencv_contrib      # 扩展模块 (freetype)
├── 📂 engine/model_files/     # 模型权重
│   ├── 📂 server/             # 高精度模型
│   └── 📂 mobile/             # 轻量级模型
├── 📂 benchmark/              # 性能基准测试
├── 📂 test/                   # 单元与集成测试
├── 📂 docs/                   # 文档
├── 📜 build.sh                # 编译脚本
├── 📜 run.sh                  # 交互式运行脚本
└── 📜 setup.sh                # 模型设置脚本
```

---

## 🧪 测试与基准测试

### 交互模式
```bash
./run.sh
```

### 手动执行
```bash
# Pipeline 测试
./build_Release/bin/test_pipeline_async

# 模块测试
./build_Release/test_detector                 # 检测
./build_Release/test_recognizer               # 识别 (Server)
./build_Release/test_recognizer_mobile        # 识别 (Mobile)
```

### 基准测试
```bash
# 运行 Python 基准测试包装器
python3 benchmark/run_benchmark.py --model server
python3 benchmark/run_benchmark.py --model mobile
```

