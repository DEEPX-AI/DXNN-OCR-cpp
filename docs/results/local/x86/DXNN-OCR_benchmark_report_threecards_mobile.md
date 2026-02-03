# DXNN-OCR Benchmark Report

**Test Configuration**:
- Model: PP-OCR v5 (DEEPX NPU acceleration)
- Total Images Tested: 20
- Success Rate: 100.0%

**Test Results**:
| Filename | Inference Time (ms) | FPS | CPS (chars/s) | Accuracy (%) |
|---|---|---|---|---|
| `image_10.jpg` | 33.00 | 30.30 | **2121.33** | **100.00** |
| `image_11.jpg` | 33.00 | 30.30 | **212.13** | **100.00** |
| `image_12.jpg` | 33.00 | 30.30 | **515.18** | **100.00** |
| `image_13.jpg` | 33.00 | 30.30 | **1090.97** | **100.00** |
| `image_14.jpg` | 33.00 | 30.30 | **454.57** | **66.67** |
| `image_15.jpg` | 33.00 | 30.30 | **303.05** | **100.00** |
| `image_16.jpg` | 33.00 | 30.30 | **727.31** | **100.00** |
| `image_17.jpg` | 33.00 | 30.30 | **363.66** | **100.00** |
| `image_18.jpg` | 33.00 | 30.30 | **1545.54** | **97.78** |
| `image_19.jpg` | 33.00 | 30.30 | **242.44** | **100.00** |
| `image_1.jpg` | 33.00 | 30.30 | **181.83** | **100.00** |
| `image_20.jpg` | 33.00 | 30.30 | **1121.28** | **93.55** |
| `image_2.jpg` | 33.00 | 30.30 | **1424.32** | **82.98** |
| `image_3.jpg` | 33.00 | 30.30 | **1636.46** | **97.87** |
| `image_4.jpg` | 33.00 | 30.30 | **1666.76** | **60.87** |
| `image_5.jpg` | 33.00 | 30.30 | **2181.94** | **71.01** |
| `image_6.jpg` | 33.00 | 30.30 | **1575.85** | **93.62** |
| `image_7.jpg` | 33.00 | 30.30 | **272.74** | **60.00** |
| `image_8.jpg` | 33.00 | 30.30 | **818.23** | **78.79** |
| `image_9.jpg` | 33.00 | 30.30 | **575.79** | **88.89** |
| **Average** | **33.00** | **30.30** | **951.57** | **89.60** |

**Performance Summary**:
- Average Inference Time: **33.00 ms**
- Average FPS: **30.30**
- Average CPS: **951.57 chars/s**
- Total Characters Detected: **628**
- Total Processing Time: **659.96 ms**
- Average Character Accuracy: **89.60%**
- Success Rate: **100.0%** (20/20 images)
