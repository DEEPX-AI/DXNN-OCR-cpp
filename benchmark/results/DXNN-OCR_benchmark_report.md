# DXNN-OCR Benchmark Report

**Test Configuration**:
- Model: PP-OCR v5 (DEEPX NPU acceleration)
- Total Images Tested: 20
- Success Rate: 100.0%

**Test Results**:
| Filename | Inference Time (ms) | FPS | CPS (chars/s) | Accuracy (%) |
|---|---|---|---|---|
| `image_10.png` | 1158.16 | 0.86 | **1090.52** | **100.00** |
| `image_11.png` | 2537.14 | 0.39 | **1097.30** | **99.34** |
| `image_12.png` | 1877.75 | 0.53 | **1241.38** | **70.56** |
| `image_13.png` | 930.52 | 1.07 | **156.90** | **72.73** |
| `image_14.png` | 1685.39 | 0.59 | **1032.99** | **80.20** |
| `image_15.png` | 3102.11 | 0.32 | **1476.41** | **99.31** |
| `image_16.png` | 911.10 | 1.10 | **155.86** | **87.50** |
| `image_17.png` | 551.94 | 1.81 | **462.00** | **98.80** |
| `image_18.png` | 1393.48 | 0.72 | **1298.19** | **98.99** |
| `image_19.png` | 1527.10 | 0.65 | **1387.60** | **98.07** |
| `image_1.png` | 968.71 | 1.03 | **0.00** | **0.00** |
| `image_20.png` | 1452.66 | 0.69 | **1073.89** | **95.88** |
| `image_2.png` | 916.17 | 1.09 | **144.08** | **60.00** |
| `image_3.png` | 892.57 | 1.12 | **38.09** | **14.29** |
| `image_4.png` | 916.98 | 1.09 | **102.51** | **53.57** |
| `image_5.png` | 866.67 | 1.15 | **15.00** | **42.86** |
| `image_6.png` | 2380.36 | 0.42 | **1628.74** | **98.23** |
| `image_7.png` | 752.41 | 1.33 | **1447.35** | **89.81** |
| `image_8.png` | 1347.08 | 0.74 | **860.38** | **84.48** |
| `image_9.png` | 1866.92 | 0.54 | **1230.90** | **92.42** |
| **Average** | **1401.76** | **0.71** | **978.27** | **76.85** |

**Performance Summary**:
- Average Inference Time: **1401.76 ms**
- Average FPS: **0.71**
- Average CPS: **978.27 chars/s**
- Total Characters Detected: **27426**
- Total Processing Time: **28035.22 ms**
- Average Character Accuracy: **76.85%**
- Success Rate: **100.0%** (20/20 images)
