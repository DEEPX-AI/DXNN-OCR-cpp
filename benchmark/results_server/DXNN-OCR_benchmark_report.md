# DXNN-OCR Benchmark Report

**Test Configuration**:
- Model: PP-OCR v5 (DEEPX NPU acceleration)
- Total Images Tested: 20
- Success Rate: 100.0%

**Test Results**:
| Filename | Inference Time (ms) | FPS | CPS (chars/s) | Accuracy (%) |
|---|---|---|---|---|
| `image_10.png` | 434.69 | 2.30 | **2912.43** | **99.75** |
| `image_11.png` | 434.69 | 2.30 | **6445.99** | **99.56** |
| `image_12.png` | 434.69 | 2.30 | **5410.76** | **71.80** |
| `image_13.png` | 434.69 | 2.30 | **455.50** | **100.00** |
| `image_14.png` | 434.69 | 2.30 | **4890.85** | **98.99** |
| `image_15.png` | 434.69 | 2.30 | **10556.97** | **99.59** |
| `image_16.png` | 434.69 | 2.30 | **381.88** | **97.92** |
| `image_17.png` | 434.69 | 2.30 | **600.43** | **100.00** |
| `image_18.png` | 434.69 | 2.30 | **4203.00** | **99.66** |
| `image_19.png` | 434.69 | 2.30 | **4881.65** | **97.77** |
| `image_1.png` | 434.69 | 2.30 | **55.21** | **57.14** |
| `image_20.png` | 434.69 | 2.30 | **3637.08** | **98.04** |
| `image_2.png` | 434.69 | 2.30 | **356.58** | **62.00** |
| `image_3.png` | 434.69 | 2.30 | **121.93** | **21.43** |
| `image_4.png` | 434.69 | 2.30 | **315.17** | **44.64** |
| `image_5.png` | 434.69 | 2.30 | **69.01** | **95.24** |
| `image_6.png` | 434.69 | 2.30 | **8999.53** | **97.67** |
| `image_7.png` | 434.69 | 2.30 | **2576.55** | **89.20** |
| `image_8.png` | 434.69 | 2.30 | **2877.92** | **94.15** |
| `image_9.png` | 434.69 | 2.30 | **5465.97** | **96.75** |
| **Average** | **434.69** | **2.30** | **3260.72** | **86.06** |

**Performance Summary**:
- Average Inference Time: **434.69 ms**
- Average FPS: **2.30**
- Average CPS: **3260.72 chars/s**
- Total Characters Detected: **28348**
- Total Processing Time: **8693.78 ms**
- Average Character Accuracy: **86.06%**
- Success Rate: **100.0%** (20/20 images)
