# Multi-Input 모델 추론 API 가이드

## 개요

DXRT는 여러 개의 입력 텐서를 가진 multi-input 모델에 대한 다양한 추론 방식을 지원합니다. 이 문서는 multi-input 모델의 추론 API 활용 방법을 설명합니다.

## Multi-Input 모델 확인

### C++
```cpp
dxrt::InferenceEngine ie(modelPath);

// 모델이 multi-input인지 확인
bool isMultiInput = ie.IsMultiInputModel();

// 입력 텐서 개수 확인
int inputCount = ie.GetInputTensorCount();

// 입력 텐서 이름들 확인
std::vector<std::string> inputNames = ie.GetInputTensorNames();

// 입력 텐서와 태스크 매핑 확인
std::map<std::string, std::string> mapping = ie.GetInputTensorToTaskMapping();
```

### Python
```python
from dx_engine import InferenceEngine

ie = InferenceEngine(model_path)

# 모델이 multi-input인지 확인
is_multi_input = ie.is_multi_input_model()

# 입력 텐서 개수 확인
input_count = ie.get_input_tensor_count()

# 입력 텐서 이름들 확인
input_names = ie.get_input_tensor_names()

# 입력 텐서와 태스크 매핑 확인
mapping = ie.get_input_tensor_to_task_mapping()
```

## Multi-Input 추론 방식

### 1. No Output Buffer Tests (자동 할당)

출력 버퍼를 제공하지 않고 추론 엔진이 자동으로 메모리를 할당하는 방식입니다.

#### 1.1 Dictionary Format (권장)

입력 텐서를 이름으로 매핑하여 제공하는 방식입니다. 가장 명확하고 실수가 적은 방법입니다.

##### C++
```cpp
// Dictionary format 사용 (자동 할당)
std::map<std::string, void*> inputTensors;
inputTensors["input1"] = input1_data;
inputTensors["input2"] = input2_data;

// 출력 버퍼 없이 동기 추론 (자동 할당)
auto outputs = ie.RunMultiInput(inputTensors);
```

##### Python
```python
# Dictionary format 사용 (자동 할당)
input_tensors = {
    "input1": input1_array,
    "input2": input2_array
}

# 출력 버퍼 없이 동기 추론 (자동 할당)
outputs = ie.run_multi_input(input_tensors)
```

#### 1.2 Vector Format

입력 텐서를 순서대로 벡터/리스트로 제공하는 방식입니다. `GetInputTensorNames()`로 반환된 순서와 일치해야 합니다.

##### C++
```cpp
// Vector format 사용 (GetInputTensorNames() 순서와 일치)
std::vector<void*> inputPtrs = {input1_data, input2_data};

// 출력 버퍼 없이 동기 추론 (자동 할당)
auto outputs = ie.RunMultiInput(inputPtrs);
```

##### Python
```python
# Vector format 사용 (get_input_tensor_names() 순서와 일치)
input_list = [input1_array, input2_array]

# 출력 버퍼 없이 동기 추론 (자동 할당)
outputs = ie.run(input_list)
```

#### 1.3 Auto-Split Format

단일 연결된 버퍼를 자동으로 여러 입력으로 분할하는 방식입니다. 총 입력 크기가 일치할 때 자동으로 적용됩니다.

##### C++
```cpp
// 모든 입력을 연결한 단일 버퍼
std::vector<uint8_t> concatenatedInput(ie.GetInputSize());
// ... 데이터 채우기 ...

// 자동 분할되어 처리됨 (출력 버퍼 자동 할당)
auto outputs = ie.Run(concatenatedInput.data());
```

##### Python
```python
# 모든 입력을 연결한 단일 배열
concatenated_input = np.zeros(ie.get_input_size(), dtype=np.uint8)
# ... 데이터 채우기 ...

# 자동 분할되어 처리됨 (출력 버퍼 자동 할당)
outputs = ie.run(concatenated_input)
```

### 2. With Output Buffer Tests (사용자 제공)

사용자가 출력 버퍼를 미리 할당하여 제공하는 방식입니다. 메모리 관리와 성능 최적화에 유리합니다.

#### 2.1 Dictionary Format

##### C++
```cpp
// Dictionary format 사용
std::map<std::string, void*> inputTensors;
inputTensors["input1"] = input1_data;
inputTensors["input2"] = input2_data;

// 출력 버퍼 생성
std::vector<uint8_t> outputBuffer(ie.GetOutputSize());

// 동기 추론 (사용자 출력 버퍼)
auto outputs = ie.RunMultiInput(inputTensors, userArg, outputBuffer.data());
```

##### Python
```python
# Dictionary format 사용
input_tensors = {
    "input1": input1_array,
    "input2": input2_array
}

# 출력 버퍼 생성
output_buffers = [np.zeros(size, dtype=np.uint8) for size in ie.get_output_tensor_sizes()]

# 동기 추론 (사용자 출력 버퍼)
outputs = ie.run_multi_input(input_tensors, output_buffers=output_buffers)
```

#### 2.2 Vector Format

##### C++
```cpp
// Vector format 사용 (GetInputTensorNames() 순서와 일치)
std::vector<void*> inputPtrs = {input1_data, input2_data};

// 출력 버퍼 생성
std::vector<uint8_t> outputBuffer(ie.GetOutputSize());

// 동기 추론 (사용자 출력 버퍼)
auto outputs = ie.RunMultiInput(inputPtrs, userArg, outputBuffer.data());
```

##### Python
```python
# Vector format 사용 (get_input_tensor_names() 순서와 일치)
input_list = [input1_array, input2_array]

# 출력 버퍼 생성
output_buffers = [np.zeros(size, dtype=np.uint8) for size in ie.get_output_tensor_sizes()]

# 동기 추론 (사용자 출력 버퍼)
outputs = ie.run(input_list, output_buffers=output_buffers)
```

#### 2.3 Auto-Split Format

##### C++
```cpp
// 모든 입력을 연결한 단일 버퍼
std::vector<uint8_t> concatenatedInput(ie.GetInputSize());
// ... 데이터 채우기 ...

// 출력 버퍼 생성
std::vector<uint8_t> outputBuffer(ie.GetOutputSize());

// 자동 분할되어 처리됨 (사용자 출력 버퍼)
auto outputs = ie.Run(concatenatedInput.data(), userArg, outputBuffer.data());
```

##### Python
```python
# 모든 입력을 연결한 단일 배열
concatenated_input = np.zeros(ie.get_input_size(), dtype=np.uint8)
# ... 데이터 채우기 ...

# 출력 버퍼 생성
output_buffers = [np.zeros(size, dtype=np.uint8) for size in ie.get_output_tensor_sizes()]

# 자동 분할되어 처리됨 (사용자 출력 버퍼)
outputs = ie.run(concatenated_input, output_buffers=output_buffers)
```

## Multi-Input Batch 추론

### Explicit Batch Format

각 배치 아이템에 대해 명시적으로 입력 텐서들을 제공하는 방식입니다.

#### Python
```python
# List[List[np.ndarray]] 형태
batch_inputs = [
    [sample1_input1, sample1_input2],  # 첫 번째 샘플
    [sample2_input1, sample2_input2],  # 두 번째 샘플
    [sample3_input1, sample3_input2]   # 세 번째 샘플
]

batch_outputs = [
    [sample1_output1, sample1_output2],  # 첫 번째 샘플 출력 버퍼
    [sample2_output1, sample2_output2],  # 두 번째 샘플 출력 버퍼
    [sample3_output1, sample3_output2]   # 세 번째 샘플 출력 버퍼
]

# 배치 추론
results = ie.run(batch_inputs, output_buffers=batch_outputs)
```

#### C++
```cpp
// 배치 입력 버퍼들 (연결된 형태)
std::vector<void*> batchInputs = {sample1_ptr, sample2_ptr, sample3_ptr};
std::vector<void*> batchOutputs = {output1_ptr, output2_ptr, output3_ptr};
std::vector<void*> userArgs = {userArg1, userArg2, userArg3};

// 배치 추론
auto results = ie.Run(batchInputs, batchOutputs, userArgs);
```

### Flattened Batch Format

모든 입력을 플래튼된 형태로 제공하는 방식입니다.

#### Python
```python
# 플래튼된 형태: [sample1_input1, sample1_input2, sample2_input1, sample2_input2, ...]
flattened_inputs = [
    sample1_input1, sample1_input2,  # 첫 번째 샘플
    sample2_input1, sample2_input2,  # 두 번째 샘플
    sample3_input1, sample3_input2   # 세 번째 샘플
]

# 자동으로 배치로 인식됨 (입력 개수가 모델 입력 개수의 배수)
results = ie.run(flattened_inputs, output_buffers=batch_outputs)
```

## 비동기 추론

### 1. 콜백 기반 비동기 추론

#### C++
```cpp
// 콜백 함수 등록
ie.RegisterCallback([](dxrt::TensorPtrs& outputs, void* userArg) -> int {
    // 출력 처리
    return 0;
});

// Dictionary format 비동기 추론
int jobId = ie.RunAsyncMultiInput(inputTensors, userArg);

// Vector format 비동기 추론
int jobId = ie.RunAsyncMultiInput(inputPtrs, userArg);
```

#### Python
```python
# 콜백 함수 정의
def callback_handler(outputs, user_arg):
    # 출력 처리 및 검증
    return 0

# 콜백 등록
ie.register_callback(callback_handler)

# Dictionary format 비동기 추론
job_id = ie.run_async_multi_input(input_tensors, user_arg=user_arg)

# Vector format 비동기 추론
job_id = ie.run_async(input_list, user_arg=user_arg)
```

### 2. 간단한 비동기 추론

#### C++
```cpp
// 단일 버퍼 비동기 추론
int jobId = ie.RunAsync(inputPtr, userArg);

// 결과 대기
auto outputs = ie.Wait(jobId);
```

#### Python
```python
# 단일 버퍼 비동기 추론
job_id = ie.run_async(input_buffer, user_arg=user_arg)

# 결과 대기
outputs = ie.wait(job_id)
```

## 장치 검증

Multi-input 모델에 대한 NPU 장치 검증도 지원됩니다.

### C++
```cpp
// Dictionary format
std::map<std::string, void*> inputTensors;
inputTensors["input1"] = input1_data;
inputTensors["input2"] = input2_data;

auto validationResults = ie.ValidateDeviceMultiInput(inputTensors, deviceId);

// Vector format
std::vector<void*> inputPtrs = {input1_data, input2_data};
auto validationResults = ie.ValidateDevice(inputPtrs, deviceId);
```

### Python
```python
# Dictionary format
input_tensors = {"input1": input1_array, "input2": input2_array}
validation_results = ie.validate_device_multi_input(input_tensors, device_id=0)

# Vector format  
input_list = [input1_array, input2_array]
validation_results = ie.validate_device(input_list, device_id=0)
```

## 출력 검증

강화된 출력 검증 기능이 포함되어 있습니다:

### 검증 항목
1. **출력 존재 여부**: None 또는 빈 리스트 검사
2. **데이터 타입**: numpy.ndarray 타입 검증
3. **텐서 크기**: 빈 텐서 (size=0) 검사
4. **형태 유효성**: 유효하지 않은 shape 검사
5. **수치 유효성**: NaN, Inf 값 검사 (Python)
6. **포인터 유효성**: null 포인터 검사 (C++)
7. **배치 구조**: 배치 출력의 올바른 중첩 구조 검증

### 검증 예제 (Python)
```python
def validate_outputs(outputs, expected_count, test_name):
    # 1. 기본 존재 여부 검사
    if outputs is None or not isinstance(outputs, list):
        return False
    
    # 2. 배치/단일 출력 구분
    is_batch = isinstance(outputs[0], list)
    
    # 3. 각 텐서별 상세 검증
    for output in outputs:
        if not isinstance(output, np.ndarray):
            return False
        if output.size == 0:  # 빈 텐서
            return False
        if len(output.shape) == 0:  # 잘못된 형태
            return False
        if np.any(np.isnan(output)) or np.any(np.isinf(output)):  # 수치 오류
            return False
    
    return True
```

## 테스트 시나리오

현재 예제 코드는 다음 10가지 시나리오를 테스트합니다:

### 단일 추론 (No Output Buffer)
1. **Dictionary Format (No Buffer)**: 딕셔너리 형태, 자동 할당
2. **Vector Format (No Buffer)**: 벡터 형태, 자동 할당
3. **Auto-Split (No Buffer)**: 자동 분할, 자동 할당

### 단일 추론 (With Output Buffer)
4. **Dictionary Format (With Buffer)**: 딕셔너리 형태, 사용자 버퍼
5. **Vector Format (With Buffer)**: 벡터 형태, 사용자 버퍼
6. **Auto-Split (With Buffer)**: 자동 분할, 사용자 버퍼

### 배치 추론
7. **Batch Explicit**: 명시적 배치 형태
8. **Batch Flattened**: 플래튼된 배치 형태 (Python만)

### 비동기 추론
9. **Async Callback**: 콜백 기반 비동기
10. **Simple Async**: 간단한 비동기

## 권장사항

1. **Dictionary Format 사용**: 가장 명확하고 실수가 적음
2. **입력 텐서 정보 확인**: `GetInputTensorNames()` 등을 사용하여 모델 요구사항 확인
3. **데이터 연속성**: Python에서는 C-contiguous 배열 사용 권장
4. **메모리 관리**: 출력 버퍼를 미리 할당하여 성능 향상
5. **에러 처리**: 잘못된 입력 형태나 크기에 대한 예외 처리
6. **출력 검증**: 강화된 출력 검증으로 False Positive 방지

## 제약사항

1. **비동기 추론**: 배치 추론은 지원하지 않음 (단일 추론만)
2. **입력 순서**: Vector format 사용 시 `GetInputTensorNames()` 순서 준수 필요
3. **데이터 타입**: 모델에서 요구하는 데이터 타입과 일치해야 함
4. **버퍼 크기**: 입력/출력 버퍼 크기가 모델 요구사항과 일치해야 함
5. **장치 검증**: Debug 모드로 컴파일된 모델에서만 지원 