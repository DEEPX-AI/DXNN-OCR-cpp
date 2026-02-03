# DXNN-OCR Benchmark Report

**Test Configuration**:
- Model: PP-OCR v5 (DEEPX NPU acceleration)
- Total Images Tested: 20
- Success Rate: 100.0%

**Test Results**:
| Filename | Inference Time (ms) | FPS | CPS (chars/s) | Accuracy (%) |
|---|---|---|---|---|
| `image_10.jpg` | 44.24 | 22.61 | **1582.43** | **100.00** |
| `image_11.jpg` | 44.24 | 22.61 | **158.24** | **100.00** |
| `image_12.jpg` | 44.24 | 22.61 | **384.31** | **100.00** |
| `image_13.jpg` | 44.24 | 22.61 | **813.82** | **100.00** |
| `image_14.jpg` | 44.24 | 22.61 | **339.09** | **66.67** |
| `image_15.jpg` | 44.24 | 22.61 | **226.06** | **100.00** |
| `image_16.jpg` | 44.24 | 22.61 | **542.55** | **100.00** |
| `image_17.jpg` | 44.24 | 22.61 | **271.27** | **100.00** |
| `image_18.jpg` | 44.24 | 22.61 | **1152.92** | **97.78** |
| `image_19.jpg` | 44.24 | 22.61 | **180.85** | **100.00** |
| `image_1.jpg` | 44.24 | 22.61 | **135.64** | **100.00** |
| `image_20.jpg` | 44.24 | 22.61 | **836.43** | **93.55** |
| `image_2.jpg` | 44.24 | 22.61 | **1062.49** | **82.98** |
| `image_3.jpg` | 44.24 | 22.61 | **1220.73** | **97.87** |
| `image_4.jpg` | 44.24 | 22.61 | **1243.34** | **60.87** |
| `image_5.jpg` | 44.24 | 22.61 | **1627.65** | **71.01** |
| `image_6.jpg` | 44.24 | 22.61 | **1175.52** | **93.62** |
| `image_7.jpg` | 44.24 | 22.61 | **203.46** | **60.00** |
| `image_8.jpg` | 44.24 | 22.61 | **610.37** | **78.79** |
| `image_9.jpg` | 44.24 | 22.61 | **429.52** | **88.89** |
| **Average** | **44.24** | **22.61** | **709.83** | **89.60** |

**Performance Summary**:
- Average Inference Time: **44.24 ms**
- Average FPS: **22.61**
- Average CPS: **709.83 chars/s**
- Total Characters Detected: **628**
- Total Processing Time: **884.71 ms**
- Average Character Accuracy: **89.60%**
- Success Rate: **100.0%** (20/20 images)
