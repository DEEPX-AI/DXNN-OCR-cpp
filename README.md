# DeepX OCR - High-Performance C++ OCR Inference Engine

<p align="center">
  <a href="README_CN.md">中文</a> •
  <img src="https://img.shields.io/badge/C++-17-blue.svg" alt="C++">
  <img src="https://img.shields.io/badge/Platform-Linux-green.svg" alt="Platform">
  <img src="https://img.shields.io/badge/Build-Passing-brightgreen.svg" alt="Build Status">
</p>

**DeepX OCR** is a high-performance, multi-threaded asynchronous OCR inference engine based on **PP-OCRv5**, optimized for **DeepX NPU** acceleration.

---

## 📖 Documentation

- **[System Architecture](docs/architecture.md)** - Detailed architecture diagrams, data flow, and model configuration.

---

## ✨ Features

- **🚀 High Performance**: Asynchronous pipeline optimized for DeepX NPU.
- **🔄 Multi-threading**: Efficient thread pool management for concurrent processing.
- **🛠️ Modular Design**: Decoupled Detection, Classification, and Recognition modules.
- **🌍 Multi-language Support**: Built-in `freetype` support for rendering multi-language text.
- **📊 Comprehensive Benchmarking**: Integrated tools for performance analysis.

---

## ⚡ Quick Start

### 1. Clone & Initialize
```bash
# Clone the repository and initialize submodules
git clone --recursive https://github.com/Chris-godz/ocr_demo.git
git checkout cppinfer
cd ocr_demo
```

### 2. Install Dependencies
```bash
# Install freetype dependencies (for multi-language text rendering)
sudo apt-get install libfreetype6-dev libharfbuzz-dev libfmt-dev
```

### 3. Build & Setup
```bash
# Build the project
./build.sh

# Download/Setup models
./setup.sh

# Set DXRT environment variables (Example)
source ./set_env.sh 1 2 1 3 2 4
```

### 4. Run Tests
```bash
# Run the interactive test menu
./run.sh
```

---

## 🛠️ Build Configuration

This project uses **Git Submodules** to manage dependencies (`nlohmann/json`, `Clipper2`, `spdlog`, `OpenCV`, `opencv_contrib`).

### Option 1: Build OpenCV from Source (Recommended)
*Includes `opencv_contrib` for better text rendering support.*

```bash
# Update submodules
git submodule update --init 3rd-party/opencv
git submodule update --init 3rd-party/opencv_contrib

# Build
./build.sh
```

### Option 2: Use System OpenCV
*Faster build if you already have OpenCV installed.*

```bash
# Set environment variable
export BUILD_OPENCV_FROM_SOURCE=OFF

# Build
./build.sh
```

---

## 📁 Project Structure

```
OCR/
├── 📂 src/                    # Source Code
│   ├── 📂 common/             # Common Utilities (geometry, visualizer, logger)
│   ├── 📂 preprocessing/      # Preprocessing (uvdoc, image_ops)
│   ├── 📂 detection/          # Text Detection Module
│   ├── 📂 classification/     # Orientation Classification
│   ├── 📂 recognition/        # Text Recognition Module
│   └── 📂 pipeline/           # Main OCR Pipeline
├── 📂 3rd-party/              # Dependencies (Git Submodules)
│   ├── 📦 json                # nlohmann/json
│   ├── 📦 clipper2            # Polygon Clipping
│   ├── 📦 spdlog              # Logging
│   ├── 📦 opencv              # Computer Vision
│   └── 📦 opencv_contrib      # Extra Modules (freetype)
├── 📂 engine/model_files/     # Model Weights
│   ├── 📂 server/             # High-Accuracy Models
│   └── 📂 mobile/             # Lightweight Models
├── 📂 benchmark/              # Performance Benchmarking
├── 📂 test/                   # Unit & Integration Tests
├── 📂 docs/                   # Documentation
├── 📜 build.sh                # Build Script
├── 📜 run.sh                  # Interactive Runner
└── 📜 setup.sh                # Model Setup Script
```

---

## 🧪 Testing & Benchmarking

### Interactive Mode
```bash
./run.sh
```

### Manual Execution
```bash
# Pipeline Test
./build_Release/bin/test_pipeline_async

# Module Tests
./build_Release/test_detector                 # Detection
./build_Release/test_recognizer               # Recognition (Server)
./build_Release/test_recognizer_mobile        # Recognition (Mobile)
```

### Benchmarking
```bash
# Run Python benchmark wrapper
python3 benchmark/run_benchmark.py --model server
python3 benchmark/run_benchmark.py --model mobile
```

### 📊 Benchmark Reports (Summary)

**Test Configuration** (from `docs/result/` reports):
- Model: PP-OCR v5 (DEEPX NPU acceleration)
- Dataset Size: 20 images
- Success Rate: 100% (20/20)

**Performance Summary (Server)**:
| Setup | Avg Inference Time (ms) | Avg FPS | Avg CPS (chars/s) | Avg Character Accuracy |
|---|---:|---:|---:|---:|
| Single Card | 135.06 | 7.40 | 243.22 | 96.93% |
| Dual Cards | 67.89 | 14.73 | 483.88 | 96.93% |
| Three Cards | 45.55 | 21.96 | 721.23 | 96.93% |

**Performance Summary (Mobile)**:
| Setup | Avg Inference Time (ms) | Avg FPS | Avg CPS (chars/s) | Avg Character Accuracy |
|---|---:|---:|---:|---:|
| Single Card | 82.93 | 12.06 | 378.63 | 89.60% |
| Dual Cards | 44.24 | 22.61 | 709.83 | 89.60% |
| Three Cards | 33.00 | 30.30 | 951.57 | 89.60% |

**Detailed Reports**:
| Setup | Server | Mobile |
|---|---|---|
| Single Card | [Report](docs/result/DXNN-OCR_benchmark_report_singlecard_server.md) | [Report](docs/result/DXNN-OCR_benchmark_report_singlecard_mobile.md) |
| Dual Cards | [Report](docs/result/DXNN-OCR_benchmark_report_dualcards_server.md) | [Report](docs/result/DXNN-OCR_benchmark_report_dualcards_mobile.md) |
| Three Cards | [Report](docs/result/DXNN-OCR_benchmark_report_threecards_server.md) | [Report](docs/result/DXNN-OCR_benchmark_report_threecards_mobile.md) |

<details>
<summary><b>🔄 Reproduce Benchmark Results</b></summary>

To reproduce the benchmark results above, run the following commands:

```bash
# 1. Build the project
./build.sh

# 2. Download/Setup models
./setup.sh

# 3. Set DXRT environment variables (Example)
source ./set_env.sh 3 2 1 3 2 4

# 4. Run benchmark (Server model, 60 runs per image)
python3 benchmark/run_benchmark.py --model server --runs 60 \
    --images_dir test/twocode_images

# 5. Run benchmark (Mobile model, 60 runs per image)
python3 benchmark/run_benchmark.py --model mobile --runs 60 \
    --images_dir test/twocode_images
```

**Parameters**:
| Parameter | Description | Default |
|---|---|---|
| `--model` | Model type (`server` / `mobile`) | `server` |
| `--runs` | Number of runs per image | `3` |
| `--images_dir` | Test images directory | `images` |
| `--no-acc` | Skip accuracy calculation | - |
| `--no-cpp` | Skip C++ benchmark (use existing results) | - |

</details>
