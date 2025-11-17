#!/bin/bash
# Check: [EXECUTABLE] dxrt-cli -d -l (device id & firmware log)

# Color code
NC='\033[0m' # No Color
RED='\033[0;31m'
DEFAULT_EXECUTABLE=$PWD
TARGET_SOURCE_DIR=${1:-$DEFAULT_EXECUTABLE}

option="-d 0 -l fwlog"
COMMAND="./bin/dxrt-cli"
#EXPECTED_MSG="[0-9]* req [0-9]* -> npu[0-2], type 0, input offset [0-9]*, output offset [0-9]*"

cd $TARGET_SOURCE_DIR

RESULT=$($COMMAND $option)
exit_code=$?

echo $RESULT

if [ -n "$RESULT" ]; then # Not NULL
	echo "PASS"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi
