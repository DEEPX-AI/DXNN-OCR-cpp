Of course\! Here is a guide for the Python examples, created with reference to the C++ guide and formatted according to your request.

-----

### 1\. Model Information

Before running inference, it's useful to inspect the model's properties. The `print_model_info` function in the example script shows how to query the inference engine for details about the model's input and output tensors.

  - **`ie.is_multi_input_model()`**: Checks if the loaded model has multiple inputs.
  - **`ie.get_input_tensor_count()`**: Gets the number of input tensors.
  - **`ie.get_input_tensor_names()`**: Retrieves the names of all input tensors.
  - **`ie.get_input_tensor_sizes()`**: Gets the size (in bytes) of each input tensor.
  - **`ie.get_output_tensor_names()` / `ie.get_output_tensor_sizes()`**: Provide similar information for output tensors.

<!-- end list -->

```python
def print_model_info(ie: InferenceEngine) -> None:
    if ie.is_multi_input_model():
        print(f"Input tensor count: {ie.get_input_tensor_count()}")
        input_names = ie.get_input_tensor_names()
        input_sizes = ie.get_input_tensor_sizes()
        for i, name in enumerate(input_names):
            print(f"  {name}: {input_sizes[i]} bytes")
```

-----

### 2\. Synchronous Single Inference

These examples demonstrate different ways to run a single inference request synchronously.

#### Input Formats

##### A. Dictionary Format (`Dict[str, np.ndarray]`)

This is the most robust method. You provide a dictionary where keys are the tensor names and values are the `numpy` arrays. This format is not sensitive to the order of tensors.

  - **API**: `ie.run_multi_input(input_tensors)`
  - **Use Case**: Recommended for clarity and to avoid errors from tensor reordering.

<!-- end list -->

```python
# Create input data
input_names = ie.get_input_tensor_names()
input_sizes = ie.get_input_tensor_sizes()
input_tensors = {name: create_dummy_input(size) for name, size in zip(input_names, input_sizes)}

# Run inference
outputs = ie.run_multi_input(input_tensors)
```

##### B. List Format (`List[np.ndarray]`)

You provide a list of `numpy` arrays. The order of arrays in the list **must** match the order returned by `ie.get_input_tensor_names()`.

  - **API**: `ie.run(input_list)`
  - **Use Case**: When tensor order is known and fixed. Can be slightly more performant than the dictionary-based approach due to less overhead.

<!-- end list -->

```python
# Create input data in the correct order
input_sizes = ie.get_input_tensor_sizes()
input_list = [create_dummy_input(size) for size in input_sizes]

# Run inference
outputs = ie.run(input_list)
```

##### C. Auto-Split Concatenated Buffer

You provide a single, contiguous `numpy` array containing all input data concatenated together. The engine automatically splits this buffer into the correct tensor inputs based on their sizes. The concatenation order **must** match the order from `ie.get_input_tensor_names()`.

  - **API**: `ie.run(concatenated_input)`
  - **Use Case**: Efficient when input data is already in a single block or when interfacing with systems that provide data this way.

<!-- end list -->

```python
# Create a single buffer with all input data concatenated
total_input_size = ie.get_input_size()
concatenated_input = create_dummy_input(total_input_size)

# Run inference
outputs = ie.run(concatenated_input)
```

#### Output Buffer Management

For each synchronous method, you can either let the engine allocate output memory automatically or provide pre-allocated buffers for performance gains.

  - **Auto-Allocated Output (No Buffer Provided)**: Simpler to use. The engine returns a new list of `numpy` arrays.

    ```python
    # Engine allocates and manages output memory
    outputs = ie.run_multi_input(input_tensors)
    ```

  - **User-Provided Output Buffers**: More performant as it avoids repeated memory allocations. The user is responsible for creating a list of `numpy` arrays with the correct sizes.

    ```python
    # User creates the output buffers
    output_sizes = ie.get_output_tensor_sizes()
    output_buffers = [np.zeros(size, dtype=np.uint8) for size in output_sizes]

    # Run inference, placing results in the provided buffers
    outputs = ie.run_multi_input(input_tensors, output_buffers=output_buffers)
    ```

-----

### 3\. Synchronous Batch Inference

For processing multiple inputs at once to maximize throughput, you can use the batch inference capabilities of the `run` method. This is more efficient than running single inferences in a loop.

##### A. Explicit Batch Format (`List[List[np.ndarray]]`)

This is the clearest way to represent a batch. The input is a list of lists, where the outer list represents the batch and each inner list contains all input tensors for a single sample.

  - **API**: `ie.run(batch_inputs, output_buffers=...)`
  - **Input**: A `List[List[np.ndarray]]`.
  - **Output**: A `List[List[np.ndarray]]`.

<!-- end list -->

```python
batch_size = 3
input_sizes = ie.get_input_tensor_sizes()
batch_inputs = []
for i in range(batch_size):
    sample_inputs = [create_dummy_input(size) for size in input_sizes]
    batch_inputs.append(sample_inputs)

# Output buffers must also match the batch structure
# ... create batch_outputs ...

# Run batch inference
results = ie.run(batch_inputs, output_buffers=batch_outputs)
```

##### B. Flattened Batch Format (`List[np.ndarray]`)

As a convenience, the API can also accept a single "flattened" list of `numpy` arrays. The total number of arrays must be a multiple of the model's input tensor count. The engine will automatically group them into batches.

  - **API**: `ie.run(flattened_inputs, output_buffers=...)`
  - **Input**: A `List[np.ndarray]` containing `batch_size * num_input_tensors` arrays.
  - **Output**: The result is still returned in the explicit batch format (`List[List[np.ndarray]]`).

<!-- end list -->

```python
batch_size = 3
input_sizes = ie.get_input_tensor_sizes()
flattened_inputs = []
for i in range(batch_size):
    for size in input_sizes:
        flattened_inputs.append(create_dummy_input(size))

# ... create flattened_output_buffers ...

# Run batch inference
results = ie.run(flattened_inputs, output_buffers=flattened_output_buffers)
```

-----

### 4\. Asynchronous Inference

Asynchronous APIs allow you to submit inference requests without blocking the calling thread. The results are returned later via a callback function. This is ideal for applications that need to remain responsive.

  - **APIs**:
      - `ie.run_async_multi_input(input_tensors, user_arg=...)`
      - `ie.run_async(input_data, user_arg=...)`
  - **Callback Registration**: `ie.register_callback(callback_function)`

The `AsyncInferenceHandler` class in the example demonstrates how to manage state across multiple asynchronous calls.

1.  **Register a Callback**: Provide a function that the engine will call upon completion of each async request. The callback receives the output arrays and a `user_arg` for context.
2.  **Submit Requests**: Call an `run_async` variant. This call returns immediately with a job ID.
3.  **Process in Callback**: The callback function is executed in a separate worker thread. Here, you can process the results. It's crucial to ensure thread safety (e.g., using a `threading.Lock`) if you modify shared data.

<!-- end list -->

```python
# 1. Create a handler and register its callback method
handler = AsyncInferenceHandler(async_count)
ie.register_callback(handler.callback)

# 2. Submit multiple async requests in a loop
for i in range(async_count):
    user_arg = f"async_sample_{i}"
    # Each call is non-blocking
    job_id = ie.run_async_multi_input(input_tensors, user_arg=user_arg)

# 3. Wait for all callbacks to complete
handler.wait_for_completion()

# 4. Clear the callback when done
ie.register_callback(None)
```