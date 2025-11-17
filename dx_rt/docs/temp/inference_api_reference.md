# DXRT Inference API 완전 참조 가이드

## 개요

이 문서는 DXRT의 모든 추론 관련 API들에 대한 상세한 참조 가이드입니다. C++과 Python 버전 모두의 허용 입력 형태와 예상 출력 형태를 comprehensive하게 정리했습니다.

## 목차

1. [C++ Inference Engine API](#c-inference-engine-api)
2. [Python Inference Engine API](#python-inference-engine-api)
3. [입력 형태 분석 로직](#입력-형태-분석-로직)
4. [출력 형태 규칙](#출력-형태-규칙)
5. [특수 케이스](#특수-케이스)

---

## C++ Inference Engine API

### 1. 동기 추론 API

#### 1.1 Run (Single Input/Output)

```cpp
TensorPtrs Run(void *inputPtr, void *userArg = nullptr, void *outputPtr = nullptr)
```

| 입력 형태 | 설명 | 모델 타입 | 출력 형태 | 비고 |
|-----------|------|-----------|-----------|------|
| `void* inputPtr` | 단일 입력 포인터 | Single-Input | `TensorPtrs` (Vector) | 전통적인 방식 |
| `void* inputPtr` | 연결된 버퍼 포인터 | Multi-Input | `TensorPtrs` (Vector) | Auto-split 적용 |

**예제:**
```cpp
// Single input model
auto outputs = ie.Run(inputData);

// Multi-input model (auto-split)
auto outputs = ie.Run(concatenatedInput);
```

#### 1.2 Run (Batch)

```cpp
std::vector<TensorPtrs> Run(
    const std::vector<void*>& inputBuffers,
    const std::vector<void*>& outputBuffers,
    const std::vector<void*>& userArgs
)
```

| 입력 형태 | 조건 | 해석 | 출력 형태 | 비고 |
|-----------|------|------|-----------|------|
| `vector<void*>` (size=1) | Single-Input | 단일 추론 | `vector<TensorPtrs>` (size=1) | 특수 케이스 |
| `vector<void*>` (size=N) | Single-Input | 배치 추론 | `vector<TensorPtrs>` (size=N) | N개 샘플 |
| `vector<void*>` (size=M) | Multi-Input, M==input_count | 단일 추론 | `vector<TensorPtrs>` (size=1) | Multi-input 단일 |
| `vector<void*>` (size=N*M) | Multi-Input, N*M==배수 | 배치 추론 | `vector<TensorPtrs>` (size=N) | N개 샘플, M개 입력 |

**예제:**
```cpp
// Single input batch
std::vector<void*> batchInputs = {sample1, sample2, sample3};
auto batchOutputs = ie.Run(batchInputs, outputBuffers, userArgs);

// Multi-input single
std::vector<void*> multiInputs = {input1, input2}; // M=2
auto singleOutput = ie.Run(multiInputs, {outputBuffer}, {userArg});

// Multi-input batch  
std::vector<void*> multiBatch = {s1_i1, s1_i2, s2_i1, s2_i2}; // N=2, M=2
auto batchOutputs = ie.Run(multiBatch, outputBuffers, userArgs);
```

#### 1.3 RunMultiInput (Dictionary)

```cpp
TensorPtrs RunMultiInput(
    const std::map<std::string, void*>& inputTensors, 
    void *userArg = nullptr, 
    void *outputPtr = nullptr
)
```

| 입력 형태 | 제약 조건 | 출력 형태 | 비고 |
|-----------|----------|-----------|------|
| `map<string, void*>` | 모든 입력 텐서 이름 포함 | `TensorPtrs` | Multi-input 전용 |

**예제:**
```cpp
std::map<std::string, void*> inputs = {
    {"input1", data1},
    {"input2", data2}
};
auto outputs = ie.RunMultiInput(inputs);
```

#### 1.4 RunMultiInput (Vector)

```cpp
TensorPtrs RunMultiInput(
    const std::vector<void*>& inputPtrs, 
    void *userArg = nullptr, 
    void *outputPtr = nullptr
)
```

| 입력 형태 | 제약 조건 | 출력 형태 | 비고 |
|-----------|----------|-----------|------|
| `vector<void*>` | size == input_tensor_count | `TensorPtrs` | 순서는 GetInputTensorNames() |

### 2. 비동기 추론 API

#### 2.1 RunAsync (Single)

```cpp
int RunAsync(void *inputPtr, void *userArg = nullptr, void *outputPtr = nullptr)
```

| 입력 형태 | 모델 타입 | 출력 형태 | 비고 |
|-----------|----------|-----------|------|
| `void* inputPtr` | Single-Input | `int` (jobId) | Wait(jobId)로 결과 수신 |
| `void* inputPtr` | Multi-Input | `int` (jobId) | Auto-split 적용 |

#### 2.2 RunAsync (Vector)

```cpp
int RunAsync(const std::vector<void*>& inputPtrs, void *userArg = nullptr, void *outputPtr = nullptr)
```

| 입력 형태 | 조건 | 해석 | 출력 형태 | 비고 |
|-----------|------|------|-----------|------|
| `vector<void*>` (size==input_count) | Multi-Input | Multi-input 단일 | `int` (jobId) | 권장 방식 |
| `vector<void*>` (size!=input_count) | Any | 첫 번째 요소만 사용 | `int` (jobId) | Fallback |

#### 2.3 RunAsyncMultiInput (Dictionary)

```cpp
int RunAsyncMultiInput(
    const std::map<std::string, void*>& inputTensors, 
    void *userArg = nullptr, 
    void *outputPtr = nullptr
)
```

| 입력 형태 | 제약 조건 | 출력 형태 | 비고 |
|-----------|----------|-----------|------|
| `map<string, void*>` | Multi-input 모델 전용 | `int` (jobId) | 가장 명확한 방식 |

#### 2.4 RunAsyncMultiInput (Vector)

```cpp
int RunAsyncMultiInput(
    const std::vector<void*>& inputPtrs, 
    void *userArg = nullptr, 
    void *outputPtr = nullptr
)
```

| 입력 형태 | 제약 조건 | 출력 형태 | 비고 |
|-----------|----------|-----------|------|
| `vector<void*>` | size == input_tensor_count | `int` (jobId) | Dictionary로 변환됨 |

### 3. 장치 검증 API

#### 3.1 ValidateDevice (Single)

```cpp
TensorPtrs ValidateDevice(void *inputPtr, int deviceId = 0)
```

| 입력 형태 | 모델 타입 | 출력 형태 | 제약 조건 |
|-----------|----------|-----------|-----------|
| `void* inputPtr` | Any | `TensorPtrs` | Debug 모드 모델만 |

#### 3.2 ValidateDevice (Vector)

```cpp
TensorPtrs ValidateDevice(const std::vector<void*>& inputPtrs, int deviceId = 0)
```

| 입력 형태 | 조건 | 해석 | 출력 형태 |
|-----------|------|------|-----------|
| `vector<void*>` (size==input_count) | Multi-Input | Multi-input 검증 | `TensorPtrs` |
| `vector<void*>` (other) | Any | 첫 번째 요소만 사용 | `TensorPtrs` |

#### 3.3 ValidateDeviceMultiInput

```cpp
TensorPtrs ValidateDeviceMultiInput(const std::map<std::string, void*>& inputTensors, int deviceId = 0)
TensorPtrs ValidateDeviceMultiInput(const std::vector<void*>& inputPtrs, int deviceId = 0)
```

---

## Python Inference Engine API

### 1. 동기 추론 API

#### 1.1 run (Unified API)

```python
def run(
    input_data: Union[np.ndarray, List[np.ndarray], List[List[np.ndarray]]],
    output_buffers: Optional[Union[List[np.ndarray], List[List[np.ndarray]]]] = None,
    user_args: Optional[Union[Any, List[Any]]] = None
) -> Union[List[np.ndarray], List[List[np.ndarray]]]
```

**상세 입력/출력 매트릭스:**

| 입력 타입 | 입력 조건 | 모델 타입 | 해석 | 출력 타입 | 출력 구조 |
|-----------|----------|----------|------|-----------|-----------|
| `np.ndarray` | size == total_input_size | Multi-Input | Auto-split 단일 | `List[np.ndarray]` | 단일 샘플 출력 |
| `np.ndarray` | size != total_input_size | Single-Input | 단일 추론 | `List[np.ndarray]` | 단일 샘플 출력 |
| `List[np.ndarray]` | len == 1 | Single-Input | 단일 추론 | `List[np.ndarray]` | 단일 샘플 출력 |
| `List[np.ndarray]` | len == input_count | Multi-Input | 단일 추론 | `List[np.ndarray]` | 단일 샘플 출력 |
| `List[np.ndarray]` | len == N*input_count | Multi-Input | 배치 추론 (N샘플) | `List[List[np.ndarray]]` | N개 샘플 출력 |
| `List[np.ndarray]` | len > 1 | Single-Input | 배치 추론 | `List[List[np.ndarray]]` | len개 샘플 출력 |
| `List[List[np.ndarray]]` | 명시적 배치 | Any | 배치 추론 | `List[List[np.ndarray]]` | 외부 리스트 크기만큼 |

**Auto-split 특수 케이스:**

| 조건 | 입력 예시 | 해석 | 출력 |
|------|----------|------|------|
| Multi-input + 첫 번째 요소가 total_size | `[concatenated_array]` | Auto-split 단일 | `List[np.ndarray]` |
| Multi-input + 모든 요소가 total_size | `[concat1, concat2, concat3]` | Auto-split 배치 | `List[List[np.ndarray]]` |

**예제:**
```python
# 1. Single array auto-split (multi-input)
concatenated = np.zeros(ie.get_input_size(), dtype=np.uint8)
outputs = ie.run(concatenated)  # List[np.ndarray]

# 2. Multi-input single
input_list = [input1_array, input2_array]  # len == 2
outputs = ie.run(input_list)  # List[np.ndarray]

# 3. Multi-input batch (flattened)
flattened = [s1_i1, s1_i2, s2_i1, s2_i2]  # 2 samples, 2 inputs each
outputs = ie.run(flattened)  # List[List[np.ndarray]], len=2

# 4. Multi-input batch (explicit)
explicit_batch = [[s1_i1, s1_i2], [s2_i1, s2_i2]]
outputs = ie.run(explicit_batch)  # List[List[np.ndarray]], len=2

# 5. Single-input batch
single_batch = [sample1, sample2, sample3]
outputs = ie.run(single_batch)  # List[List[np.ndarray]], len=3
```

#### 1.2 run_multi_input (Dictionary)

```python
def run_multi_input(
    input_tensors: Dict[str, np.ndarray],
    output_buffers: Optional[List[np.ndarray]] = None,
    user_arg: Any = None
) -> List[np.ndarray]
```

| 입력 타입 | 제약 조건 | 출력 타입 | 비고 |
|-----------|----------|-----------|------|
| `Dict[str, np.ndarray]` | 모든 입력 텐서 포함 | `List[np.ndarray]` | Multi-input 전용 |

### 2. 비동기 추론 API

#### 2.1 run_async

```python
def run_async(
    input_data: Union[np.ndarray, List[np.ndarray]],
    user_arg: Any = None,
    output_buffer: Optional[Union[np.ndarray, List[np.ndarray]]] = None
) -> int
```

| 입력 타입 | 조건 | 해석 | 출력 타입 | 제약 |
|-----------|------|------|-----------|------|
| `np.ndarray` | Any | 단일 추론 | `int` (jobId) | 배치 지원 안함 |
| `List[np.ndarray]` | len == input_count | Multi-input 단일 | `int` (jobId) | 배치 지원 안함 |
| `List[np.ndarray]` | len == 1 | Single-input 단일 | `int` (jobId) | 배치 지원 안함 |

#### 2.2 run_async_multi_input

```python
def run_async_multi_input(
    input_tensors: Dict[str, np.ndarray],
    user_arg: Any = None,
    output_buffer: Optional[List[np.ndarray]] = None
) -> int
```

| 입력 타입 | 제약 조건 | 출력 타입 | 비고 |
|-----------|----------|-----------|------|
| `Dict[str, np.ndarray]` | Multi-input 모델 전용 | `int` (jobId) | 단일 추론만 |

### 3. 장치 검증 API

#### 3.1 validate_device

```python
def validate_device(
    input_data: Union[np.ndarray, List[np.ndarray]], 
    device_id: int = 0
) -> List[np.ndarray]
```

| 입력 타입 | 조건 | 해석 | 출력 타입 |
|-----------|------|------|-----------|
| `np.ndarray` | Any | 단일 검증 | `List[np.ndarray]` |
| `List[np.ndarray]` | len == input_count | Multi-input 검증 | `List[np.ndarray]` |
| `List[np.ndarray]` | len != input_count | 첫 번째 요소만 | `List[np.ndarray]` |

#### 3.2 validate_device_multi_input

```python
def validate_device_multi_input(
    input_tensors: Dict[str, np.ndarray], 
    device_id: int = 0
) -> List[np.ndarray]
```

### 4. 기타 API

#### 4.1 run_benchmark

```python
def run_benchmark(
    num_loops: int, 
    input_data: Optional[List[np.ndarray]] = None
) -> float
```

| 입력 타입 | 제약 조건 | 출력 타입 | 비고 |
|-----------|----------|-----------|------|
| `List[np.ndarray]` | Single input format | `float` (FPS) | 첫 번째 요소 반복 사용 |

#### 4.2 wait

```python
def wait(job_id: int) -> List[np.ndarray]
```

---

## 입력 형태 분석 로직

### Python 입력 분석 플로우

```python
def _analyze_input_format(input_data):
    # 1. np.ndarray 검사
    if isinstance(input_data, np.ndarray):
        if should_auto_split_input(input_data):
            return auto_split_single_inference()
        else:
            return single_inference()
    
    # 2. List 검사
    if isinstance(input_data, list):
        if isinstance(input_data[0], list):
            # List[List[np.ndarray]] - 명시적 배치
            return explicit_batch_inference()
        else:
            # List[np.ndarray] - 복잡한 분석 필요
            return analyze_list_ndarray(input_data)
```

### List[np.ndarray] 분석 상세

```python
def analyze_list_ndarray(input_data):
    input_count = len(input_data)
    
    if is_multi_input_model():
        expected_count = get_input_tensor_count()
        
        if input_count == expected_count:
            return single_inference()
        elif input_count % expected_count == 0:
            batch_size = input_count // expected_count
            return batch_inference(batch_size)
        elif all(should_auto_split_input(arr) for arr in input_data):
            return auto_split_batch_inference()
        else:
            raise ValueError("Invalid input count")
    else:  # Single-input model
        if input_count == 1:
            return single_inference()
        else:
            return batch_inference(input_count)
```

## 출력 형태 규칙

### 1. 단일 추론 출력

| API | 출력 형태 | 구조 |
|-----|-----------|------|
| C++ Run | `TensorPtrs` | `vector<shared_ptr<Tensor>>` |
| Python run | `List[np.ndarray]` | `[output1, output2, ...]` |

### 2. 배치 추론 출력

| API | 출력 형태 | 구조 |
|-----|-----------|------|
| C++ Run (batch) | `vector<TensorPtrs>` | `[sample1_outputs, sample2_outputs, ...]` |
| Python run (batch) | `List[List[np.ndarray]]` | `[[s1_o1, s1_o2], [s2_o1, s2_o2], ...]` |

### 3. 비동기 출력

| API | 즉시 반환 | Wait 후 |
|-----|-----------|---------|
| C++ RunAsync | `int` (jobId) | `TensorPtrs` |
| Python run_async | `int` (jobId) | `List[np.ndarray]` |

## 특수 케이스

### 1. Auto-Split 조건

**C++:**
```cpp
bool shouldAutoSplitInput() const {
    return _isMultiInput && _inputTasks.size() == 1;
}
```

**Python:**
```python
def _should_auto_split_input(input_data: np.ndarray) -> bool:
    if not self.is_multi_input_model():
        return False
    
    expected_total_size = self.get_input_size()
    actual_size = input_data.nbytes
    
    return actual_size == expected_total_size
```

### 2. 배치 크기 결정

| 조건 | 배치 크기 계산 |
|------|---------------|
| Single-input + List[np.ndarray] | `len(input_data)` |
| Multi-input + List[np.ndarray] | `len(input_data) // input_tensor_count` |
| List[List[np.ndarray]] | `len(input_data)` |

### 3. 에러 조건

| 조건 | 에러 타입 | 메시지 |
|------|----------|--------|
| Multi-input + 잘못된 크기 | `ValueError` | "Invalid input count for multi-input model" |
| 비동기 + 배치 | `ValueError` | "Batch inference not supported in async" |
| 빈 입력 | `ValueError` | "Input data cannot be empty" |
| 타입 불일치 | `TypeError` | "Expected np.ndarray or List[np.ndarray]" |

### 4. Output Buffer 처리

#### Python Output Buffer 매트릭스

| 입력 형태 | Output Buffer 형태 | 처리 방식 |
|-----------|-------------------|-----------|
| 단일 추론 | `None` | 자동 할당 |
| 단일 추론 | `List[np.ndarray]` | 사용자 제공 |
| 단일 추론 | `np.ndarray` (total_size) | Auto-split 후 사용 |
| 배치 추론 | `List[List[np.ndarray]]` | 명시적 배치 버퍼 |
| 배치 추론 | `List[np.ndarray]` | 플래튼된 배치 버퍼 |

## 성능 고려사항

### 1. 메모리 할당

| 방식 | 장점 | 단점 |
|------|------|------|
| 자동 할당 (No Buffer) | 사용 편의성 | 매번 메모리 할당 |
| 사용자 제공 (With Buffer) | 성능 최적화 | 메모리 관리 복잡 |

### 2. 추론 방식

| 방식 | 용도 | 특징 |
|------|------|------|
| 동기 추론 | 간단한 처리 | 순차 실행 |
| 비동기 추론 | 높은 처리량 | 콜백 관리 필요 |
| 배치 추론 | 대량 처리 | 메모리 사용량 증가 |

---

이 문서는 DXRT의 모든 추론 API에 대한 완전한 참조를 제공합니다. 각 API의 정확한 사용법과 예상 동작을 이해하여 올바른 추론 코드를 작성할 수 있습니다. 