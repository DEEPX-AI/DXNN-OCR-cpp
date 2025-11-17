#!/bin/bash
# Check: [EXECUTABLE] parse_model -m (model load)
DEFAULT_EXECUTABLE=$PWD
TARGET_SOURCE_DIR=${1:-$DEFAULT_EXECUTABLE}
TARGET_MODEL_DIR=${2:-$DEFAULT_EXECUTABLE}

# Color code
NC='\033[0m' # No Color
RED='\033[0;31m'

MODEL_PATH=$(realpath $TARGET_MODEL_DIR'YOLOV7-4.dxnn')

cd $TARGET_SOURCE_DIR

RESULT=$(./bin/parse_model -m "$MODEL_PATH" | tr -d '\0') 
echo $RESULT

if echo $RESULT | grep -q 'DXNN Model Ver.'; then
    echo "PASS"
else
    echo -e "${RED}FAIL${NC}"
fi
