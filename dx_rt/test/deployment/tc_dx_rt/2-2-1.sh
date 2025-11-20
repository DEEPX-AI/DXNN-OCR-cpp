#!/bin/bash
# Check: [INSTALL] ./install.sh --onnxruntime

TARGET_SOURCE_DIR=$1

# Color code
NC='\033[0m' # No Color
RED='\033[0;31m'

cd $TARGET_SOURCE_DIR

./install.sh --onnxruntime

exit_code=$?
if [ $exit_code -eq 0 ]; then
    echo "PASS"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi
