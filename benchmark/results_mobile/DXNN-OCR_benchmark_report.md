# DXNN-OCR Benchmark Report

**Test Configuration**:
- Model: PP-OCR v5 (DEEPX NPU acceleration)
- Total Images Tested: 20
- Success Rate: 100.0%

**Test Results**:
| Filename | Inference Time (ms) | FPS | CPS (chars/s) | Accuracy (%) |
|---|---|---|---|---|
| `image_10.png` | 173.94 | 5.75 | **7215.27** | **99.50** |
| `image_11.png` | 173.94 | 5.75 | **15505.65** | **93.83** |
| `image_12.png` | 173.94 | 5.75 | **12740.27** | **71.53** |
| `image_13.png` | 173.94 | 5.75 | **1155.59** | **98.48** |
| `image_14.png` | 173.94 | 5.75 | **12130.85** | **86.85** |
| `image_15.png` | 173.94 | 5.75 | **23336.09** | **79.78** |
| `image_16.png` | 173.94 | 5.75 | **879.63** | **95.83** |
| `image_17.png` | 173.94 | 5.75 | **1408.56** | **95.18** |
| `image_18.png` | 173.94 | 5.75 | **10452.08** | **99.33** |
| `image_19.png` | 173.94 | 5.75 | **12119.36** | **95.68** |
| `image_1.png` | 173.94 | 5.75 | **120.73** | **42.86** |
| `image_20.png` | 173.94 | 5.75 | **9106.76** | **95.29** |
| `image_2.png` | 173.94 | 5.75 | **896.88** | **38.00** |
| `image_3.png` | 173.94 | 5.75 | **212.72** | **50.00** |
| `image_4.png` | 173.94 | 5.75 | **781.89** | **44.64** |
| `image_5.png` | 173.94 | 5.75 | **172.48** | **95.24** |
| `image_6.png` | 173.94 | 5.75 | **22272.48** | **96.30** |
| `image_7.png` | 173.94 | 5.75 | **6364.39** | **83.33** |
| `image_8.png` | 173.94 | 5.75 | **7019.80** | **93.13** |
| `image_9.png` | 173.94 | 5.75 | **13458.92** | **95.13** |
| **Average** | **173.94** | **5.75** | **7867.52** | **82.50** |

**Performance Summary**:
- Average Inference Time: **173.94 ms**
- Average FPS: **5.75**
- Average CPS: **7867.52 chars/s**
- Total Characters Detected: **27369**
- Total Processing Time: **3478.73 ms**
- Average Character Accuracy: **82.50%**
- Success Rate: **100.0%** (20/20 images)
