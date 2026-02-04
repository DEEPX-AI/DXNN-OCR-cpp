# OCR API Benchmark Report: default

**Generated at:** 2026-02-03 15:55:52

## 📋 Test Configuration

| Parameter | Value |
|-----------|-------|
| Test Name | default |
| Test Mode | throughput |
| Concurrency | 10 |
| Runs per Sample | 20 |
| Server URL | http://localhost:8080/ocr |
| Timeout | 60s |

## 🚀 Performance Summary

| Metric | Value |
|--------|-------|
| **Total Requests** | 400 |
| **Success Rate** | 100.00% |
| **QPS** | **3.71** |
| **Success QPS** | 3.71 |
| **Total Duration (ms)** | 107676.26 |

## ⏱️ Latency Statistics

| Percentile | Latency (ms) |
|------------|--------------|
| Min | 334.69 |
| P50 (Median) | 2422.49 |
| P90 | 4499.47 |
| P95 | 5512.62 |
| P99 | 7143.42 |
| P99.9 | 9418.67 |
| Max | 10343.26 |
| Mean | 2660.18 |
| Std Dev | 1405.84 |

## 📝 OCR Statistics

| Metric | Value |
|--------|-------|
| Total Characters | 195800 |
| Total Pages (pdf)| 0 |
| CPS (Chars/sec) | 1764.40 |
| Accuracy | 86.06% |

## ⚠️ Error Statistics

| Error Type | Count |
|------------|-------|

**Error Rate:** 0.00%
**Timeout Rate:** 0.00%

## 💻 Resource Monitoring

### CPU
| Metric | Value |
|--------|-------|
| Average | 53.28% |
| Max | 100.00% |
| Min | 0.00% |

### Memory
| Metric | Value |
|--------|-------|
| Average | 11.74% |
| Max | 12.00% |
| Min | 7.60% |

## 📊 Per-Sample Results

| Sample Name | Runs | Success | Avg Latency (ms) | Chars | Pages | Accuracy (%) |
|-------------|------|---------|------------------|-------|-------|--------------|
| `image_1.png` | 20 | 20 | 4095.89 | 8 | 0 | 57.14 |
| `image_10.png` | 20 | 20 | 2002.68 | 422 | 0 | 99.75 |
| `image_11.png` | 20 | 20 | 2401.81 | 946 | 0 | 99.56 |
| `image_12.png` | 20 | 20 | 2998.11 | 850 | 0 | 71.80 |
| `image_13.png` | 20 | 20 | 2114.34 | 66 | 0 | 100.00 |
| `image_14.png` | 20 | 20 | 2263.28 | 714 | 0 | 98.99 |
| `image_15.png` | 20 | 20 | 3122.00 | 1533 | 0 | 99.59 |
| `image_16.png` | 20 | 20 | 2739.28 | 56 | 0 | 97.92 |
| `image_17.png` | 20 | 20 | 2432.90 | 87 | 0 | 100.00 |
| `image_18.png` | 20 | 20 | 2000.32 | 629 | 0 | 99.66 |
| `image_19.png` | 20 | 20 | 3039.10 | 714 | 0 | 97.77 |
| `image_2.png` | 20 | 20 | 4876.05 | 53 | 0 | 62.00 |
| `image_20.png` | 20 | 20 | 3607.97 | 527 | 0 | 98.04 |
| `image_3.png` | 20 | 20 | 4696.36 | 25 | 0 | 21.43 |
| `image_4.png` | 20 | 20 | 1688.58 | 71 | 0 | 44.64 |
| `image_5.png` | 20 | 20 | 2270.29 | 20 | 0 | 95.24 |
| `image_6.png` | 20 | 20 | 2313.59 | 1335 | 0 | 97.67 |
| `image_7.png` | 20 | 20 | 1112.45 | 396 | 0 | 89.20 |
| `image_8.png` | 20 | 20 | 1542.41 | 463 | 0 | 94.15 |
| `image_9.png` | 20 | 20 | 1886.30 | 875 | 0 | 96.75 |

---