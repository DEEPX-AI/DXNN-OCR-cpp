#!/bin/bash
# Check: [BUILD] USE_ORT=ON (@aarch64)
TARGET_SOURCE_DIR=$1

if [ $(uname -m) == "aarch64" ]; then
	exit 0
fi

. /etc/os-release
if [[ "$ID" == "ubuntu" ]]; then
    if [[ "$VERSION_ID" == "18.04" ]]; then
        echo "$ID $VERSION_ID"
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

# Use sed to find and replace 'USE_ORT' line containing 'OFF' with 'ON'
sed -i 's/\(USE_ORT.*\)OFF/\1ON/' "$CONFIG_FILE"



CONFIG_FILE2=cmake/toolchain.aarch64.cmake
cp $CONFIG_FILE2 $CONFIG_FILE2.origin 


DXRT_DIR=$(realpath $TARGET_SOURCE_DIR | sed 's/\//\\\//g')

sed -i "s/\(.*onnxruntime_LIB_DIRS.*\)\/usr\/local/\1${DXRT_DIR}\/util\/onnxruntime_aarch64/" "$CONFIG_FILE2"

./build.sh --clean --arch aarch64

exit_code=$?

mv $CONFIG_FILE.origin $CONFIG_FILE
mv $CONFIG_FILE2.origin $CONFIG_FILE2

if [ $exit_code -eq 0 ]; then
    echo "PASS"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi
