#!/bin/bash
# Check: [BUILD] USE_ORT=OFF (@aarch64)
TARGET_SOURCE_DIR=$1

if [ $(uname -m) == "aarch64" ] ; then 
	exit 0
fi

. /etc/os-release
if [[ "$ID" == "ubuntu" ]]; then
    if [[ "$VERSION_ID" == "18.04" ]]; then
        echo "Ignore ARM Arch 64 Build Test"
        echo "PASS"
        exit 0
    else
        echo "$ID $VERSION_ID"
    fi
else
    echo "This is not Ubuntu"
fi

# Color code
NC='\033[0m' # No Color
RED='\033[0;31m'


cd $TARGET_SOURCE_DIR

CONFIG_FILE=cmake/dxrt.cfg.cmake
cp $CONFIG_FILE $CONFIG_FILE.origin 

# Use sed to find and replace 'USE_ORT' line containing 'ON' with 'OFF'
sed -i 's/\(USE_ORT.*\)ON/\1OFF/' "$CONFIG_FILE"


./build.sh --clean --arch aarch64

exit_code=$?

mv $CONFIG_FILE.origin $CONFIG_FILE

if [ $exit_code -eq 0 ]; then
    echo "PASS"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi

