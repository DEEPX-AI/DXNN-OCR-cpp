#!/bin/bash
# Check: [EXECUTABLE] run_model -m -i (mode & input)
DEFAULT_EXECUTABLE=$PWD
TARGET_SOURCE_DIR=${1:-$DEFAULT_EXECUTABLE}
TARGET_MODEL_DIR=${2:-$DEFAULT_EXECUTABLE}

# Color code
NC='\033[0m' # No Color
RED='\033[0;31m'

MODEL_PATH=$(realpath $TARGET_MODEL_DIR'YOLOV7-4.dxnn')
INPUT_PATH=$(realpath $TARGET_MODEL_DIR'gt/npu_0_input_0.bin')

cd $TARGET_SOURCE_DIR


./bin/run_model -m $MODEL_PATH -i $INPUT_PATH 
exit_code=$?

rm output.bin
if [ $exit_code -eq 0 ]; then
    echo "PASS"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi
