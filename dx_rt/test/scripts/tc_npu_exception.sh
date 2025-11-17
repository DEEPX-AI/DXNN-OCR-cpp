#!/bin/bash
# Description: Model testing script with recovery logic

MODEL_PATH='/mnt/regression_storage/ci-data/bitmatch_data_set/M1A/YOLOv7_512-YOLOV7-4/YOLOv7_512.dxnn'
NPU_ERR_MODEL_PATH='/mnt/regression_storage/ci-data/bitmatch_data_set/V3/MobileNetV2_9:MobileNetV2-10/MobileNetV2_9.dxnn'

# Function to reset the device
reset_device() {
    echo "Resetting device..."
    dxrt-cli -r 0
    sleep 0.1
}

# Function to run a model with optional loop count and background option
run_model_with_recovery() {
    local model_path=$1
    local loop_count=$2
    local background=$3

    if [[ -z $loop_count ]]; then
        run_model -m "$model_path"
    else
        run_model -m "$model_path" -l "$loop_count" &
    fi
}

# Single-process recovery concept
echo " ** Running single-process recovery test..."
run_model_with_recovery "$NPU_ERR_MODEL_PATH"
reset_device
run_model_with_recovery "$MODEL_PATH"

# Multi-process recovery concept
echo " ** Running multi-process recovery test..."
run_model_with_recovery "$MODEL_PATH" 10000 1
sleep 2
run_model_with_recovery "$NPU_ERR_MODEL_PATH"
reset_device
run_model_with_recovery "$MODEL_PATH"

echo "Terminate shell script"
