#!/bin/bash
# Check: [EXECUTABLE] dxrt-cli -c (update firmware settings)

# Color code
NC='\033[0m' # No Color
RED='\033[0;31m'
DEFAULT_EXECUTABLE=$PWD
TARGET_SOURCE_DIR=${1:-$DEFAULT_EXECUTABLE}

option="-c"
COMMAND="./bin/dxrt-cli"
FW_Voltage="750" #default
FW_Freq="1000" #default
EXPECTED_MSG="voltage $FW_Voltage mV, clock $FW_Freq MHz"

cd $TARGET_SOURCE_DIR

# -c <npu0-voltage> -c <npu0-freq> -c <npu1-voltage> -c <npu1-freq> -c <npu2-voltage> -c <npu2-freq>
# -C <json-filename>
RESULT=$($COMMAND $option $FW_Voltage $option $FW_Freq $option $FW_Voltage $option $FW_Freq $option $FW_Voltage $option $FW_Freq)
exit_code=$?

if [ $exit_code -eq 1 ]; then
    echo -e "${RED}FAIL${NC}"
    exit 1
fi

RESULT=$(./bin/dxrt-cli -s)
echo $RESULT

if echo $RESULT |  grep -q "$EXPECTED_MSG" ; then
	echo "PASS"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi
