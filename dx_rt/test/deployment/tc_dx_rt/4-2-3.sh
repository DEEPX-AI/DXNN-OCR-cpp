#!/bin/bash
# Check: [EXECUTABLE] run_model -m -i -o (model & input & output)
DEFAULT_EXECUTABLE=$PWD
TARGET_SOURCE_DIR=${1:-$DEFAULT_EXECUTABLE}
TARGET_MODEL_DIR=${2:-$DEFAULT_EXECUTABLE}
TARGET_MODEL_NAME=${3:-""}

# Color code
NC='\033[0m' # No Color
RED='\033[0;31m'

MODEL_PATH=$(realpath $TARGET_MODEL_DIR$TARGET_MODEL_NAME)
INPUT_PATH=$(realpath $TARGET_MODEL_DIR'gt/npu_0_input_0.bin')

cd $TARGET_SOURCE_DIR

OUTPUT_FILE='YOLOv7_output_0.bin'

./bin/run_model -m $MODEL_PATH -i $INPUT_PATH -o $OUTPUT_FILE
exit_code=$?

if [ $exit_code -eq 0 ]; then
	echo "PASS"
else
	echo -e "${RED}FAIL${NC}"
	exit 1
fi

if find ./ | grep -q $OUTPUT_FILE ; then
	rm $OUTPUT_FILE
else
	echo -e "${RED}FAIL${NC}"
	exit 1
fi
