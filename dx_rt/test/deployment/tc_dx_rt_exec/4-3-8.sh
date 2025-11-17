#!/bin/bash
# Check: [EXECUTABLE] dxrt-cli -g (get firmware version)

# Color code
NC='\033[0m' # No Color
RED='\033[0;31m'
DEFAULT_EXECUTABLE=$PWD
TARGET_SOURCE_DIR=${1:-$DEFAULT_EXECUTABLE}

option="-g"
COMMAND="./bin/dxrt-cli"
EXPECTED_MSG="F[Ii]rmware Ver: [0-9]\+\.[0-9]\+\.[0-9]\+"


if  echo $PWD | grep -qE 'tc_dx_rt' ; then
	FW_DIR=$(realpath ./fw.bin)
else
	FW_DIR=$(realpath ./tc_dx_rt/fw.bin)
fi

cd $TARGET_SOURCE_DIR


RESULT=$($COMMAND $option $FW_DIR)
exit_code=$?

echo $RESULT

if  echo $RESULT | grep -q "$EXPECTED_MSG"; then
	echo "PASS"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi
