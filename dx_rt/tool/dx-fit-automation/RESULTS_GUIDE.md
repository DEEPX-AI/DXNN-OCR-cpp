# 📊 결과 분석 가이드

**Version**: 1.2.0 | **Date**: 2025-10-16

## 🗂️ 실험 디렉토리 구조

### 통합된 결과 관리 (v1.2.0 신규)

자동화 테스트 실행 시 **하나의 통합된 실험 디렉토리**에 모든 결과가 자동 정리됩니다:

```
results/
└── 20241016_143052/                    # 실험 타임스탬프 (YYYYMMDD_HHMMSS)
    ├── summary.csv                     # ✅ COMPLETE - 모든 데이터 포함
    ├── detailed.json                   # 상세 raw 데이터
    ├── models/                         # 모델별 dx-fit 결과
    │   ├── AlexNet_5-AlexNet-6/
    │   │   ├── best_config.json        # dx-fit 최적 configuration
    │   │   ├── results.csv             # 전체 테스트 결과
    │   │   └── tuning_report_*.txt     # 튜닝 리포트
    │   ├── ResNet50/
    │   │   ├── best_config.json
    │   │   └── results.csv
    │   └── MobileNet/
    │       └── ... (결과 파일들)
    └── logs/
        └── automation.log              # 전체 실행 로그
```

### 🎯 핵심 개선사항 (v1.2.0)

| 기능 | 이전 (v1.1) | 현재 (v1.2) |
|------|-------------|-------------|
| **결과 위치** | 여러 곳에 분산 | 단일 실험 디렉토리 |
| **dx-fit 결과** | `tool/dx-fit/` 산재 | `models/{name}/` 자동 정리 |
| **summary.csv** | 불완전 (recover 필요) | 완전한 데이터 (즉시 분석) |
| **빈 디렉토리** | 생성됨 (혼란) | 생성 안됨 (깔끔) |
| **파일명** | 타임스탬프 suffix | 간결 (summary.csv) |

---

## 📄 결과 파일 상세

### 1. `summary.csv` - **Excel 즉시 분석 가능**

**✅ 완전한 데이터**: 모든 정보가 처음부터 기입되어 있습니다. recover 단계 불필요!

#### 주요 컬럼

**성능 지표** (자동 기입, 소수점 3자리):
```
default_fps              # 초기 FPS (예: 245.300)
default_latency          # 초기 지연시간 ms (예: 4.080)
default_npu_time         # 초기 NPU 실행 시간 ms (예: 2.500)
best_fps                 # dx-fit 최적화 후 FPS (예: 352.700)
best_latency             # 최적화 후 지연시간 ms (예: 2.840)
best_npu_time            # 최적화 후 NPU 실행 시간 ms (예: 2.300)
fps_improvement          # 성능 향상 배수 (예: 1.35)
fps_improvement_percent  # 성능 향상 퍼센트 (예: +35.2)
```

**Best Configuration** (Excel 필터/정렬/피벗 가능):
```
CUSTOM_INTER_OP_THREADS_COUNT     # Inter-op 스레드 수
CUSTOM_INTRA_OP_THREADS_COUNT     # Intra-op 스레드 수
DXRT_DYNAMIC_CPU_THREAD    # Dynamic CPU 스레드
DXRT_TASK_MAX_LOAD               # Task 최대 로드 ⭐
NFH_INPUT_WORKER_THREADS          # Input worker 스레드
NFH_OUTPUT_WORKER_THREADS         # Output worker 스레드
```

**메타데이터**:
```
adjusted_loop_count         # Loop count (config 또는 dx-fit이 자동 선정)
dxfit_total_tests           # 총 테스트 수
dxfit_successful_tests      # 성공한 테스트 수
total_time_minutes          # 총 소요 시간 (분)
default_test_success        # 초기 테스트 성공 여부 (YES/NO)
dxfit_success               # dx-fit 성공 여부 (YES/NO)
timestamp                   # 테스트 시각
```

#### CSV 예시 (소수점 3자리)

```csv
model_name,default_fps,default_latency,default_npu_time,best_fps,best_latency,best_npu_time,fps_improvement,fps_improvement_percent,DXRT_TASK_MAX_LOAD,...
AlexNet_5,245.300,4.080,2.500,352.700,2.840,2.300,1.44,+43.7,9,...
ResNet50,85.200,11.740,7.200,128.600,7.780,6.900,1.51,+50.9,12,...
MobileNet,412.800,2.420,1.500,598.300,1.670,1.400,1.45,+44.9,9,...
```

### 2. `detailed.json` - 프로그래밍 용도

전체 `ModelTestResult` 객체의 JSON dump. 자동화 도구나 스크립트로 처리할 때 사용.

```json
[
  {
    "model_name": "AlexNet_5",
    "model_path": "/mnt/.../AlexNet_5.dxnn",
    "default_fps": 245.300,
    "default_latency": 4.080,
    "default_npu_time": 2.500,
    "dxfit_best_fps": 352.700,
    "dxfit_best_latency": 2.840,
    "dxfit_best_npu_time": 2.300,
    "best_DXRT_TASK_MAX_LOAD": 9,
    ...
  }
]
```

### 3. `models/{model_name}/` - 모델별 상세 결과

각 모델마다 dx-fit이 생성한 파일들이 자동으로 정리됩니다:

#### `best_config.json`
dx-fit이 찾은 최적 configuration:
```json
{
  "fps": 352.7,
  "latency": 2.84,
  "npu_time": 2.3,
  "parameters": {
    "DXRT_TASK_MAX_LOAD": 9,
    "CUSTOM_INTRA_OP_THREADS_COUNT": 3,
    "NFH_OUTPUT_WORKER_THREADS": 4
  },
  "thermal_data": {
    "pre_test_temp": 62.5,
    "post_test_temp": 68.3,
    "pre_test_voltage": 825.0,
    "post_test_voltage": 825.0,
    "cooling_time": 0.0
  },
  "timestamp": "2025-10-17T15:30:45.123456"
}
```

#### `results.csv`
모든 configuration 조합의 테스트 결과:
```csv
DXRT_TASK_MAX_LOAD,CUSTOM_INTRA_OP_THREADS_COUNT,NFH_OUTPUT_WORKER_THREADS,fps,latency,success
3,1,1,198.5,5.04,true
3,2,2,234.7,4.26,true
...
9,3,4,352.7,2.84,true
...
```

### 4. `logs/automation.log` - 전체 실행 로그

자동화 과정의 상세 로그:
```
=== DX-Fit Model Testing Automation v1.2.0 ===
Experiment: 20241016_143052
Results: results/20241016_143052

[1/3] Testing model: AlexNet_5
  → Loop selection: 150 loops (model: 12.3MB, tier: High, strategy: bayesian)
  → Running dx-fit...
  ✓ dx-fit completed
    Best FPS: 352.7
    Best Latency: 2.84ms
...
```

---

## 💡 결과 활용 방법

### 1. Excel로 즉시 분석 (권장) 📊

```bash
# 1. 최신 실험 찾기
ls -lt results/ | head -5

# 2. summary.csv 열기
cd results/20241016_143052/
# Windows
explorer.exe summary.csv
# macOS
open summary.csv
# Linux with LibreOffice
libreoffice summary.csv
```

#### Excel 활용 팁

**필터로 패턴 찾기**:
1. 데이터 → 필터 활성화
2. `DXRT_TASK_MAX_LOAD` = 9 선택
3. `fps_improvement` 내림차순 정렬
4. → Task Load 9일 때 성능이 좋은 모델 확인

**피벗 테이블로 통계 분석**:
1. 삽입 → 피벗 테이블
2. 행: `DXRT_TASK_MAX_LOAD`
3. 값: `fps_improvement` (평균)
4. → 각 Task Load 설정의 평균 성능 확인

**조건부 서식으로 시각화**:
1. `fps_improvement_percent` 열 선택
2. 홈 → 조건부 서식 → 색조
3. → 성능 향상이 높은 모델 강조

**차트로 시각화**:
1. `model_name`, `default_fps`, `best_fps` 선택
2. 삽입 → 세로 막대형 차트
3. → Before/After 비교 차트

### 2. 터미널 결과 리포트 (빠른 확인)

Excel 사용 전에 터미널에서 바로 요약 확인:

```bash
# 최신 결과 자동 분석
python3 analyze_results.py

# 특정 파일 분석
python3 analyze_results.py results/20241016_143052/summary.csv

# Top 20 모델 표시
python3 analyze_results.py -n 20
```

**출력 예시**:
```
================================================================================
  DX-Fit 결과 분석 리포트
================================================================================
  파일: results/20241016_143052/summary.csv
  시간: 2024-10-16 14:35:22
================================================================================

================================================================================
  📊 테스트 요약
================================================================================

  총 모델 수:            50개
  Default 테스트 성공:   50개  (100.0%)
  dx-fit 최적화 성공:    48개  ( 96.0%)
  실패:                   2개  (  4.0%)

  🚀 성능 향상 통계:
     평균:    1.67x
     최소:    1.12x
     중앙:    1.58x
     최대:    2.89x

  📈 성능 향상 분포:
     탁월 (≥2.0x):     12개  ████████████
     우수 (1.5-2.0x):  25개  █████████████████████████
     양호 (1.2-1.5x):   9개  █████████
     미미 (<1.2x):      2개  ██

  ⏱️  실행 시간:
     총 시간:       3시간 42분  (222.5분)
     모델당 평균:   4.5분

================================================================================
  🏆 Top 10 성능 향상 모델
================================================================================

순위 등급 모델                                          Before      After       향상        
------------------------------------------------------------------------------------------
1    🌟  YOLOv8n-YOLOv8-Nano                           58.3 FPS   168.5 FPS  2.89x (+ 189%)
2    ⭐  ResNet50-ResNet-51                            85.2 FPS   189.4 FPS  2.22x (+ 122%)
3    ⭐  EfficientNet-B0                               102.7 FPS   215.3 FPS  2.10x (+ 110%)
...

💡 주요 인사이트:
  🥇 최고 성능 향상: YOLOv8n (2.89x)
  📈 평균 이상 향상: 28개 (58.3%)
  ⚡ 2배 이상 향상: 12개 (25.0%)
```

이 리포트로 빠르게 결과를 확인한 후, 상세 분석은 Excel에서 수행하세요.

### 3. 개별 모델 상세 분석

특정 모델의 전체 튜닝 과정을 확인:

```bash
cd results/20241016_143052/models/ResNet50/

# 1. 최적 configuration 확인
cat best_config.json | python3 -m json.tool

# 2. 전체 테스트 결과 확인
column -t -s, results.csv | less -S

# 3. 튜닝 과정 로그
cat tuning_report_*.txt
```

---

## 🔍 자주 하는 분석

### Q1: 성능이 가장 많이 향상된 모델은?

```bash
# CSV를 정렬하여 확인
sort -t, -k8 -rn results/20241016_143052/summary.csv | head -10
# 또는 Excel에서 fps_improvement_percent 열 정렬
```

### Q2: 특정 configuration이 좋은 모델 타입은?

```bash
# DXRT_TASK_MAX_LOAD=9일 때 성능이 좋은 모델들
grep ",9," summary.csv | sort -t, -k8 -rn
# 또는 Excel 필터 사용
```

### Q3: 실패한 테스트는 몇 개?

```bash
# dxfit_success = NO인 항목
grep ",NO," summary.csv | wc -l
# 또는 Excel에서 dxfit_success 필터
```

### Q4: 평균 성능 향상은?

```python
import pandas as pd
df = pd.read_csv('summary.csv')
print(f"Average: {df['fps_improvement'].mean():.2f}x")
print(f"Median: {df['fps_improvement'].median():.2f}x")
print(f"Std Dev: {df['fps_improvement'].std():.2f}")
```

### Q5: Configuration 조합 패턴은?

```python
import pandas as pd
df = pd.read_csv('summary.csv')

# 가장 흔한 configuration
common_config = df.groupby([
    'DXRT_TASK_MAX_LOAD',
    'CUSTOM_INTRA_OP_THREADS_COUNT',
    'NFH_OUTPUT_WORKER_THREADS'
]).size().sort_values(ascending=False).head(10)

print("Top 10 most common configurations:")
print(common_config)
```

---

## 📈 시계열 분석

여러 실험을 비교하려면:

```bash
# 실험 히스토리
ls -lt results/
```

```python
# 여러 실험 비교
import pandas as pd
import glob

experiments = glob.glob('results/*/summary.csv')
experiments.sort()

for exp in experiments:
    df = pd.read_csv(exp)
    avg_improvement = df['fps_improvement'].mean()
    exp_date = exp.split('/')[2]
    print(f"{exp_date}: {avg_improvement:.2f}x average")
```

---

## 🚨 Troubleshooting

### summary.csv에 데이터가 비어있다

**원인**: 테스트가 실패했거나 dx-fit이 실행되지 않음

**해결**:
1. `logs/automation.log` 확인
2. 에러 메시지 찾기
3. 실패한 모델의 `models/{name}/` 확인

### models/ 디렉토리가 비어있다

**원인**: dx-fit이 결과 파일을 생성하지 못함

**해결**:
1. dx-fit 명령어 확인 (`which dx-fit`)
2. test.yaml 설정 확인
3. 모델 경로가 올바른지 확인

### Excel에서 한글이 깨진다

**해결**:
1. CSV → UTF-8 with BOM으로 저장
2. 또는 LibreOffice Calc 사용 (UTF-8 지원)

---

## 💾 백업 및 공유

### 실험 결과 백업

```bash
# 특정 실험 백업
tar -czf experiment_20241016_143052.tar.gz results/20241016_143052/

# 모든 실험 백업
tar -czf all_experiments_$(date +%Y%m%d).tar.gz results/
```

### 결과 공유

```bash
# 요약만 공유 (가벼움)
cp results/20241016_143052/summary.csv ~/shared/

# 전체 공유
scp -r results/20241016_143052/ user@server:/path/
```

---

## 🔗 관련 문서

- **자동화 도구**: [README.md](README.md)
- **Loop 선택 가이드**: [LOOP_SELECTION_V2_GUIDE.md](LOOP_SELECTION_V2_GUIDE.md)
- **dx-fit 문서**: `../dx-fit/README.md`

---

**v1.2.0 핵심**: summary.csv가 완전하므로 recover 단계가 불필요합니다!
