Of course\! Here is a detailed explanation of the multi-input examples in Markdown (`.md`) format, based on the C++ code you provided.

-----

# DXRT Multi-Input Inference Examples

This document explains various methods for performing inference on multi-input models using the `dxrt::InferenceEngine`. The examples cover different input formats, synchronous and asynchronous execution, and batch processing.

## 1\. Model Information

Before running inference, it's useful to inspect the model's properties. The `printModelInfo` function shows how to query the inference engine for details about the model's input and output tensors.

  - **`ie.IsMultiInputModel()`**: Checks if the loaded model has multiple inputs.
  - **`ie.GetInputTensorCount()`**: Gets the number of input tensors.
  - **`ie.GetInputTensorNames()`**: Retrieves the names of all input tensors.
  - **`ie.GetInputTensorSizes()`**: Gets the size (in bytes) of each input tensor.
  - **`ie.GetOutputTensorNames()` / `ie.GetOutputTensorSizes()`**: Provide similar information for output tensors.

<!-- end list -->

```cpp
void printModelInfo(dxrt::InferenceEngine& ie) {
    if (ie.IsMultiInputModel()) {
        std::cout << "Input tensor count: " << ie.GetInputTensorCount() << std::endl;
        auto inputNames = ie.GetInputTensorNames();
        auto inputSizes = ie.GetInputTensorSizes();
        for (size_t i = 0; i < inputNames.size(); ++i) {
            std::cout << "  " << inputNames[i] << ": " << inputSizes[i] << " bytes" << std::endl;
        }
    }
}
```

-----

## 2\. Synchronous Single Inference

These examples demonstrate different ways to run a single inference request synchronously.

### Input Formats

#### A. Dictionary Format (`std::map<std::string, void*>`)

This is the most robust method. You provide a map where keys are the tensor names and values are pointers to the input data. This format is not sensitive to the order of tensors.

  - **API**: `ie.RunMultiInput(inputTensors)`
  - **Use Case**: Recommended for clarity and to avoid errors from tensor reordering.

<!-- end list -->

```cpp
// Create input data
std::map<std::string, void*> inputTensors;
inputTensors["input_1"] = inputData1.data();
inputTensors["input_2"] = inputData2.data();

// Run inference
auto outputs = ie.RunMultiInput(inputTensors);
```

#### B. Vector Format (`std::vector<void*>`)

You provide a vector of pointers to the input data. The order of pointers in the vector **must** match the order returned by `ie.GetInputTensorNames()`.

  - **API**: `ie.RunMultiInput(inputPtrs)`
  - **Use Case**: When tensor order is known and fixed. Can be slightly more performant than the map-based approach due to less overhead.

<!-- end list -->

```cpp
// Create input data in the correct order
std::vector<void*> inputPtrs;
inputPtrs.push_back(inputData1.data()); // Corresponds to first name in GetInputTensorNames()
inputPtrs.push_back(inputData2.data()); // Corresponds to second name

// Run inference
auto outputs = ie.RunMultiInput(inputPtrs);
```

#### C. Auto-Split Concatenated Buffer

You provide a single, contiguous buffer containing all input data concatenated together. The engine automatically splits this buffer into the correct tensor inputs based on their sizes. The concatenation order **must** match the order from `ie.GetInputTensorNames()`.

  - **API**: `ie.Run(concatenatedInput.data())`
  - **Use Case**: Efficient when input data is already in a single block or when interfacing with systems that provide data this way.

<!-- end list -->

```cpp
// Create a single buffer with all input data concatenated
auto concatenatedInput = createDummyInput(ie.GetInputSize());

// Run inference
auto outputs = ie.Run(concatenatedInput.data());
```

### Output Buffer Management

For each synchronous method, you can either let the engine allocate output memory automatically or provide a pre-allocated buffer for performance gains.

  - **Auto-Allocated Output (No Buffer Provided)**: Simpler to use. The engine returns smart pointers to newly allocated memory.

    ```cpp
    // Engine allocates and manages output memory
    auto outputs = ie.RunMultiInput(inputTensors);
    ```

  - **User-Provided Output Buffer**: More performant as it avoids repeated memory allocations. The user is responsible for allocating a buffer of size `ie.GetOutputSize()`.

    ```cpp
    // User allocates the output buffer
    std::vector<uint8_t> outputBuffer(ie.GetOutputSize());

    // Run inference, placing results in the provided buffer
    auto outputs = ie.RunMultiInput(inputTensors, nullptr, outputBuffer.data());
    ```

-----

## 3\. Synchronous Batch Inference

For processing multiple inputs at once to maximize throughput, you can use the batch inference API. This is more efficient than running single inferences in a loop.

  - **API**: `ie.Run(batchInputPtrs, batchOutputPtrs, userArgs)`
  - **Input**: A vector of pointers, where each pointer is a concatenated buffer for one sample in the batch.
  - **Output**: A vector of pointers, where each pointer is a pre-allocated buffer for the corresponding sample's output.

<!-- end list -->

```cpp
int batchSize = 3;
std::vector<void*> batchInputPtrs;
std::vector<void*> batchOutputPtrs;

// Prepare input and output buffers for each sample in the batch
for (int i = 0; i < batchSize; ++i) {
    // Each input is a full concatenated buffer
    batchInputData[i] = createDummyInput(ie.GetInputSize());
    batchInputPtrs.push_back(batchInputData[i].data());

    // Pre-allocate output buffer for each sample
    batchOutputData[i].resize(ie.GetOutputSize());
    batchOutputPtrs.push_back(batchOutputData[i].data());
}

// Run batch inference
auto batchOutputs = ie.Run(batchInputPtrs, batchOutputPtrs);
```

-----

## 4\. Asynchronous Inference

Asynchronous APIs allow you to submit inference requests without blocking the calling thread. The results are returned later via a callback function. This is ideal for applications that need to remain responsive, such as those with a user interface.

  - **APIs**:
      - `ie.RunAsyncMultiInput(inputTensors, userArg)`
      - `ie.RunAsync(concatenatedInput.data(), userArg)`
  - **Callback Registration**: `ie.RegisterCallback(callback_function)`

The `AsyncInferenceHandler` class demonstrates how to manage state across multiple asynchronous calls.

1.  **Register a Callback**: Provide a function that the engine will call upon completion of each async request. The callback receives the output tensors and a `userArg` pointer for context.
2.  **Submit Requests**: Call an `RunAsync` variant. This call returns immediately with a job ID.
3.  **Process in Callback**: The callback function is executed in a separate worker thread. Here, you can process the results. It's crucial to ensure thread safety if you modify shared data.

<!-- end list -->

```cpp
// 1. Create a handler and register its callback method
AsyncInferenceHandler handler(asyncCount);
ie.RegisterCallback([&handler](dxrt::TensorPtrs& outputs, void* userArg) -> int {
    return handler.callback(outputs, userArg);
});

// 2. Submit multiple async requests in a loop
for (int i = 0; i < asyncCount; ++i) {
    void* userArg = reinterpret_cast<void*>(static_cast<uintptr_t>(i));
    // Each call is non-blocking
    ie.RunAsyncMultiInput(asyncInputTensors[i], userArg);
}

// 3. Wait for all callbacks to complete
handler.waitForCompletion();

// 4. Clear the callback when done
ie.RegisterCallback(nullptr);
```