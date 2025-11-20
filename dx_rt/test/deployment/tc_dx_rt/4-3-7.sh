#!/bin/bash
# Check: [EXECUTABLE] dxrt-cli -u (update firmware)

# Color code
NC='\033[0m' # No Color
RED='\033[0;31m'
DEFAULT_EXECUTABLE=$PWD
TARGET_SOURCE_DIR=${1:-$DEFAULT_EXECUTABLE}

option="-u"
COMMAND="./bin/dxrt-cli"
#EXPECTED_MSG="Device 0 update firmware\[[0-9]\+\.[0-9]\+\.[0-9]\+\] by $FW_PATH, SubCmd:[0-9] : SUCCES"
EXPECTED_MSG="Device 0 update firmware.*, SubCmd:[0-9] : SUCCES"


if  echo $PWD | grep -qE 'tc_dx_rt' ; then
	FW_PATH=$(realpath ./fw.bin)
else
	FW_PATH=$(realpath ./tc_dx_rt/fw.bin)
fi

cd $TARGET_SOURCE_DIR

if  $COMMAND -s | grep -q "SOM" ; then
	echo "This is not M.2 module";	
	exit 0;
fi

RESULT=$($COMMAND $option $FW_PATH -d 0)
exit_code=$?

echo $RESULT

if  echo $RESULT | grep -q "$EXPECTED_MSG"; then
	echo "PASS"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi
