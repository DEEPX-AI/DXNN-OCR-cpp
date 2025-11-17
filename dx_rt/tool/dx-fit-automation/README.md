# DX-Fit Automation

**여러 모델에 대한 자동화된 dx-fit 최적화 도구**

---

## 🚀 빠른 시작

```bash
cd /home/dxuser/workspace/jjg/dx_rt/tool/dx-fit-automation

# dx-fit 설정 파일 복사
cp ../dx-fit/examples/03_bayesian_quick.yaml ./test.yaml

# 자동 loop 선정을 원하면 target_duration 추가
echo "target_duration: 3.0" >> test.yaml

# 실행
python3 automate_model_testing.py -c test.yaml
```

**💡 Tip**: dx-fit가 `target_duration` 설정에 따라 자동으로 적절한 loop count를 계산합니다. `loop-selector` 도구를 별도로 실행할 필요가 없습니다.

---

## 💡 기본 사용법

### 1. 설정 파일 준비

dx-fit 예제 중 하나를 선택하세요:

| 파일 | 전략 | 반복 | 시간/모델 | 용도 |
|------|------|------|-----------|------|
| `02_quick_random.yaml` | Random | 10 | 2-3분 | 빠른 탐색 |
| `03_bayesian_quick.yaml` ⭐ | Bayesian | 15 | 3-5분 | **권장** |
| `04_bayesian_standard.yaml` | Bayesian | 30 | 10-15분 | 정밀 최적화 |
| `05_grid_small.yaml` | Grid | 6-10 | 3-5분 | 작은 범위 전수 |

```bash
# 권장 설정 복사
cp ../dx-fit/examples/03_bayesian_quick.yaml ./my_test.yaml

# 필요시 수정
vi my_test.yaml
```

### 2. 모델 리스트 준비

`config/model_list.txt` 파일에 테스트할 모델 이름 작성:

```
AlexNet_5-AlexNet-6
ResNet50-ResNet-51
MobileNet-MobileNet-1
```

### 3. 실행

```bash
# 기본 실행
python3 automate_model_testing.py -c my_test.yaml

# 옵션 지정
python3 automate_model_testing.py \
    -c my_test.yaml \
    -m config/model_list.txt \
    -p /mnt/regression_storage/dxnn_regr_data/M1B/RELEASE
```

**주요 옵션:**
- `-c, --config`: dx-fit 설정 파일 (필수)
- `-m, --model-list`: 모델 리스트 파일 (기본: config/test_model_list.txt)
- `-p, --model-path`: 모델 기본 경로
- `-o, --output`: 결과 디렉토리 (기본: results/)

---

## 📊 결과 확인

### 결과 디렉토리 구조

```
results/
└── 20241016_143052/              # 실험 타임스탬프
    ├── summary.csv               # Excel 분석용 (메인)
    ├── detailed.json             # 상세 데이터
    ├── models/                   # 모델별 결과
    │   ├── AlexNet_5/
    │   │   ├── best_config.json
    │   │   └── results.csv
    │   └── ResNet50/
    │       └── ...
    └── logs/
        └── automation.log
```

### Excel 분석

```bash
# 1. 최신 결과 찾기
ls -lt results/

# 2. summary.csv 열기
cd results/20241016_143052/
open summary.csv            # macOS
explorer.exe summary.csv    # Windows
libreoffice summary.csv     # Linux
```

**summary.csv 주요 컬럼:**
- `default_fps` / `best_fps` - 초기 vs 최적화 FPS
- `fps_improvement` - 성능 향상 배수
- `DXRT_TASK_MAX_LOAD`, `CUSTOM_INTRA_OP_THREADS_COUNT` 등 - 최적 설정값

**Excel 활용:**
- 필터: 특정 설정값 찾기 (예: TASK_LOAD=9)
- 정렬: fps_improvement 내림차순
- 피벗 테이블: configuration 패턴 분석
- 차트: Before/After 비교

📘 **상세 분석 가이드**: [RESULTS_GUIDE.md](RESULTS_GUIDE.md)

---

## ⚙️ 설정 파일 수정

dx-fit 예제를 복사한 후 필요시 수정:

```yaml
# 실행 설정
loop_count: 50              # 고정 반복 횟수
# 또는
target_duration: 3.0        # 자동 loop 선정 (초 단위)

warmup_runs: 3              # 워밍업 횟수
timeout: 300                # 타임아웃 (초)
use_ort: true               # ONNX Runtime 사용

# 전략
strategy: bayesian          # bayesian, random, grid
max_random_samples: 15      # Bayesian/Random 반복 횟수

# 최적화 파라미터
parameters:
  DXRT_TASK_MAX_LOAD: [3, 6, 9, 12, 15]
  CUSTOM_INTRA_OP_THREADS_COUNT: [1, 2, 3, 4]
  NFH_OUTPUT_WORKER_THREADS: [1, 2, 3, 4, 5]
```

> **Note**: 
> - `loop_count`: 고정된 반복 횟수를 사용하고 싶을 때
> - `target_duration`: 모델 속도에 따라 자동으로 적절한 loop 수를 계산 (권장)
> - Loop 선정은 **dx-fit가 내부적으로 처리**하므로, dx-fit-automation은 config를 그대로 전달합니다

---

## 🔧 문제 해결

### "dx-fit not found"

```bash
# dx-fit 확인
ls ../dx-fit/dx-fit

# 있어야 함: -rwxr-xr-x ... dx-fit
```

### "model not found"

```bash
# 모델 경로 확인
ls /mnt/regression_storage/dxnn_regr_data/M1B/RELEASE/

# 경로 지정 실행
python3 automate_model_testing.py -c test.yaml -p /your/model/path
```

### 설정 파일 에러

```bash
# YAML 문법 검증
python3 -c "import yaml; yaml.safe_load(open('test.yaml'))"
```

---

## 📁 파일 구조

```
dx-fit-automation/
├── automate_model_testing.py    # 메인 스크립트
├── loop_selection_policy.py     # DEPRECATED - use loop-selector tool instead
├── analyze_results.py            # 결과 리포트 도구
├── quickstart.sh                 # 대화형 실행
│
├── README.md                     # 이 파일
├── RESULTS_GUIDE.md             # 결과 분석 가이드
│
└── config/
    ├── model_list.txt           # 전체 모델 리스트
    └── test_model_list.txt      # 테스트용 (6개)

../loop-selector/                 # 독립 실행 가능한 CLI 도구
├── loop-selector                # loop count 추천 CLI
└── LOOP-SELECTOR.md             # 사용 가이드
```

**Architecture Note**: 
- dx-fit-automation은 **config를 그대로 dx-fit에 전달**합니다
- Loop 선정 로직은 **dx-fit가 내부적으로 처리** (`target_duration` 설정 시 자동)
- dx-fit은 필요시 `loop-selector` 도구를 활용할 수 있습니다
- `loop_selection_policy.py`는 deprecated되었습니다

---

## 💡 팁

**✅ DO:**
- dx-fit 공식 예제 사용
- 작은 모델 리스트로 먼저 테스트
- `03_bayesian_quick.yaml` 기본으로 사용
- 장시간 실행시 백그라운드: `nohup ... &`

**❌ DON'T:**
- 설정 파일 처음부터 작성 (예제 복사 후 수정)
- `grid_full` 전체 모델에 사용 (시간 오래 걸림)
- thermal management 없이 장시간 실행

---

**DX-RT 서브 도구** | Telechips
