# Dynamic Shape Output 사용 가이드

## 개요

DX-RT는 동적 형태(dynamic shape) 출력을 가진 모델을 지원합니다. 동적 형태 모델은 입력에 따라 출력 텐서의 크기가 런타임에 결정되는 모델입니다.

## 동적 형태 모델 감지

모델이 동적 형태 출력을 가지는지 확인하는 방법:

```cpp
#include "dxrt/dxrt_api.h"

dxrt::InferenceEngine ie("model.dxnn");

// 모델의 동적 형태 여부 확인
if (ie.HasDynamicOutput()) {
    std::cout << "This model has dynamic shape outputs" << std::endl;
}
```

## API 사용법

### 1. GetOutputSize() vs GetOutputTensorSizes()

#### GetOutputSize() - 전체 출력 크기

- **정적 형태 모델**: 모든 출력 텐서의 총 크기를 바이트 단위로 반환
- **동적 형태 모델**: -1을 반환하고 경고 메시지 출력

```cpp
uint64_t totalSize = ie.GetOutputSize();
if (totalSize == static_cast<uint64_t>(-1)) {
    // 동적 형태 모델 - 메모리 할당에 사용하지 마세요!
    std::cout << "Dynamic shape model detected" << std::endl;
} else {
    // 정적 형태 모델 - 안전하게 메모리 할당 가능
    std::vector<uint8_t> outputBuffer(totalSize);
}
```

#### GetOutputTensorSizes() - 개별 텐서 크기

개별 출력 텐서의 크기를 확인할 때 사용:

```cpp
auto tensorSizes = ie.GetOutputTensorSizes();
for (size_t i = 0; i < tensorSizes.size(); ++i) {
    std::cout << "Tensor " << i << " size: " << tensorSizes[i] << " bytes" << std::endl;
}
```

### 2. 동적 형태 모델에서의 메모리 관리

동적 형태 모델에서는 사전에 출력 버퍼를 할당할 수 없으므로, 엔진이 관리하는 버퍼를 사용해야 합니다:

```cpp
// 동적 형태 모델의 올바른 사용법
if (ie.HasDynamicOutput()) {
    // nullptr를 전달하여 엔진이 버퍼를 관리하도록 함
    auto outputs = ie.Run(inputBuffers, nullptr);
    
    // 런타임에 실제 형태 확인
    for (const auto& tensorPtr : outputs) {
        std::cout << "Tensor name: " << tensorPtr->name() << std::endl;
        std::cout << "Actual shape: ";
        for (int64_t dim : tensorPtr->shape()) {
            std::cout << dim << " ";
        }
        std::cout << std::endl;
    }
}
```

### 3. 배치 처리에서의 동적 형태

배치 처리에서도 동일한 원칙이 적용됩니다:

```cpp
std::vector<void*> inputBuffers = /* 입력 버퍼들 */;
std::vector<void*> outputBuffers;

if (ie.HasDynamicOutput()) {
    // 동적 형태 모델: nullptr 버퍼 사용
    outputBuffers.resize(batch_count, nullptr);
} else {
    // 정적 형태 모델: 사전 할당
    uint64_t outputSize = ie.GetOutputSize();
    outputBuffers.resize(batch_count);
    for (auto& ptr : outputBuffers) {
        ptr = new uint8_t[outputSize];
    }
}

auto results = ie.Run(inputBuffers, outputBuffers);

// 정적 형태 모델에서만 메모리 해제
if (!ie.HasDynamicOutput()) {
    for (auto& ptr : outputBuffers) {
        delete[] static_cast<uint8_t*>(ptr);
    }
}
```

## Python API

Python에서도 동일한 개념이 적용됩니다:

```python
from dx_engine import InferenceEngine

ie = InferenceEngine("model.dxnn")

# 모델의 동적 형태 여부 확인
if ie.has_dynamic_output():
    print("Dynamic shape model detected")
    
    # 동적 모델에서는 get_output_size()가 -1을 반환
    total_size = ie.get_output_size()
    if total_size == -1:
        print("Dynamic model - use individual tensor sizes")
        tensor_sizes = ie.get_output_tensor_sizes()
        print(f"Individual tensor sizes: {tensor_sizes}")
else:
    print("Static shape model")
    total_size = ie.get_output_size()
    print(f"Total output size: {total_size} bytes")
```

## 주의사항

1. **메모리 할당 금지**: 동적 형태 모델에서 `GetOutputSize()` 반환값을 메모리 할당에 사용하지 마세요.

2. **버퍼 관리**: 동적 형태 모델에서는 엔진이 메모리를 관리하도록 하세요 (출력 버퍼에 nullptr 전달).

3. **런타임 형태 확인**: 동적 형태 모델의 실제 출력 형태는 추론 후에만 알 수 있습니다.

4. **성능 고려사항**: 동적 형태 모델은 런타임 메모리 할당으로 인해 약간의 성능 오버헤드가 있을 수 있습니다.

## 트러블슈팅

### 문제: GetOutputSize()가 -1을 반환합니다

**해결책**: 모델이 동적 형태 출력을 가집니다. `HasDynamicOutputs()`으로 확인하고 적절한 처리를 하세요.

### 문제: 메모리 할당 시 오류가 발생합니다

**해결책**: 동적 형태 모델에서 `GetOutputSize()` 반환값으로 메모리를 할당하려고 했을 가능성이 있습니다. nullptr 버퍼를 사용하세요.

### 문제: 출력 텐서 크기가 예상과 다릅니다

**해결책**: 동적 형태 모델의 출력 크기는 입력에 따라 달라집니다. 추론 후 실제 형태를 확인하세요.

## 성능 최적화 팁

1. **배치 처리**: 동적 형태 모델에서도 배치 처리가 가능하며, 메모리 효율성을 위해 권장됩니다.

2. **메모리 재사용**: 연속된 추론에서 유사한 출력 크기가 예상되는 경우, 엔진의 내부 메모리 풀이 재사용을 최적화합니다.

3. **정적 추정**: 개발 단계에서 `get_static_size_estimate()` 메서드를 사용하여 메모리 사용량을 추정할 수 있습니다.

## 예제 코드

완전한 예제는 다음 파일들을 참조하세요:

- `examples/cpp/run_batch_model/run_batch_model.cpp` (동적 형태 + 배치 처리)
- `examples/python/run_batch_model.py`
- `examples/cpp/multi_input_model_inference/multi_input_model_inference.cpp`
