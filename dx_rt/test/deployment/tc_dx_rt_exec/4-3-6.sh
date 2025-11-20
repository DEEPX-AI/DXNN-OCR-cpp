#!/bin/bash
# Check: [EXECUTABLE] dxrt-cli -p (dump device internal file)

# Color code
NC='\033[0m' # No Color
RED='\033[0;31m'
DEFAULT_EXECUTABLE=$PWD
TARGET_SOURCE_DIR=${1:-$DEFAULT_EXECUTABLE}

option="-p"
COMMAND="./bin/dxrt-cli"
EXPECTED_MSG="Device 0 dump to file dump"
DUMP_FILE="NPU_0_DUMP"


cd $TARGET_SOURCE_DIR


RESULT=$($COMMAND -d 0 $option $DUMP_FILE)
exit_code=$?

echo $RESULT

if ! [ $exit_code -eq 0 ]; then
    echo -e "${RED}FAIL${NC}"
    exit 1
fi

if [ -e $DUMP_FILE ]; then
    echo "Dump file:" $DUMP_FILE
	echo "PASS"
	rm $DUMP_FILE*
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi
