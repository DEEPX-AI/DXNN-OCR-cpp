# DeepX OCR Server Web UI

基于 Gradio 框架的 OCR 在线演示界面，用于 DeepX OCR Server。

## 📋 目录

- [功能特性](#-功能特性)
- [前置条件](#-前置条件)
- [快速启动](#-快速启动)
- [目录结构](#-目录结构)
- [使用说明](#-使用说明)

## ✨ 功能特性

- **支持多种文件格式**: JPG, PNG, JPEG, PDF
- **图像处理选项** (Module Selection): 
  - 图像方向矫正 (Image Orientation Correction)
  - 图像扭曲矫正 (Image Distortion Correction)
  - 文本行方向矫正 (Text Line Orientation Correction)
- **OCR 参数调整** (OCR Settings): 
  - 文本检测像素阈值 (Text Detection Pixel Threshold): 0~1
  - 文本检测框阈值 (Text Detection Box Threshold): 0~1
  - 扩张系数 (Expansion Coefficient): 1.0~3.0
  - 文本识别置信度阈值 (Text Recognition Score Threshold): 0~1
- **PDF 处理** (PDF Settings): 
  - 可调节渲染 DPI (72-300)，默认 150
  - 可设置最大处理页数 (1-100)，默认 10
- **结果展示**: 
  - 可视化 OCR 结果图像 (OCR Tab)
  - JSON 格式数据 (JSON Tab)
  - 完整结果 ZIP 下载 (包含 OCR 图像、原始图像、JSON 数据)
- **响应式 UI**: 
  - 侧边栏折叠功能 (HIDE/SHOW LEFT MENU)
  - 自定义 PaddleOCR 风格主题

## 🔧 前置条件

### 1. 运行 OCR Server

此 Web UI 需要与后端 OCR 服务器通信，请先启动 OCR 服务器：

```bash
cd server

# 使用默认配置启动（端口 8080，Server 模型）
./run_server.sh
```

### 2. 系统要求

- **Python**: 3.10 或更高版本
- **内存**: 最少 2GB RAM
- **磁盘空间**: 约 500MB

## 🚀 快速启动

```bash
# 进入 WebUI 目录
cd server/webui

# 创建 Python 虚拟环境
python3 -m venv venv

# 激活虚拟环境
source venv/bin/activate

# 安装依赖
pip install --upgrade pip
pip install -r requirements.txt

# 启动 WebUI（默认连接 localhost:8080 的 OCR Server）
python app.py
```

**访问 WebUI**: 在浏览器中打开 **http://localhost:7860**

## 📁 目录结构

```
webui/
├── app.py              # 主应用 (Gradio UI)
├── requirements.txt    # Python 依赖
├── README.md           # 本文档
├── examples/           # 图片示例文件 (8 个)
│   ├── ancient_demo.png
│   ├── handwrite_ch_demo.png
│   ├── handwrite_en_demo.png
│   ├── japan_demo.png
│   ├── magazine.png
│   ├── pinyin_demo.png
│   ├── research.png
│   └── tech.png
├── examples_pdf/       # PDF 示例文件 (10 个)
│   ├── 1251647.pdf
│   ├── 3M-7770.pdf
│   ├── 438417-cap-prr-receipt.pdf
│   ├── 6275314-011414-Board-Meeting-Minutes-Approved.pdf
│   ├── BVRC_Meeting_Minutes_2024-04.pdf
│   ├── jresv101n1p69_A1b.pdf
│   ├── meeting_minutes_september_30_2020.pdf
│   ├── MiscMssLempereur_27.pdf
│   ├── physics0409110.pdf
│   └── Yinglish_Mikado Song Text comparison...pdf
└── res/                # 资源文件
    └── img/            # Banner 图片资源
        ├── deepx-baidu-pp-banner.png
        └── DEEPX-Banner-CES-2026-01.png
```

## 🎯 使用说明

### 1. 上传文件
- **拖拽上传**: 将文件拖拽到 "📁 Input File" 上传区域
- **点击上传**: 点击上传区域选择文件
- **示例选择**: 
  - 点击 "📷 Image Examples" 下方的示例图片
  - 点击 "📄 PDF Examples" 下方的示例 PDF

### 2. 调整参数 (⚙️ Settings)
- **Module Selection (模块选择)**:
  - Image Orientation Correction: 图像方向矫正
  - Image Distortion Correction: 图像扭曲矫正
  - Text Line Orientation Correction: 文本行方向矫正
- **OCR Settings (OCR 参数)**:
  - Text Detection Pixel Threshold (0.30): 文本检测像素阈值
  - Text Detection Box Threshold (0.60): 文本检测框阈值
  - Expansion Coefficient (1.5): 扩张系数
  - Text Recognition Score Threshold (0.00): 文本识别置信度阈值
- **PDF Settings (PDF 设置)**:
  - PDF Render DPI (150): 渲染分辨率
  - PDF Max Pages (10): 最大处理页数

### 3. 解析文档
- 点击 "🚀 Parse Document" 按钮开始 OCR 处理
- 处理过程中会显示加载动画

### 4. 查看结果 (📋 Results)
- **OCR Tab**: 带检测框的可视化图像，多页时左侧显示缩略图
- **JSON Tab**: 结构化的识别结果数据
- **下载**: 点击 "📦 Download Full Results (ZIP)" 打包下载所有结果

### 5. 展开结果视图
- 点击左侧边缘的 "HIDE LEFT MENU" 按钮可隐藏左侧菜单，全屏查看结果
- 再次点击 "SHOW LEFT MENU" 可恢复左侧菜单