# OCR API Benchmark Report: default

**Generated at:** 2026-02-03 18:10:25

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
| **QPS** | **2.06** |
| **Success QPS** | 2.06 |
| **Total Duration (ms)** | 194192.87 |

## ⏱️ Latency Statistics

| Percentile | Latency (ms) |
|------------|--------------|
| Min | 625.96 |
| P50 (Median) | 4306.95 |
| P90 | 8342.72 |
| P95 | 9607.25 |
| P99 | 13366.27 |
| P99.9 | 16429.25 |
| Max | 17841.94 |
| Mean | 4809.76 |
| Std Dev | 2543.16 |

## 📝 OCR Statistics

| Metric | Value |
|--------|-------|
| Total Characters | 195720 |
| Total Pages (pdf)| 0 |
| CPS (Chars/sec) | 990.53 |
| Accuracy | 86.09% |

## ⚠️ Error Statistics

| Error Type | Count |
|------------|-------|

**Error Rate:** 0.00%
**Timeout Rate:** 0.00%

## 💻 Resource Monitoring

### CPU
| Metric | Value |
|--------|-------|
| Average | 32.29% |
| Max | 81.00% |
| Min | 0.00% |

### Memory
| Metric | Value |
|--------|-------|
| Average | 30.55% |
| Max | 31.20% |
| Min | 17.20% |

## 📊 Per-Sample Results

| Sample Name | Runs | Success | Avg Latency (ms) | Chars | Pages | Accuracy (%) |
|-------------|------|---------|------------------|-------|-------|--------------|
| `image_1.png` | 20 | 20 | 6808.84 | 8 | 0 | 57.14 |
| `image_10.png` | 20 | 20 | 3703.10 | 422 | 0 | 99.75 |
| `image_11.png` | 20 | 20 | 4640.18 | 946 | 0 | 99.56 |
| `image_12.png` | 20 | 20 | 6718.54 | 850 | 0 | 71.80 |
| `image_13.png` | 20 | 20 | 3754.72 | 66 | 0 | 100.00 |
| `image_14.png` | 20 | 20 | 4486.63 | 714 | 0 | 98.99 |
| `image_15.png` | 20 | 20 | 5040.94 | 1533 | 0 | 99.59 |
| `image_16.png` | 20 | 20 | 5730.09 | 56 | 0 | 97.92 |
| `image_17.png` | 20 | 20 | 4284.40 | 87 | 0 | 100.00 |
| `image_18.png` | 20 | 20 | 3720.35 | 629 | 0 | 99.66 |
| `image_19.png` | 20 | 20 | 5276.14 | 714 | 0 | 97.77 |
| `image_2.png` | 20 | 20 | 8482.67 | 52 | 0 | 62.00 |
| `image_20.png` | 20 | 20 | 7096.75 | 526 | 0 | 97.84 |
| `image_3.png` | 20 | 20 | 8405.39 | 25 | 0 | 21.43 |
| `image_4.png` | 20 | 20 | 2653.28 | 71 | 0 | 44.64 |
| `image_5.png` | 20 | 20 | 3297.11 | 20 | 0 | 95.24 |
| `image_6.png` | 20 | 20 | 4282.43 | 1335 | 0 | 97.67 |
| `image_7.png` | 20 | 20 | 2053.54 | 394 | 0 | 89.81 |
| `image_8.png` | 20 | 20 | 2643.16 | 463 | 0 | 94.15 |
| `image_9.png` | 20 | 20 | 3116.92 | 875 | 0 | 96.75 |

---