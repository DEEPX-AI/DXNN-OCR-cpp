#!/bin/bash
# Check: [BUILD] USE_ORT=OFF 

TARGET_SOURCE_DIR=$1

# Color code
NC='\033[0m' # No Color
RED='\033[0;31m'


cd $TARGET_SOURCE_DIR

CONFIG_FILE=cmake/dxrt.cfg.cmake
cp $CONFIG_FILE $CONFIG_FILE.origin 

# Use sed to find and replace 'USE_ORT' line containing 'ON' with 'OFF'
sed -i 's/\(USE_ORT.*\)ON/\1OFF/' "$CONFIG_FILE"


./build.sh --clean 

exit_code=$?

mv $CONFIG_FILE.origin $CONFIG_FILE

if [ $exit_code -eq 0 ]; then
    echo "PASS"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi

