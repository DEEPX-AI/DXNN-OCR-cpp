#!/bin/bash
# Check: [BUILD] ./build --install (install user path)
TEMP_DIR=$PWD/TEMP
TARGET_SOURCE_DIR=$1

# Color code
NC='\033[0m' # No Color
RED='\033[0;31m'

cd $TARGET_SOURCE_DIR

./build.sh --install $TEMP_DIR
exit_code=$?

sudo rm -rf $TEMP_DIR

if [ $exit_code -eq 0 ]; then
    echo "PASS"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi
