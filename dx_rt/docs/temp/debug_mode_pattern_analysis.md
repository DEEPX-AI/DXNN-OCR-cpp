# Debug Mode Pattern Analysis Guide

## 개요

Debug mode는 모델의 중간 출력(intermediate outputs)을 분석하여 GT(Ground Truth)와 RT(Runtime) 간의 차이를 정밀하게 진단합니다. Binary-first 접근 방식을 사용하여 먼저 바이트 레벨에서 완전 일치를 확인하고, 불일치 발생 시 패턴을 분석하여 원인을 분류합니다.

## 분석 프로세스

### Step 1: Binary-level Comparison (바이너리 비교)
- 모든 파일을 `np.uint8` (raw bytes)로 로드
- Byte-by-byte 비교 수행
- 100% 일치 시 → `IDENTICAL` (추가 분석 불필요)
- 불일치 시 → Step 2로 진행

### Step 2: Pattern Analysis (패턴 분석)
불일치가 발생한 경우, 데이터를 float32/int32 등으로 재해석하여 차이의 패턴을 분석합니다.

---

## Difference Types (차이 유형)

### 1. **IDENTICAL** - "Identical"
```
패턴 이름: N/A (완벽한 일치)
Match: 100.0%
출력 예시: ✅ task_name  100.00%  Identical
```

**의미:**
- GT와 RT가 바이너리 레벨에서 완전히 일치
- 모든 바이트가 동일함
- 이상적인 상태

**발생 조건:**
- `binary_match_percentage == 100.0`

---

### 2. **SHAPE_MISMATCH** - "Shape mismatch"
```
패턴 이름: N/A (형상 불일치)
Match: 0.0%
출력 예시: ❌ task_name    0.00%  Shape mismatch
```

**의미:**
- GT와 RT 파일의 크기(shape)가 다름
- 데이터 구조 자체가 불일치

**발생 조건:**
- `gt_data.shape != rt_data.shape`

**원인:**
- 모델 버전 불일치
- 컴파일러 설정 오류
- 잘못된 GT 데이터 사용

---

### 3. **SMALL_NUMERICAL_ERROR** - "Small numerical error"

이 타입은 여러 세부 패턴을 포함합니다:

#### 3-1. Pattern: `padding_area_difference`
```
Match: 98.0% ~ 99.9%
출력 예시: ❌ npu_0_output  98.46%  Small numerical error  [padding_area_difference]
```

**의미:**
- **주기적 패턴:** 차이가 일정한 간격을 두고 반복적으로 발생 (주기적 덩어리)
- **C-channel Padding:** NCHW, NHWC 등의 layout에서 C 차원을 memory word 단위로 padding할 때 발생
- **특수 케이스:** 주기가 1인 경우 (패턴이 한 번만 나타나 파일 마지막 10%만 다른 경우)
- Padding 영역의 값은 대부분 garbage 값(쓰레기값) 또는 0

**발생 조건 (2가지 방법):**

1. **주기적 패턴 감지 (Periodic Pattern Detection):**
```python
# Method 1: 가장 흔한 gap이 60% 이상 반복
most_common_gap_ratio > 0.6

# Method 2: Chunk 간격의 변동계수(CV) < 30%
chunk_spacing_CV < 0.3
```

2. **Legacy 방식 (주기=1 케이스):**
```python
# 모든 차이가 파일 끝 10% 영역에 있음
if diff_indices[0] >= int(file_size * 0.9):
    return "padding_area_difference"
```

**판단 기준:**
```python
# 1. 차이 위치 간의 간격(gap) 분석
gaps = np.diff(diff_indices)
unique_gaps, gap_counts = np.unique(gaps, return_counts=True)
most_common_count / len(gaps) > 0.6  # 주기적

# 2. Chunk 단위 분석 (gap > 100 bytes로 chunk 구분)
chunk_gaps = np.diff(chunk_starts)
(std(chunk_gaps) / mean(chunk_gaps)) < 0.3  # 일정한 간격

# 3. Padding 값 검증
zeros_or_small = count(byte_value < 10)
padding_ratio = zeros_or_small / total_diff_bytes
# 높은 padding_ratio -> 실제 padding
```

**실제 예시:**
```
파일 구조 (NCHW, C=3 channels, word padding to C=4):
[R1 G1 B1 P1] [R2 G2 B2 P2] [R3 G3 B3 P3] ...
              ↑            ↑            ↑
            padding      padding      padding
```
- P (padding) 위치에서 차이 발생
- 일정한 간격(4 words)으로 반복
- P 값은 0 또는 garbage

**원인:**
- **Layout 변환:** NCHW ↔ NHWC 변환 시 channel padding
- **Memory alignment:** Word boundary에 맞추기 위한 padding
- **Channel expansion:** 3-channel을 4-channel로 확장 (RGB → RGBA)
- 초기화되지 않은 padding 메모리 영역

**무시 가능 여부:**
- ✅ **무시 가능:** 실제 데이터(R, G, B)는 일치하고 padding 영역만 다름
- 조건: `padding_ratio > 0.7` (차이 중 70% 이상이 작은 값/0)

---

#### 3-2. Pattern: `small_float_error`
```
Match: 95.0% ~ 99.5%
Correlation: 0.0 ~ 0.99
출력 예시: ❌ task_name  97.23%  Small numerical error  [small_float_error]
```

**의미:**
- Float 값으로 해석 시 작은 수치 오차
- 평균 절대 오차가 매우 작음 (< 0.001)
- 데이터 패턴은 유지됨

**발생 조건:**
```python
mean_abs_diff < 1e-3  # 0.001
```

**판단 기준:**
- 바이너리 불일치하지만 float로 해석 시 오차 작음

**원인:**
- Floating-point 연산의 rounding 차이
- 하드웨어 구현 차이
- **대부분 허용 가능:** 수치적으로 거의 동일

---

#### 3-3. Pattern: `isolated_byte_differences`
```
Match: 95.0% ~ 99.9%
출력 예시: ❌ task_name  99.12%  Small numerical error  [isolated_byte_differences]
```

**의미:**
- 전체의 5% 미만만 다름
- 산발적으로 흩어진 바이트 차이
- Float 해석 실패 (파일 크기가 4의 배수가 아니거나 유효한 float 값 부족)

**발생 조건:**
```python
diff_percentage < 5.0  # 5% 미만
```

**원인:**
- 일부 바이트만 다른 경우
- Integer 데이터의 미세한 차이
- **조사 필요:** 원인에 따라 허용 여부 결정

---

### 4. **FLOAT_PRECISION_ISSUE** - "Float precision issue"
```
패턴 이름: float_precision_error
Match: 90.0% ~ 98.0%
Max Relative Error: < 1%
출력 예시: ❌ task_name  95.34%  Float precision issue  [float_precision_error]
```

**의미:**
- Float 값들의 상대 오차(relative error)가 매우 작음
- 데이터 패턴은 거의 동일하나 정밀도 차이 존재
- 반올림, 타입 변환 등의 차이

**발생 조건 (2가지 중 하나):**

1. **매우 작은 절대 오차:**
```python
max_abs_diff < 1e-4 (0.0001)
AND mean_abs_diff < 1e-5 (0.00001)
```

2. **작은 상대 오차 (더 직관적):**
```python
mean_relative_error < 0.01  # 평균 상대 오차 < 1%
```

**판단 기준:**
```python
# 상대 오차 계산
relative_error = |GT - RT| / |GT|

# 조건 1: 절대값 기준
if max_abs_diff < 1e-4 and mean_abs_diff < 1e-5:
    return "float_precision_error"

# 조건 2: 상대 오차 기준
if mean(relative_error) < 0.01:  # 1%
    return "float_precision_error"
```

**실제 예시:**
```
GT:  [1.0000, 2.5000, 3.7500]
RT:  [0.9999, 2.4998, 3.7502]
상대 오차: [0.01%, 0.008%, 0.005%]
평균: 0.0077% < 1% ✅
```

**원인:**
- **타입 변환:** float32 ↔ float16 변환 시 정밀도 손실
- **하드웨어 차이:** CPU vs NPU 연산 정밀도 (rounding 방식 차이)
- **누적 오차:** 여러 연산을 거치면서 발생하는 미세한 오차 누적

**무시 가능 여부:**
- ✅ **일반적으로 허용 가능:** 상대 오차 1% 미만은 실용적으로 동일
- 참고: Correlation은 참고용으로만 사용 (직관적이지 않음)

---

### 5. **SCALING_ISSUE** - "Scaling issue"
```
패턴 이름: scaling_or_offset
Match: 85.0% ~ 95.0%
Mean Relative Error: 1% ~ 10%
출력 예시: ❌ task_name  89.45%  Scaling issue  [scaling_or_offset]
```

**의미:**
- 상대 오차가 중간 정도 (1% ~ 10%)
- 오차 비율이 일정함 (consistent ratio)
- 값들이 선형적으로 스케일되거나 offset이 있음: y = ax + b

**발생 조건:**
```python
mean_relative_error < 0.1  # 평균 상대 오차 < 10%
AND
max_relative_error / mean_relative_error < 5  # 일관된 비율
```

**판단 기준:**
```python
# 상대 오차 계산
relative_errors = |GT - RT| / |GT|

# 조건
mean_rel_error = mean(relative_errors)
max_rel_error = max(relative_errors)

if mean_rel_error < 0.1 and (max_rel_error / mean_rel_error) < 5:
    return "scaling_or_offset"
```

**실제 예시:**
```
# Case 1: Scaling (a ≠ 1, b = 0)
GT:  [1.00, 2.00, 3.00, 4.00]
RT:  [1.05, 2.10, 3.15, 4.20]  # 1.05배 scale
상대 오차: [5%, 5%, 5%, 5%]
평균: 5%, Max/Mean = 1.0 ✅ 일관됨

# Case 2: Offset (a = 1, b ≠ 0)
GT:  [1.00, 2.00, 3.00, 4.00]
RT:  [1.10, 2.10, 3.10, 4.10]  # +0.1 offset
상대 오차: [10%, 5%, 3.3%, 2.5%]
평균: 5.2%, Max/Mean = 1.92 ✅ 일관됨

# Case 3: Both (a ≠ 1, b ≠ 0)
GT:  [1.00, 2.00, 3.00, 4.00]
RT:  [1.15, 2.25, 3.35, 4.45]  # 1.05배 + 0.1
```

**원인:**
- **Quantization scale 불일치:** INT8 양자화 시 scale factor 다름
- **Normalization 차이:** Batch norm, Layer norm의 parameter 불일치
- **Activation scaling:** PReLU, LeakyReLU의 alpha 값 차이
- **입력 전처리:** 입력 이미지의 정규화 scale/offset 차이

**조사 방법:**
```python
# Scale factor 추정
estimated_scale = mean(RT / GT)
estimated_offset = mean(RT - GT * estimated_scale)

# 예: scale=1.05, offset=0.1
# RT ≈ 1.05 * GT + 0.1
```

**무시 가능 여부:**
- ⚠️ **조사 필요:** Scale factor가 의도된 것인지 확인
- 허용 가능한 경우: 의도적인 scale 변경 (예: 모델 최적화)
- 문제가 되는 경우: 버그로 인한 예상치 못한 scaling

---

### 6. **PARTIAL_MATCH** - "Partial match"
```
패턴 이름: significant_numerical_diff
Match: 50.0% ~ 90.0%
Correlation: 0.0 ~ 0.95
출력 예시: ❌ task_name  73.21%  Partial match  [significant_numerical_diff]
```

**의미:**
- 일부는 일치하나 상당 부분 불일치
- 평균 절대 오차가 큼 (≥ 0.001)
- 상관관계 낮음

**발생 조건:**
```python
mean_abs_diff >= 1e-3 and correlation <= 0.95
```

**원인:**
- 중간 layer 출력 차이
- 연산 로직 불일치
- **문제 가능성 높음:** 정밀 분석 필요

---

### 7. **COMPLETELY_DIFFERENT** - "Completely different"
```
패턴 이름: major_binary_difference
Match: 0.0% ~ 50.0%
출력 예시: ❌ task_name  12.34%  Completely different  [major_binary_difference]
```

**의미:**
- 5% 이상의 바이트가 불일치
- 데이터 자체가 근본적으로 다름

**발생 조건:**
```python
diff_percentage >= 5.0  # 5% 이상
```

**원인:**
- 잘못된 모델 사용
- 심각한 연산 오류
- 데이터 corruption
- **심각한 문제:** 즉시 조사 필요

---

### 8. **TENSOR_ORDER_ISSUE** - "Tensor order issue"
```
현재 사용되지 않음 (향후 확장 가능)
```

**의미:**
- Tensor의 차원 순서가 다름 (예: NCHW vs NHWC)

---

### 9. **ENDIANNESS_ISSUE** - "Endianness issue"
```
현재 사용되지 않음 (향후 확장 가능)
```

**의미:**
- Byte order가 다름 (little-endian vs big-endian)

---

### 10. **PATTERN_SHIFT** - "Pattern shift"
```
현재 사용되지 않음 (향후 확장 가능)
```

**의미:**
- 데이터 패턴이 shift되어 있음 (offset 불일치)

---

## 패턴 판단 Flow Chart (개선된 버전)

```
시작
  ↓
Shape 일치? → No → SHAPE_MISMATCH
  ↓ Yes
Binary 100% 일치? → Yes → IDENTICAL
  ↓ No
주기적 패턴 감지? → Yes → Padding 값(0/작은값)? → Yes → SMALL_NUMERICAL_ERROR (padding_area_difference)
  ↓ No                                          ↓ No
차이 위치 ≥ 90%? → Yes ─────────────────────→ (계속 분석)
  ↓ No
Float32로 해석 가능? → No → 차이 < 5%? → Yes → SMALL_NUMERICAL_ERROR (isolated_byte_differences)
  ↓ Yes                              ↓ No
유효한 float 값? → No ─────────────→ COMPLETELY_DIFFERENT (major_binary_difference)
  ↓ Yes (10개 이상)
┌─────────────────────────────────────────┐
│ Float 값 분석 (상대 오차 기반)           │
└─────────────────────────────────────────┘
  ↓
Max abs diff < 1e-4 AND Mean < 1e-5? → Yes → FLOAT_PRECISION_ISSUE (float_precision_error)
  ↓ No
Mean relative error < 1%? → Yes → FLOAT_PRECISION_ISSUE (float_precision_error)
  ↓ No
Mean relative error < 10% AND 일관된 비율? → Yes → SCALING_ISSUE (scaling_or_offset)
  ↓ No
Mean abs diff < 1e-3? → Yes → SMALL_NUMERICAL_ERROR (small_float_error)
  ↓ No
PARTIAL_MATCH (significant_numerical_diff)
```

