#!/bin/bash
# Check: [EXECUTABLE] dxrt-cli -r 0 (device:0 reset)

# Color code
NC='\033[0m' # No Color
RED='\033[0;31m'
DEFAULT_EXECUTABLE=$PWD
TARGET_SOURCE_DIR=${1:-$DEFAULT_EXECUTABLE}

option="-r 0" # reset NPU only
COMMAND="./bin/dxrt-cli"
EXPECTED_MSG="Device [0-2] reset by option [0-1]"

cd $TARGET_SOURCE_DIR

RESULT=$($COMMAND $option)
echo $RESULT

if echo $RESULT |  grep -q "$EXPECTED_MSG" ; then
	echo "PASS"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi
