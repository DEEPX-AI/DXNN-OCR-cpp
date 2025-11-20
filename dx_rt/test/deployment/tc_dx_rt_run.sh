#!/bin/bash

# Define the directory containing the scripts
SCRIPT_DIR=tc_dx_rt

DEFAULT_TARGET_DIR='../dx_rt'
DEFAULT_MODEL_DIR='/mnt/regression_storage/ci-data/bitmatch_data_set/M1A/YOLOv7_512-YOLOV7-4/'
DEFAULT_MODEL_NAME='YOLOv7_512.dxnn'
TEST_TARGET_DIR=${1:-$DEFAULT_TARGET_DIR}
TEST_MODEL_DIR=${2:-$DEFAULT_MODEL_DIR}
TEST_MODEL_NAME=${3:-$DEFAULT_MODEL_NAME}

LOG_FILE="test_dx_rt_$(date +"%y%m%d_%H%M").log"

if [ -d "$TEST_MODEL_DIR" ]; then
  echo "Model Path:" $TEST_MODEL_DIR | tee -a $LOG_FILE
else
  echo "Model Path: $TEST_MODEL_DIR does not exist."
  exit 1
fi

echo "Start: " $(date +"%y%m%d_%H:%M:%S") | tee -a $LOG_FILE


# Check if the directory exists
if [ ! -d "$SCRIPT_DIR" ]; then
  echo "Directory $SCRIPT_DIR does not exist."
  exit 1
fi

# total test result
TOTAL_RESULT=0;

# Loop over each script in the directory
for script in "$SCRIPT_DIR"/*.sh; do
  # Check if there are no scripts in the directory
  if [ ! -e "$script" ]; then
    echo "No scripts found in $SCRIPT_DIR."
    break
  fi

  if [ "$script" == "$SCRIPT_DIR/1-1.sh" ]; then
	  continue
  fi

  # ignore checking change log 
  if [ "$script" == "$SCRIPT_DIR/1-2.sh" ]; then
	  continue
  fi

  # USE_ORT=ON (x86_64)
  #if [ "$script" == "$SCRIPT_DIR/3-1-1.sh" ]; then
	#  continue
  #fi

  # USE_ORT=ON (aarch64)
  #if [ "$script" == "$SCRIPT_DIR/3-4-1.sh" ]; then
	#  continue
  #fi

  # parse model
  if [ "$script" == "$SCRIPT_DIR/4-1.sh" ]; then
	  continue
  fi

  # run model
  if [ "$script" == "$SCRIPT_DIR/4-2-1.sh" ]; then
	  continue
  fi

  # run model
  if [ "$script" == "$SCRIPT_DIR/4-2-2.sh" ]; then
	  continue
  fi

  # run model
  if [ "$script" == "$SCRIPT_DIR/4-2-3.sh" ]; then
	  continue
  fi

  # run model
  if [ "$script" == "$SCRIPT_DIR/4-2-4.sh" ]; then
	  continue
  fi

  # run model
  if [ "$script" == "$SCRIPT_DIR/4-2-5.sh" ]; then
	  continue
  fi


  # exclude reset 
  if [ "$script" == "$SCRIPT_DIR/4-3-3.sh" ]; then
	  continue
  fi

  # exclude control firmware parameters 
  if [ "$script" == "$SCRIPT_DIR/4-3-5.sh" ]; then
	  continue
  fi

  # exclude dump device internal file
  if [ "$script" == "$SCRIPT_DIR/4-3-6.sh" ]; then
	  continue
  fi

  # exclude update firmware
  if [ "$script" == "$SCRIPT_DIR/4-3-7.sh" ]; then
	  continue
  fi

  # exclude firmware file version
  if [ "$script" == "$SCRIPT_DIR/4-3-8.sh" ]; then
	  continue
  fi

  # python binding
  if [ "$script" == "$SCRIPT_DIR/4-4-1.sh" ]; then
	  continue
  fi

#  if [ "$script" == "$SCRIPT_DIR/1-2.sh" ]; then
#	  continue
#  fi


  # Make sure the script is executable
  chmod +x "$script"

  item=$(grep "# Check" $script)

  # Execute the script
  RESULT=$($script $TEST_TARGET_DIR $TEST_MODEL_DIR $TEST_MODEL_NAME | tr -d '\0')

  # Check the exit status of the script
  #if [ $? -eq 1 ]; then
  #if echo $RESULT | grep -qE "FAIL|Error|error"; then
  if echo $RESULT | grep -qE "FAIL"; then
    echo "$script:$item: Fail" | tee -a $LOG_FILE
    TOTAL_RESULT=1;
    #exit 1
  else
    echo "$script:$item: Pass" | tee -a $LOG_FILE
  fi


done

echo "End: " $(date +"%y%m%d_%H:%M:%S") | tee -a $LOG_FILE
echo "Exit: " $TOTAL_RESULT | tee -a $LOG_FILE

exit $TOTAL_RESULT

