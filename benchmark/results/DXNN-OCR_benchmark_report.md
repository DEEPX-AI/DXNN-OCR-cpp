# DXNN-OCR Benchmark Report

**Test Configuration**:
- Model: PP-OCR v5 (DEEPX NPU acceleration)
- Total Images Tested: 20
- Success Rate: 100.0%

**Test Results**:
| Filename | Inference Time (ms) | FPS | CPS (chars/s) | Accuracy (%) |
|---|---|---|---|---|
| `image_10.png` | 1179.76 | 0.85 | **1070.56** | **100.00** |
| `image_11.png` | 2568.33 | 0.39 | **1083.19** | **99.56** |
| `image_12.png` | 1849.22 | 0.54 | **1153.46** | **87.90** |
| `image_13.png` | 1071.54 | 0.93 | **187.58** | **98.48** |
| `image_14.png` | 1858.49 | 0.54 | **1140.17** | **98.84** |
| `image_15.png` | 3222.46 | 0.31 | **1423.14** | **99.52** |
| `image_16.png` | 1111.12 | 0.90 | **151.20** | **95.83** |
| `image_17.png` | 592.07 | 1.69 | **385.09** | **86.75** |
| `image_18.png` | 1514.61 | 0.66 | **1204.27** | **99.33** |
| `image_19.png` | 1810.85 | 0.55 | **1156.91** | **95.68** |
| `image_1.png` | 1031.60 | 0.97 | **34.90** | **85.71** |
| `image_20.png` | 1702.30 | 0.59 | **917.58** | **96.67** |
| `image_2.png` | 1028.22 | 0.97 | **149.77** | **60.00** |
| `image_3.png` | 964.31 | 1.04 | **23.85** | **64.29** |
| `image_4.png` | 988.81 | 1.01 | **97.09** | **48.21** |
| `image_5.png` | 977.76 | 1.02 | **30.68** | **95.24** |
| `image_6.png` | 2410.28 | 0.41 | **1613.50** | **97.91** |
| `image_7.png` | 800.09 | 1.25 | **1279.86** | **95.06** |
| `image_8.png` | 1505.04 | 0.66 | **801.97** | **99.75** |
| `image_9.png` | 1933.72 | 0.52 | **1205.45** | **98.24** |
| **Average** | **1506.03** | **0.66** | **921.33** | **90.15** |

**Performance Summary**:
- Average Inference Time: **1506.03 ms**
- Average FPS: **0.66**
- Average CPS: **921.33 chars/s**
- Total Characters Detected: **27751**
- Total Processing Time: **30120.58 ms**
- Average Character Accuracy: **90.15%**
- Success Rate: **100.0%** (20/20 images)
