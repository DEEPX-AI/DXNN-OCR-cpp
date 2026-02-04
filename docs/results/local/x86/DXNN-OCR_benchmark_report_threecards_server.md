# DXNN-OCR Benchmark Report

**Test Configuration**:
- Model: PP-OCR v5 (DEEPX NPU acceleration)
- Total Images Tested: 20
- Success Rate: 100.0%

**Test Results**:
| Filename | Inference Time (ms) | FPS | CPS (chars/s) | Accuracy (%) |
|---|---|---|---|---|
| `image_10.jpg` | 45.55 | 21.96 | **1514.90** | **100.00** |
| `image_11.jpg` | 45.55 | 21.96 | **175.64** | **100.00** |
| `image_12.jpg` | 45.55 | 21.96 | **373.24** | **100.00** |
| `image_13.jpg` | 45.55 | 21.96 | **790.38** | **100.00** |
| `image_14.jpg` | 45.55 | 21.96 | **461.06** | **100.00** |
| `image_15.jpg` | 45.55 | 21.96 | **219.55** | **100.00** |
| `image_16.jpg` | 45.55 | 21.96 | **526.92** | **100.00** |
| `image_17.jpg` | 45.55 | 21.96 | **263.46** | **100.00** |
| `image_18.jpg` | 45.55 | 21.96 | **1075.80** | **100.00** |
| `image_19.jpg` | 45.55 | 21.96 | **175.64** | **100.00** |
| `image_1.jpg` | 45.55 | 21.96 | **131.73** | **100.00** |
| `image_20.jpg` | 45.55 | 21.96 | **746.47** | **100.00** |
| `image_2.jpg` | 45.55 | 21.96 | **1075.80** | **100.00** |
| `image_3.jpg` | 45.55 | 21.96 | **1141.67** | **100.00** |
| `image_4.jpg` | 45.55 | 21.96 | **1580.77** | **73.91** |
| `image_5.jpg` | 45.55 | 21.96 | **1580.77** | **72.46** |
| `image_6.jpg` | 45.55 | 21.96 | **1163.62** | **97.87** |
| `image_7.jpg` | 45.55 | 21.96 | **241.51** | **100.00** |
| `image_8.jpg` | 45.55 | 21.96 | **790.38** | **100.00** |
| `image_9.jpg` | 45.55 | 21.96 | **395.19** | **94.44** |
| **Average** | **45.55** | **21.96** | **721.23** | **96.93** |

**Performance Summary**:
- Average Inference Time: **45.55 ms**
- Average FPS: **21.96**
- Average CPS: **721.23 chars/s**
- Total Characters Detected: **657**
- Total Processing Time: **910.95 ms**
- Average Character Accuracy: **96.93%**
- Success Rate: **100.0%** (20/20 images)
