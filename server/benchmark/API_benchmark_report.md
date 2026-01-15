# DXNN-OCR API Server Benchmark Report

**Test Configuration**:
- Model: PP-OCR v5 (DEEPX NPU acceleration)
- Total Images Tested: 20
- Runs per Image: 20
- Success Rate: 100.0%

**Test Results**:
| Filename | Inference Time (ms) | FPS | CPS (chars/s) | Accuracy (%) |
|---|---|---|---|---|
| `image_1.png` | 408.34 | 2.45 | **19.59** | **72.73** |
| `image_10.png` | 425.48 | 2.35 | **991.83** | **99.88** |
| `image_11.png` | 835.77 | 1.20 | **1131.90** | **99.37** |
| `image_12.png` | 736.59 | 1.36 | **1153.97** | **83.40** |
| `image_13.png` | 311.17 | 3.21 | **212.10** | **100.00** |
| `image_14.png` | 683.41 | 1.46 | **1044.77** | **98.18** |
| `image_15.png` | 1114.30 | 0.90 | **1375.75** | **99.74** |
| `image_16.png` | 349.93 | 2.86 | **160.03** | **94.44** |
| `image_17.png` | 240.88 | 4.15 | **361.18** | **100.00** |
| `image_18.png` | 527.63 | 1.90 | **1192.13** | **99.84** |
| `image_19.png` | 582.72 | 1.72 | **1225.29** | **97.46** |
| `image_2.png` | 389.22 | 2.57 | **136.17** | **70.37** |
| `image_20.png` | 547.78 | 1.83 | **962.07** | **99.05** |
| `image_3.png` | 362.48 | 2.76 | **68.97** | **65.00** |
| `image_4.png` | 316.53 | 3.16 | **224.31** | **70.77** |
| `image_5.png` | 297.65 | 3.36 | **67.19** | **97.56** |
| `image_6.png` | 867.78 | 1.15 | **1538.41** | **97.75** |
| `image_7.png` | 316.77 | 3.16 | **1250.13** | **92.35** |
| `image_8.png` | 541.14 | 1.85 | **855.60** | **96.37** |
| `image_9.png` | 714.32 | 1.40 | **1224.94** | **96.76** |
| **Average** | **528.49** | **1.89** | **926.22** | **91.55** |

**Performance Summary**:
- Average Inference Time: **528.49 ms**
- Average FPS: **1.89**
- Average CPS: **926.22 chars/s**
- Total Characters Detected: **9790**
- Total Processing Time: **10569.88 ms**
- Average Character Accuracy: **91.55%**
- Success Rate: **100.0%** (20/20 images)