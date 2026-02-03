# DXNN-OCR Benchmark Report

**Test Configuration**:
- Model: PP-OCR v5 (DEEPX NPU acceleration)
- Total Images Tested: 20
- Success Rate: 100.0%

**Test Results**:
| Filename | Inference Time (ms) | FPS | CPS (chars/s) | Accuracy (%) |
|---|---|---|---|---|
| `image_10.jpg` | 67.89 | 14.73 | **1016.36** | **100.00** |
| `image_11.jpg` | 67.89 | 14.73 | **117.84** | **100.00** |
| `image_12.jpg` | 67.89 | 14.73 | **250.41** | **100.00** |
| `image_13.jpg` | 67.89 | 14.73 | **530.27** | **100.00** |
| `image_14.jpg` | 67.89 | 14.73 | **309.33** | **100.00** |
| `image_15.jpg` | 67.89 | 14.73 | **147.30** | **100.00** |
| `image_16.jpg` | 67.89 | 14.73 | **353.52** | **100.00** |
| `image_17.jpg` | 67.89 | 14.73 | **176.76** | **100.00** |
| `image_18.jpg` | 67.89 | 14.73 | **721.76** | **100.00** |
| `image_19.jpg` | 67.89 | 14.73 | **117.84** | **100.00** |
| `image_1.jpg` | 67.89 | 14.73 | **88.38** | **100.00** |
| `image_20.jpg` | 67.89 | 14.73 | **500.81** | **100.00** |
| `image_2.jpg` | 67.89 | 14.73 | **721.76** | **100.00** |
| `image_3.jpg` | 67.89 | 14.73 | **765.95** | **100.00** |
| `image_4.jpg` | 67.89 | 14.73 | **1060.55** | **73.91** |
| `image_5.jpg` | 67.89 | 14.73 | **1060.55** | **72.46** |
| `image_6.jpg` | 67.89 | 14.73 | **780.68** | **97.87** |
| `image_7.jpg` | 67.89 | 14.73 | **162.03** | **100.00** |
| `image_8.jpg` | 67.89 | 14.73 | **530.27** | **100.00** |
| `image_9.jpg` | 67.89 | 14.73 | **265.14** | **94.44** |
| **Average** | **67.89** | **14.73** | **483.88** | **96.93** |

**Performance Summary**:
- Average Inference Time: **67.89 ms**
- Average FPS: **14.73**
- Average CPS: **483.88 chars/s**
- Total Characters Detected: **657**
- Total Processing Time: **1357.79 ms**
- Average Character Accuracy: **96.93%**
- Success Rate: **100.0%** (20/20 images)
