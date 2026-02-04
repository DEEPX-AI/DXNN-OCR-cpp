# DXNN-OCR Benchmark Report

**Test Configuration**:
- Model: PP-OCR v5 (DEEPX NPU acceleration)
- Total Images Tested: 20
- Success Rate: 100.0%

**Test Results**:
| Filename | Inference Time (ms) | FPS | CPS (chars/s) | Accuracy (%) |
|---|---|---|---|---|
| `image_10.jpg` | 60.00 | 16.67 | **1166.58** | **100.00** |
| `image_11.jpg` | 60.00 | 16.67 | **116.66** | **100.00** |
| `image_12.jpg` | 60.00 | 16.67 | **283.31** | **100.00** |
| `image_13.jpg` | 60.00 | 16.67 | **599.95** | **100.00** |
| `image_14.jpg` | 60.00 | 16.67 | **249.98** | **66.67** |
| `image_15.jpg` | 60.00 | 16.67 | **166.65** | **100.00** |
| `image_16.jpg` | 60.00 | 16.67 | **399.97** | **100.00** |
| `image_17.jpg` | 60.00 | 16.67 | **199.98** | **100.00** |
| `image_18.jpg` | 60.00 | 16.67 | **849.93** | **97.78** |
| `image_19.jpg` | 60.00 | 16.67 | **133.32** | **100.00** |
| `image_1.jpg` | 60.00 | 16.67 | **99.99** | **100.00** |
| `image_20.jpg` | 60.00 | 16.67 | **599.95** | **90.32** |
| `image_2.jpg` | 60.00 | 16.67 | **833.27** | **82.98** |
| `image_3.jpg` | 60.00 | 16.67 | **899.93** | **97.87** |
| `image_4.jpg` | 60.00 | 16.67 | **916.60** | **59.42** |
| `image_5.jpg` | 60.00 | 16.67 | **1199.91** | **71.01** |
| `image_6.jpg` | 60.00 | 16.67 | **866.60** | **93.62** |
| `image_7.jpg` | 60.00 | 16.67 | **149.99** | **60.00** |
| `image_8.jpg` | 60.00 | 16.67 | **449.97** | **78.79** |
| `image_9.jpg` | 60.00 | 16.67 | **316.64** | **88.89** |
| **Average** | **60.00** | **16.67** | **524.96** | **89.37** |

**Performance Summary**:
- Average Inference Time: **60.00 ms**
- Average FPS: **16.67**
- Average CPS: **524.96 chars/s**
- Total Characters Detected: **630**
- Total Processing Time: **1200.09 ms**
- Average Character Accuracy: **89.37%**
- Success Rate: **100.0%** (20/20 images)
