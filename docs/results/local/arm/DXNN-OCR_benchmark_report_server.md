# DXNN-OCR Benchmark Report

**Test Configuration**:
- Model: PP-OCR v5 (DEEPX NPU acceleration)
- Total Images Tested: 20
- Success Rate: 100.0%

**Test Results**:
| Filename | Inference Time (ms) | FPS | CPS (chars/s) | Accuracy (%) |
|---|---|---|---|---|
| `image_10.jpg` | 133.88 | 7.47 | **515.39** | **100.00** |
| `image_11.jpg` | 133.88 | 7.47 | **59.76** | **100.00** |
| `image_12.jpg` | 133.88 | 7.47 | **126.98** | **100.00** |
| `image_13.jpg` | 133.88 | 7.47 | **268.90** | **100.00** |
| `image_14.jpg` | 133.88 | 7.47 | **156.86** | **100.00** |
| `image_15.jpg` | 133.88 | 7.47 | **74.69** | **100.00** |
| `image_16.jpg` | 133.88 | 7.47 | **179.27** | **100.00** |
| `image_17.jpg` | 133.88 | 7.47 | **89.63** | **100.00** |
| `image_18.jpg` | 133.88 | 7.47 | **373.47** | **97.78** |
| `image_19.jpg` | 133.88 | 7.47 | **59.76** | **100.00** |
| `image_1.jpg` | 133.88 | 7.47 | **44.82** | **100.00** |
| `image_20.jpg` | 133.88 | 7.47 | **253.96** | **100.00** |
| `image_2.jpg` | 133.88 | 7.47 | **366.00** | **100.00** |
| `image_3.jpg` | 133.88 | 7.47 | **388.41** | **100.00** |
| `image_4.jpg` | 133.88 | 7.47 | **537.80** | **73.91** |
| `image_5.jpg` | 133.88 | 7.47 | **537.80** | **72.46** |
| `image_6.jpg` | 133.88 | 7.47 | **395.88** | **97.87** |
| `image_7.jpg` | 133.88 | 7.47 | **82.16** | **100.00** |
| `image_8.jpg` | 133.88 | 7.47 | **268.90** | **100.00** |
| `image_9.jpg` | 133.88 | 7.47 | **134.45** | **94.44** |
| **Average** | **133.88** | **7.47** | **245.74** | **96.82** |

**Performance Summary**:
- Average Inference Time: **133.88 ms**
- Average FPS: **7.47**
- Average CPS: **245.74 chars/s**
- Total Characters Detected: **658**
- Total Processing Time: **2677.59 ms**
- Average Character Accuracy: **96.82%**
- Success Rate: **100.0%** (20/20 images)
