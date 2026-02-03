# DXNN-OCR Benchmark Report

**Test Configuration**:
- Model: PP-OCR v5 (DEEPX NPU acceleration)
- Total Images Tested: 20
- Success Rate: 100.0%

**Test Results**:
| Filename | Inference Time (ms) | FPS | CPS (chars/s) | Accuracy (%) |
|---|---|---|---|---|
| `image_10.jpg` | 82.93 | 12.06 | **844.07** | **100.00** |
| `image_11.jpg` | 82.93 | 12.06 | **84.41** | **100.00** |
| `image_12.jpg` | 82.93 | 12.06 | **204.99** | **100.00** |
| `image_13.jpg` | 82.93 | 12.06 | **434.09** | **100.00** |
| `image_14.jpg` | 82.93 | 12.06 | **180.87** | **66.67** |
| `image_15.jpg` | 82.93 | 12.06 | **120.58** | **100.00** |
| `image_16.jpg` | 82.93 | 12.06 | **289.39** | **100.00** |
| `image_17.jpg` | 82.93 | 12.06 | **144.70** | **100.00** |
| `image_18.jpg` | 82.93 | 12.06 | **614.96** | **97.78** |
| `image_19.jpg` | 82.93 | 12.06 | **96.46** | **100.00** |
| `image_1.jpg` | 82.93 | 12.06 | **72.35** | **100.00** |
| `image_20.jpg` | 82.93 | 12.06 | **446.15** | **93.55** |
| `image_2.jpg` | 82.93 | 12.06 | **566.73** | **82.98** |
| `image_3.jpg` | 82.93 | 12.06 | **651.14** | **97.87** |
| `image_4.jpg` | 82.93 | 12.06 | **663.20** | **60.87** |
| `image_5.jpg` | 82.93 | 12.06 | **868.18** | **71.01** |
| `image_6.jpg` | 82.93 | 12.06 | **627.02** | **93.62** |
| `image_7.jpg` | 82.93 | 12.06 | **108.52** | **60.00** |
| `image_8.jpg` | 82.93 | 12.06 | **325.57** | **78.79** |
| `image_9.jpg` | 82.93 | 12.06 | **229.10** | **88.89** |
| **Average** | **82.93** | **12.06** | **378.63** | **89.60** |

**Performance Summary**:
- Average Inference Time: **82.93 ms**
- Average FPS: **12.06**
- Average CPS: **378.63 chars/s**
- Total Characters Detected: **628**
- Total Processing Time: **1658.63 ms**
- Average Character Accuracy: **89.60%**
- Success Rate: **100.0%** (20/20 images)
