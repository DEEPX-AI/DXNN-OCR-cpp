# OCR API Benchmark Report: default

**Generated at:** 2026-02-03 16:14:18

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
| **QPS** | **2.04** |
| **Success QPS** | 2.04 |
| **Total Duration (ms)** | 195703.64 |

## ⏱️ Latency Statistics

| Percentile | Latency (ms) |
|------------|--------------|
| Min | 516.54 |
| P50 (Median) | 4567.16 |
| P90 | 8122.77 |
| P95 | 9784.33 |
| P99 | 13053.77 |
| P99.9 | 14155.49 |
| Max | 14441.90 |
| Mean | 4846.49 |
| Std Dev | 2495.14 |

## 📝 OCR Statistics

| Metric | Value |
|--------|-------|
| Total Characters | 195800 |
| Total Pages (pdf)| 0 |
| CPS (Chars/sec) | 984.43 |
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
| Average | 33.42% |
| Max | 97.50% |
| Min | 0.00% |

### Memory
| Metric | Value |
|--------|-------|
| Average | 11.00% |
| Max | 11.50% |
| Min | 8.00% |

## 📊 Per-Sample Results

| Sample Name | Runs | Success | Avg Latency (ms) | Chars | Pages | Accuracy (%) |
|-------------|------|---------|------------------|-------|-------|--------------|
| `image_1.png` | 20 | 20 | 7516.73 | 8 | 0 | 57.14 |
| `image_10.png` | 20 | 20 | 3759.62 | 422 | 0 | 99.75 |
| `image_11.png` | 20 | 20 | 4364.86 | 946 | 0 | 99.56 |
| `image_12.png` | 20 | 20 | 6538.68 | 850 | 0 | 71.80 |
| `image_13.png` | 20 | 20 | 4202.83 | 66 | 0 | 100.00 |
| `image_14.png` | 20 | 20 | 3923.38 | 714 | 0 | 98.99 |
| `image_15.png` | 20 | 20 | 5318.24 | 1533 | 0 | 99.59 |
| `image_16.png` | 20 | 20 | 5607.07 | 56 | 0 | 97.92 |
| `image_17.png` | 20 | 20 | 4519.50 | 87 | 0 | 100.00 |
| `image_18.png` | 20 | 20 | 3982.00 | 629 | 0 | 99.66 |
| `image_19.png` | 20 | 20 | 4307.62 | 714 | 0 | 97.77 |
| `image_2.png` | 20 | 20 | 8529.63 | 53 | 0 | 62.00 |
| `image_20.png` | 20 | 20 | 6958.16 | 527 | 0 | 98.04 |
| `image_3.png` | 20 | 20 | 8878.18 | 25 | 0 | 21.43 |
| `image_4.png` | 20 | 20 | 3028.36 | 71 | 0 | 44.64 |
| `image_5.png` | 20 | 20 | 2983.49 | 20 | 0 | 95.24 |
| `image_6.png` | 20 | 20 | 4049.98 | 1335 | 0 | 97.67 |
| `image_7.png` | 20 | 20 | 2242.70 | 396 | 0 | 89.20 |
| `image_8.png` | 20 | 20 | 2853.16 | 463 | 0 | 94.15 |
| `image_9.png` | 20 | 20 | 3365.60 | 875 | 0 | 96.75 |

---