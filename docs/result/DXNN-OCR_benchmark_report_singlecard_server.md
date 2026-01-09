# DXNN-OCR Benchmark Report

**Test Configuration**:
- Model: PP-OCR v5 (DEEPX NPU acceleration)
- Total Images Tested: 20
- Success Rate: 100.0%

**Test Results**:
| Filename | Inference Time (ms) | FPS | CPS (chars/s) | Accuracy (%) |
|---|---|---|---|---|
| `image_10.jpg` | 135.06 | 7.40 | **510.87** | **100.00** |
| `image_11.jpg` | 135.06 | 7.40 | **59.23** | **100.00** |
| `image_12.jpg` | 135.06 | 7.40 | **125.87** | **100.00** |
| `image_13.jpg` | 135.06 | 7.40 | **266.54** | **100.00** |
| `image_14.jpg` | 135.06 | 7.40 | **155.48** | **100.00** |
| `image_15.jpg` | 135.06 | 7.40 | **74.04** | **100.00** |
| `image_16.jpg` | 135.06 | 7.40 | **177.69** | **100.00** |
| `image_17.jpg` | 135.06 | 7.40 | **88.85** | **100.00** |
| `image_18.jpg` | 135.06 | 7.40 | **362.79** | **100.00** |
| `image_19.jpg` | 135.06 | 7.40 | **59.23** | **100.00** |
| `image_1.jpg` | 135.06 | 7.40 | **44.42** | **100.00** |
| `image_20.jpg` | 135.06 | 7.40 | **251.73** | **100.00** |
| `image_2.jpg` | 135.06 | 7.40 | **362.79** | **100.00** |
| `image_3.jpg` | 135.06 | 7.40 | **385.00** | **100.00** |
| `image_4.jpg` | 135.06 | 7.40 | **533.08** | **73.91** |
| `image_5.jpg` | 135.06 | 7.40 | **533.08** | **72.46** |
| `image_6.jpg` | 135.06 | 7.40 | **392.41** | **97.87** |
| `image_7.jpg` | 135.06 | 7.40 | **81.44** | **100.00** |
| `image_8.jpg` | 135.06 | 7.40 | **266.54** | **100.00** |
| `image_9.jpg` | 135.06 | 7.40 | **133.27** | **94.44** |
| **Average** | **135.06** | **7.40** | **243.22** | **96.93** |

**Performance Summary**:
- Average Inference Time: **135.06 ms**
- Average FPS: **7.40**
- Average CPS: **243.22 chars/s**
- Total Characters Detected: **657**
- Total Processing Time: **2701.28 ms**
- Average Character Accuracy: **96.93%**
- Success Rate: **100.0%** (20/20 images)
