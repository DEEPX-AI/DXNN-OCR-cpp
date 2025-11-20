# DXRT Validation Test - Python Version

This is a Python port of the C++ DXRT validation test application.

## Overview

The validation test application performs comprehensive testing of DXRT inference engines with various configurations and execution modes.

## Files Structure

```
python/
├── validation_test.py      # Main application entry point
├── generator.py           # Test case generator
├── test_manager.py        # Test execution manager
├── executor_manager.py    # Thread and IE management
├── executor.py           # Different execution mode implementations
├── bitmatcher.py         # Output verification against ground truth
├── concurrent_queue.py   # Thread-safe queue implementation
├── utils.py              # Utility functions
└── README.md            # This file
```

## Usage

```bash
python3 validation_test.py -b <base_path> -j <json_file> --tmp-model <model_path> [options]
```

### Arguments

- `-b, --base-path`: Path of base directory for test files (required)
- `-j, --json-file`: Path of json file for test files (required)
- `--tmp-model`: Temporary model path for workaround (required)
- `-r, --result-file`: Path of result file (optional)
- `-v, --verbose`: Verbose level 0-4 (default: 0)
- `--random`: Randomize test case generation

### Example

```bash
python3 validation_test.py -b ~/models/ -j ~/jsons/partial_test.json --tmp-model ~/models/YoloXL-YOLOXL-1/YoloXL.dxnn -v 2 --random -r "test_results.json"
```

## Manual Replacements Needed

This Python port contains placeholder code that needs to be replaced with actual DXRT Python API calls. Search for the following patterns:

### 1. DXRT Import and Basic Usage

**Files**: All files  
**Search**: `# TODO: Import DXRT Python bindings`  
**Replace**: Import actual DXRT Python module

```python
# Replace this:
# import dxrt  # This needs to be replaced with actual DXRT Python API

# With actual import:
import dxrt_python  # or whatever the actual module name is
```

### 2. InferenceEngine Creation

**Files**: `validation_test.py`, `executor_manager.py`  
**Search**: `# TODO: Temporal Error Handling`  
**Replace**: Create InferenceEngine with actual API

```python
# Replace this:
# ie2 = dxrt.InferenceEngine(args.tmp_model)

# With actual API:
ie2 = dxrt_python.InferenceEngine(args.tmp_model)
```

### 3. Device Count Retrieval

**File**: `generator.py`  
**Search**: `# TODO: Replace with actual DXRT Python API call`  
**Replace**: Get device count

```python
# Replace this:
# num_devices = dxrt.get_device_count()
num_devices = 2  # Default assumption - manual replacement needed

# With actual API:
num_devices = dxrt_python.get_device_count()
```

### 4. Configuration Settings

**File**: `test_manager.py`  
**Search**: `# TODO: Configure dynamic CPU offloading`  
**Replace**: Configure DXRT settings

```python
# Replace placeholder with actual configuration API
config = dxrt_python.Configuration.get_instance()
config.set_enable(dxrt_python.Configuration.SHOW_PROFILE, False)
# etc.
```

### 5. Inference Operations

**File**: `executor.py`  
**Search**: `# TODO: Replace with actual DXRT Python API calls`  
**Replace**: Inference execution calls

```python
# Replace placeholders with actual inference API calls
outputs = ie.run_inference(input_buffer)
inference_id = ie.run_inference_async(input_buffer, user_arg)
# etc.
```

### 6. Tensor Operations

**File**: `bitmatcher.py`  
**Search**: `# TODO: Get tensor data from output`  
**Replace**: Extract tensor data

```python
# Replace this:
# tensor_data = self.outputs[i].get_data()
tensor_data = None  # Placeholder

# With actual API:
tensor_data = self.outputs[i].get_data()  # or appropriate method
```

### 7. Model Information

**File**: `utils.py`  
**Search**: `# print(f"Model Name: {ie.get_model_name()}")`  
**Replace**: Uncomment and verify actual API methods

### 8. Input Buffer Creation

**File**: `utils.py`  
**Search**: `# TODO: Replace with actual implementation`  
**Replace**: Create input buffers with actual API

### 9. Exception Handling

**Files**: Various  
**Search**: `# TODO: Add DXRT specific exception handling`  
**Replace**: Add actual DXRT exception types

```python
# Replace generic exceptions with DXRT-specific ones
except dxrt_python.DXRTException as e:
    print(f"DXRT Error: {e.message()}")
```

## Testing

1. Replace all TODO items with actual DXRT Python API calls
2. Test with a simple model first
3. Verify JSON output format matches expectations
4. Test with different verbose levels
5. Test random mode functionality

## Notes

- The structure and logic are preserved from the C++ version
- Thread safety is maintained using Python threading primitives
- JSON output format matches the C++ version
- Error handling patterns are consistent with the original

## Dependencies

- Python 3.6+
- DXRT Python bindings (to be installed separately)
- Standard library modules: json, threading, time, random, os, argparse

## Migration Status

- ✅ Core structure ported
- ✅ Class hierarchy maintained
- ✅ Thread safety implemented
- ⚠️ DXRT API calls need manual replacement
- ⚠️ Input/output buffer handling needs implementation
- ⚠️ Bitmatcher validation needs testing