**주요 개선 사항:**
1. **Padding 패턴 우선 검사**: 주기적 패턴 감지 추가 (NCHW/NHWC C-channel padding)
2. **Correlation 제거**: 상대 오차(relative error) 기반으로 변경 (더 직관적)
3. **일관성 체크**: Scaling issue는 오차 비율의 일관성 확인
4. **직관적 기준**: 절대값과 상대값을 모두 고려

---

## 실제 사용 예시

### 예시 1: C-channel Padding 차이
```bash
❌ npu_0_output  98.46%  Small numerical error  [padding_area_difference]
```
**해석:**
- 98.46% 바이트 일치
- 주기적 패턴 감지: NCHW/NHWC layout의 C-dimension padding 차이
- 차이 위치: 일정한 간격으로 반복되는 덩어리 (예: 4 words마다)
- Padding 값: 대부분 0 또는 작은 garbage 값
- **조치:** ✅ 무시 가능, 실제 RGB 데이터는 정상, padding 영역만 다름

### 예시 2: Float 정밀도 차이
```bash
❌ npu_0_decoder  96.78%  Float precision issue  [float_precision_error]
```
**해석:**
- 96.78% 바이트 일치
- 평균 상대 오차 < 1% (매우 작음)
- 예: GT=1.0000, RT=0.9998 → 상대 오차 0.02%
- **조치:** ✅ float16/float32 변환 확인, 일반적으로 허용 가능

### 예시 3: 완벽한 일치
```bash
✅ npu_0_encoder_input  100.00%  Identical
```
**해석:**
- 모든 바이트 완전 일치
- **조치:** 없음, 정상

### 예시 4: 심각한 불일치
```bash
❌ cpu_task_output  23.45%  Completely different  [major_binary_difference]
```
**해석:**
- 76.55%의 바이트가 불일치
- **조치:** 즉시 조사 필요, 모델/데이터 검증

---

## 판단 기준 요약표

| Pattern Type | Match % | Relative Error | Mean Abs Diff | 심각도 | 허용 여부 |
|--------------|---------|----------------|---------------|--------|----------|
| IDENTICAL | 100% | 0% | 0 | 없음 | ✅ 완벽 |
| padding_area_difference | 98~99.9% | - | - | 낮음 | ✅ 대부분 OK (주기적 패턴) |
| float_precision_error | 95~98% | <1% | <0.0001 | 낮음 | ✅ 일반적으로 OK |
| small_float_error | 95~99% | - | <0.001 | 낮음 | ✅ 일반적으로 OK |
| isolated_byte_differences | 95~99.9% | - | - | 낮음 | ⚠️ 확인 필요 |
| scaling_or_offset | 85~95% | 1%~10% (일관됨) | - | 중간 | ⚠️ Scale factor 확인 |
| significant_numerical_diff | 50~90% | >10% | ≥0.001 | 높음 | ❌ 분석 필요 |
| major_binary_difference | 0~50% | - | - | 매우 높음 | ❌ 즉시 조사 |
| SHAPE_MISMATCH | 0% | - | - | 매우 높음 | ❌ 즉시 조사 |

---

## 디버깅 권장 사항

### 허용 가능한 패턴 (일반적으로 무시 가능)
1. `padding_area_difference` - Padding 영역 차이
2. `float_precision_error` - Float 정밀도 차이 (correlation > 0.99)
3. `small_float_error` - 작은 float 오차 (< 0.001)

### 확인 필요한 패턴
1. `isolated_byte_differences` - 원인 파악 후 판단
2. `scaling_or_offset` - Scale factor 확인

### 즉시 조사 필요한 패턴
1. `significant_numerical_diff` - 연산 로직 검증
2. `major_binary_difference` - 모델/데이터 검증
3. `SHAPE_MISMATCH` - 구조적 문제

---

## 참고 사항

- **Binary-first 철학:** 모든 분석은 바이너리 비교부터 시작
- **NaN 처리:** GT와 RT 모두 NaN인 경우 일치로 간주
- **Correlation:** Pearson correlation coefficient 사용
- **Match percentage:** 바이트 레벨 일치율 (binary match %)
