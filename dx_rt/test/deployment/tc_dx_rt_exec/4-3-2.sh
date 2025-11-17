#!/bin/bash
# Check: [EXECUTABLE] dxrt-cli -i (device info)

# Color code
NC='\033[0m' # No Color
RED='\033[0;31m'
DEFAULT_EXECUTABLE=$PWD
TARGET_SOURCE_DIR=${1:-$DEFAULT_EXECUTABLE}

option="i"
COMMAND="./bin/dxrt-cli"
EXPECTED_MSG="FW v[0-9].[0-9].[0-9]"

cd $TARGET_SOURCE_DIR

RESULT=$($COMMAND -$option)
echo $RESULT

if echo $RESULT |  grep -q "$EXPECTED_MSG" ; then
	echo "PASS"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi
