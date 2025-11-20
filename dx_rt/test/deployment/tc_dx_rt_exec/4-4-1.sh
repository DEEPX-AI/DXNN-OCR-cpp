#!/bin/bash
# Check: [EXECUTABLE] bitmatch model result(gt vs. rt) (python binding)
DEFAULT_EXECUTABLE=$PWD
TARGET_SOURCE_DIR=${1:-$DEFAULT_EXECUTABLE}
TARGET_MODEL_DIR=${2:-$DEFAULT_EXECUTABLE}

# Color code
NC='\033[0m' # No Color
RED='\033[0;31m'

MODEL_PATH=$(realpath $TARGET_MODEL_DIR'YOLOV7-4.dxnn')
RT_PATH=$(realpath './rt') # output data path (current folder)
PYTHON_BITMATCH_PATH=$(realpath $TARGET_SOURCE_DIR'python_package/examples/bitmatch_test.py')

cd $TARGET_SOURCE_DIR
cd python_package
pip install .  
if [ $? -eq 1 ]; then
    echo -e "${RED}FAIL${NC}"
    exit 1
fi

cd ..

PYTHON_PATH=$(which python3) # find python3 executable file path
COMMAND="$PYTHON_PATH $PYTHON_BITMATCH_PATH"
RESULT=$($COMMAND -m $MODEL_PATH --rt_dir $RT_PATH | tr -d '\0')
exit_code=$?

echo $RESULT
rm -rf $RT_PATH # delete RT folder & all files

if [ $exit_code -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi



