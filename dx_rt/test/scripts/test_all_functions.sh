#!/bin/bash

DEFAULT_SERVICE='on'
DEFAULT_USE_ORT='off'
TEST_SERVICE=${1:-$DEFAULT_SERVICE}
TEST_USE_ORT=${2:-$DEFAULT_USE_ORT}

SERVICE_NAME='dxrtd'

if [ $TEST_SERVICE = 'off' ]; then
    if pgrep -x $SERVICE_NAME > /dev/null
    then
        echo "The service is running. ($SERVICE_NAME)"
        exit -1
    else
        echo "The service is not running. (OK) ($SERVICE_NAME)"
    fi
else
    if pgrep -x $SERVICE_NAME > /dev/null
    then
        echo "The service is running. (OK) ($SERVICE_NAME)"
    else
        echo "The service is not running. ($SERVICE_NAME)"
        exit -1
    fi
fi

MODEL_PATHS=(
    #'/home/sjkim/Development/Assets/models/YOLOv7_512-YOLOV7-4/YOLOv7_512.dxnn'
    #'/home/sjkim/Development/Assets/models/EfficientNetB4_2-EfficientNetB4-3/EfficientNetB4_2.dxnn'
    #'/mnt/regression_storage/ci-data/bitmatch_data_set/M1A/ResNet50_7-ResNet50-8/ResNet50_7.dxnn'
    '/mnt/regression_storage/ci-data/bitmatch_data_set/M1A/YOLOv7_512-YOLOV7-4/YOLOv7_512.dxnn'
    '/mnt/regression_storage/ci-data/bitmatch_data_set/M1A/EfficientNetB4_2-EfficientNetB4-3/EfficientNetB4_2.dxnn'
)

for M_PATH in "${MODEL_PATHS[@]}"; do
    echo $M_PATH
    if [ -e $M_PATH ]; then
        echo "File exists."
    else
        echo "File does not exist."
        exit -1
    fi
done


LOG_FILE="test_dx_rt_all_$(date +"%y%m%d_%H%M").log"

if [ $TEST_SERVICE = 'off' ]; then
    echo 'Start DXRT Test All (USE_SERVICE=OFF)' | tee $LOG_FILE
else
    echo 'Start DXRT Test All (USE_SERVICE=ON)' | tee $LOG_FILE
fi

# Color code
NC='\033[0m' # No Color
RED='\033[0;31m'

STR_Success='Success'
STR_Failure='Failure'
STR_START='Start'
STR_END='End'

echo "Start Time: " $(date +"%y%m%d_%H:%M:%S") | tee -a $LOG_FILE
ALL_TEST_COUNT=0
PASS_TEST_COUNT=0


# Test Functions
FUNC_UNITTEST()
{
    TEST_NAME=$1
    COMMAND=$2
    MODEL_PATH=$3
    #LOG_FILE=$4

    ((ALL_TEST_COUNT++))

    echo $TEST_NAME $STR_START
    $COMMAND '-m' $MODEL_PATH
    exit_code=$?
    if [ $exit_code -eq 0 ]; then
        echo $TEST_NAME $STR_END ":" $STR_Success
        echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME ":" $STR_Success | tee -a $LOG_FILE
        ((PASS_TEST_COUNT++))
        return 0
    else
        echo -e $TEST_NAME $STR_END ":" "${RED}$STR_Failure${NC}"
        echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME ":" $STR_Failure | tee -a $LOG_FILE
        echo $COMMAND '-m' $MODEL_PATH | tee -a $LOG_FILE
        return -1
    fi 

}

FUNC_RUN_MODEL()
{
    TEST_NAME=$1
    COMMAND=$2
    MODEL_PATH=$3
    OPTION_1=$4
    ARGUMENT_1=$5
    EXTRA_1=$6
    EXTRA_2=$7
    EXTRA_3=$8
    
    ((ALL_TEST_COUNT++))

    echo $TEST_NAME $STR_START
    #echo $COMMAND '-m' $MODEL_PATH $OPTION_1 $ARGUMENT_1 $EXTRA_1 $EXTRA_2 $EXTRA_3 | tee -a $LOG_FILE
    $COMMAND '-m' $MODEL_PATH $OPTION_1 $ARGUMENT_1 $EXTRA_1 $EXTRA_2 $EXTRA_3

    exit_code=$?
    if [ $exit_code -eq 0 ]; then
        echo $TEST_NAME $STR_END ":" $STR_Success
        echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME ":" $STR_Success | tee -a $LOG_FILE
        ((PASS_TEST_COUNT++))
        return 0
    else
        echo -e $TEST_NAME $STR_END ":" "${RED}$STR_Failure${NC}"
        echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME ":" $STR_Failure | tee -a $LOG_FILE
        echo $COMMAND '-m' $MODEL_PATH $OPTION_1 $ARGUMENT_1 $EXTRA_1 $EXTRA_2 $EXTRA_3 | tee -a $LOG_FILE
        return -1
    fi 

}

FUNC_RUN_MODEL_CHECK_OUTPUT()
{
    TEST_NAME=$1
    COMMAND=$2
    MODEL_PATH=$3
    OPTION_1=$4
    ARGUMENT_1=$5
    OPTION_2=$6
    ARGUMENT_2=$7

    ((ALL_TEST_COUNT++))
    

    echo $TEST_NAME $STR_START
    #echo $COMMAND '-m' $MODEL_PATH $OPTION_1 $ARGUMENT_1 $OPTION_2 $ARGUMENT_2 | tee -a $LOG_FILE
    $COMMAND '-m' $MODEL_PATH $OPTION_1 $ARGUMENT_1 $OPTION_2 $ARGUMENT_2

    exit_code=$?
    if [ $exit_code -eq 0 ]; then
        #echo $TEST_NAME $STR_END ":" $STR_Success
        #echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME ":" $STR_Success | tee -a $LOG_FILE
        if find ./ | grep -q $ARGUMENT_2 ; then
	        rm $ARGUMENT_2
            echo $TEST_NAME $STR_END ":" $STR_Success
            echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME "(check output file) :" $STR_Success | tee -a $LOG_FILE
            ((PASS_TEST_COUNT++))
            return 0
        else
            echo -e $TEST_NAME $STR_END ":" "${RED}$STR_Failure${NC}"
            echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME "(check output file) :" $STR_Failure | tee -a $LOG_FILE
            echo $COMMAND '-m' $MODEL_PATH $OPTION_1 $ARGUMENT_1 $OPTION_2 $ARGUMENT_2 | tee -a $LOG_FILE
            return -1
        fi
        
    else
        echo -e $TEST_NAME $STR_END ":" "${RED}$STR_Failure${NC}"
        echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME ":" $STR_Failure | tee -a $LOG_FILE
        echo $COMMAND '-m' $MODEL_PATH $OPTION_1 $ARGUMENT_1 $OPTION_2 $ARGUMENT_2 | tee -a $LOG_FILE
        return -1
    fi 
}

FUNC_CLI()
{
    TEST_NAME=$1
    COMMAND=$2
    OPTION=$3
    ARGUMENT=$4

    ((ALL_TEST_COUNT++))
    
    echo $TEST_NAME $STR_START
    $COMMAND $OPTION $ARGUMENT
    exit_code=$?
    echo "log" $LOG_FILE
    if [ $exit_code -eq 0 ]; then
        echo $TEST_NAME $STR_END ":" $STR_Success
        echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME ":" $STR_Success | tee -a $LOG_FILE
        ((PASS_TEST_COUNT++))
        return 0
    else
        echo -e $TEST_NAME $STR_END ":" "${RED}$STR_Failure${NC}"
        echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME ":" $STR_Failure | tee -a $LOG_FILE
        echo $COMMAND $OPTION $ARGUMENT | tee -a $LOG_FILE
        return -1
    fi 

}

FUNC_BITMATCH_CPP()
{
    TEST_NAME=$1
    COMMAND=$2
    MODEL_PATH=$3
    PROCESS_COUNT=$4
    LOOP_COUNT=$5
    SYNC=$6
    THREAD_COUNT=$7
    USE_ORT_OPT=$8

    ((ALL_TEST_COUNT++))

    echo $TEST_NAME $STR_START
    $COMMAND $MODEL_PATH $PROCESS_COUNT $LOOP_COUNT $SYNC $THREAD_COUNT $USE_ORT_OPT
    exit_code=$?

    CONDITION=$(echo 'p='$PROCESS_COUNT 't='$THREAD_COUNT $SYNC 'loop='$LOOP_COUNT)
    if [ $exit_code -eq 0 ]; then
        echo $TEST_NAME $STR_END ":" $STR_Success
        echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME $CONDITION ":" $STR_Success | tee -a $LOG_FILE
        ((PASS_TEST_COUNT++))
        return 0
    else
        echo -e $TEST_NAME $STR_END ":" "${RED}$STR_Failure${NC}"
        echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME $CONDITION ":" $STR_Failure | tee -a $LOG_FILE
        echo $COMMAND $MODEL_PATH $PROCESS_COUNT $LOOP_COUNT $SYNC $THREAD_COUNT $USE_ORT_OPT | tee -a $LOG_FILE
        return -1
    fi    
}

FUNC_PYTHON_API() 
{
    TEST_NAME=$1
    PYTHON_PATH=$2
    PYTHON_API_TEST=$3
    MODEL_PATH=$4

    ((ALL_TEST_COUNT++))

    echo $TEST_NAME $STR_START
    $PYTHON_PATH $PYTHON_API_TEST '--model' $MODEL_PATH
    exit_code=$?

    if [ $exit_code -eq 0 ]; then
        echo $TEST_NAME $STR_END ":" $STR_Success
        echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME ":" $STR_Success | tee -a $LOG_FILE
        ((PASS_TEST_COUNT++))
        return 0
    else
        echo -e $TEST_NAME $STR_END ":" "${RED}$STR_Failure${NC}"
        echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME ":" $STR_Failure | tee -a $LOG_FILE
        echo $PYTHON_PATH $PYTHON_API_TEST '--model' $MODEL_PATH | tee -a $LOG_FILE
        return -1
    fi    
}

FUNC_PYTHON_EXAMPLE() 
{
    TEST_NAME=$1
    PYTHON_PATH=$2
    PYTHON_API_TEST=$3
    MODEL_PATH=$4
    LOOP_COUNT=$5

    ((ALL_TEST_COUNT++))

    echo $TEST_NAME $STR_START
    $PYTHON_PATH $PYTHON_API_TEST $MODEL_PATH $LOOP_COUNT
    exit_code=$?

    if [ $exit_code -eq 0 ]; then
        echo $TEST_NAME $STR_END ":" $STR_Success
        echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME "("$PYTHON_API_TEST") " ":" $STR_Success | tee -a $LOG_FILE
        ((PASS_TEST_COUNT++))
        return 0
    else
        echo -e $TEST_NAME $STR_END ":" "${RED}$STR_Failure${NC}"
        echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME "("$PYTHON_API_TEST") " ":" $STR_Failure | tee -a $LOG_FILE
        echo $PYTHON_PATH $PYTHON_API_TEST $MODEL_PATH $LOOP_COUNT | tee -a $LOG_FILE
        return -1
    fi    

}

FUNC_BITMATCH_PYTHON() {
    TEST_NAME=$1
    PYTHON_PATH=$2
    PYTHON_BITMATCH_TEST=$3
    MODEL_PATH=$4
    LOOP_COUNT=$5
    ASYNC_BATCH=$6
    PROCESS_COUNT=$7

    ((ALL_TEST_COUNT++))

    echo $TEST_NAME $STR_START

    if [ "$ASYNC_BATCH" == "async" ]; then
        COMMAND="$PYTHON_PATH $PYTHON_BITMATCH_TEST --model $MODEL_PATH -l $LOOP_COUNT -v"
    elif [ "$ASYNC_BATCH" == "batch" ]; then
        COMMAND="$PYTHON_PATH $PYTHON_BITMATCH_TEST --model $MODEL_PATH -l $LOOP_COUNT -b -v"
    else
        echo "Invalid ASYNC_BATCH value: $ASYNC_BATCH"
        return -1
    fi

    exit_codes=()
    for ((i = 1; i <= PROCESS_COUNT; i++)); do
        echo "Starting process $i: $COMMAND"
        $COMMAND &
        pids[$i]=$! 
    done

    for pid in ${pids[@]}; do
        wait $pid
        exit_codes+=($?)
    done

    failure_detected=0
    for code in "${exit_codes[@]}"; do
        if [ $code -ne 0 ]; then
            failure_detected=1
            break
        fi
    done

    CONDITION=$(echo 'p='$PROCESS_COUNT 'loop='$LOOP_COUNT)
    if [ $failure_detected -eq 0 ]; then
        echo $TEST_NAME $STR_END ":" $STR_Success
        echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME $CONDITION ":" $STR_Success | tee -a $LOG_FILE
        ((PASS_TEST_COUNT++))
        return 0
    else
        echo -e $TEST_NAME $STR_END ":" "${RED}$STR_Failure${NC}"
        echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME $CONDITION ":" $STR_Failure | tee -a $LOG_FILE
        echo $COMMAND $MODEL_PATH $PROCESS_COUNT $LOOP_COUNT $SYNC $THREAD_COUNT | tee -a $LOG_FILE
        return -1
    fi
}


FUNC_EXAMPLE() 
{
    TEST_NAME=$1
    EXAMPLE_NAME=$2
    MODEL_PATH=$3
    LOOP_COUNT=$4

    ((ALL_TEST_COUNT++))

    echo $TEST_NAME $EXAMPLE_NAME $STR_START
    $EXAMPLE_NAME $MODEL_PATH $LOOP_COUNT
    exit_code=$?

    if [ $exit_code -eq 0 ]; then
        echo $TEST_NAME $EXAMPLE_NAME $STR_END ":" $STR_Success
        echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME "("$EXAMPLE_NAME") " ":" $STR_Success | tee -a $LOG_FILE
        ((PASS_TEST_COUNT++))
        return 0
    else
        echo -e $TEST_NAME $EXAMPLE_NAME $STR_END ":" "${RED}$STR_Failure${NC}"
        echo $(date +"%y%m%d_%H:%M:%S") $TEST_NAME "("$EXAMPLE_NAME") " ":" $STR_Failure | tee -a $LOG_FILE
        echo $EXAMPLE_NAME $MODEL_PATH $LOOP_COUNT | tee -a $LOG_FILE
        return -1
    fi    

}



#Test Items
TEST_UNITTEST()
{
    MODEL_PATH=$1
    COMMAND='./bin/dxrt_test'
    FUNC_UNITTEST 'Unit Test' $COMMAND $MODEL_PATH

}

TEST_RUN_MODEL()
{
    MODEL_PATH=$1
    MODEL_FOLDER="${MODEL_PATH%/*}"
    MODEL_NAME=$(basename "$MODEL_PATH")

    COMMAND='./bin/run_model'
    OPTION_1=""
    ARGUMENT_1=""
    FUNC_RUN_MODEL 'Run Model (model only)' $COMMAND $MODEL_PATH $OPTION_1 $ARGUMENT_1

    if [ $TEST_USE_ORT = 'off' ]; then
        GT_INPUT_NAME='gt/npu_0_input_0.bin'
        RT_OUTPUT_NAME="npu_0_output_0.${MODEL_NAME}.bin"
    else
        GT_INPUT_NAME='gt/input_0.bin'
        RT_OUTPUT_NAME="output_0.${MODEL_NAME}.bin"
    fi

    OPTION_1='-i'
    ARGUMENT_1=$MODEL_FOLDER/$GT_INPUT_NAME
    FUNC_RUN_MODEL 'Run Model (model & input)' $COMMAND $MODEL_PATH $OPTION_1 $ARGUMENT_1

    OPTION_1='-i'
    ARGUMENT_1=$MODEL_FOLDER/$GT_INPUT_NAME
    FUNC_RUN_MODEL 'Run Model (loop=100)' $COMMAND $MODEL_PATH $OPTION_1 $ARGUMENT_1 '-b' '-l' '100'

    #OPTION_1='-i'
    #ARGUMENT_1=$MODEL_FOLDER/$GT_INPUT_NAME
    #OPTION_2="-o"
    #ARGUMENT_2=$RT_OUTPUT_NAME
    #FUNC_RUN_MODEL_CHECK_OUTPUT 'Run Model (model & input & output)' $COMMAND $MODEL_PATH $OPTION_1 $ARGUMENT_1 $OPTION_2 $ARGUMENT_2

}

TEST_BITMATCH()
{   
    MODEL_PATH=$1
    COMMAND='./bin/test/dxrt_test_bit_match'

    if [ $TEST_SERVICE = 'off' ]; then
        FUNC_BITMATCH_CPP 'Function Test (basic-inference)' $COMMAND $MODEL_PATH 1 30 sync 0 $TEST_USE_ORT
        FUNC_BITMATCH_CPP 'Function Test (basic-inference)' $COMMAND $MODEL_PATH 1 30 async 0 $TEST_USE_ORT
        FUNC_BITMATCH_CPP 'Function Test (multi-thread)' $COMMAND $MODEL_PATH 1 20 sync 16 $TEST_USE_ORT
        FUNC_BITMATCH_CPP 'Function Test (multi-thread)' $COMMAND $MODEL_PATH 1 20 async 16 $TEST_USE_ORT
        ignore() {
            echo ''
        }
    else
        FUNC_BITMATCH_CPP 'Function Test (basic-inference)' $COMMAND $MODEL_PATH 1 30 sync 0 $TEST_USE_ORT
        FUNC_BITMATCH_CPP 'Function Test (basic-inference)' $COMMAND $MODEL_PATH 1 30 async 0 $TEST_USE_ORT
        FUNC_BITMATCH_CPP 'Function Test (multi-process & multi-thread)' $COMMAND $MODEL_PATH 3 10 async 5 $TEST_USE_ORT
        ignore() {
        
            
            FUNC_BITMATCH_CPP 'Function Test (multi-process)' $COMMAND $MODEL_PATH 5 20 sync 0 $TEST_USE_ORT
            FUNC_BITMATCH_CPP 'Function Test (multi-process)' $COMMAND $MODEL_PATH 5 20 async 0 $TEST_USE_ORT
            FUNC_BITMATCH_CPP 'Function Test (multi-thread)' $COMMAND $MODEL_PATH 1 20 sync 16 $TEST_USE_ORT
            FUNC_BITMATCH_CPP 'Function Test (multi-thread)' $COMMAND $MODEL_PATH 1 20 async 16 $TEST_USE_ORT
        }
    fi
    
}

TEST_PYTHON()
{
    MODEL_PATH=$1

    PYTHON_PATH=$(which python3)
    PYTHON_API_TEST='./python_package/examples/python_api_test.py'
    COMMAND_PYTHON_API=$PYTHON_PATH # $PYTHON_API_TEST
    echo $COMMAND_PYTHON_API
    echo $PYTHON_API_TEST
    FUNC_PYTHON_API 'Function Test (python-api)' $PYTHON_PATH $PYTHON_API_TEST $MODEL_PATH

    FUNC_PYTHON_EXAMPLE 'Example Test (Python) ' $PYTHON_PATH './examples/python/run_sync_model.py' $MODEL_PATH 100
    FUNC_PYTHON_EXAMPLE 'Example Test (Python) ' $PYTHON_PATH './examples/python/run_async_model.py' $MODEL_PATH 100
    FUNC_PYTHON_EXAMPLE 'Example Test (Python) ' $PYTHON_PATH './examples/python/run_async_model_thread.py' $MODEL_PATH 100
    FUNC_PYTHON_EXAMPLE 'Example Test (Python) ' $PYTHON_PATH './examples/python/run_async_model_wait.py' $MODEL_PATH 100
    #FUNC_EXAMPLE 'Example Test(Python) ' './bin/examples/run_sync_model_bound' $MODEL_PATH 100
    #FUNC_EXAMPLE 'Example Test(Python) ' './bin/examples/run_async_model' $MODEL_PATH 100
    #FUNC_EXAMPLE 'Example Test(Python) ' './bin/examples/run_async_model_thread' $MODEL_PATH 100
    #FUNC_EXAMPLE 'Example Test(Python) ' './bin/examples/run_async_model_wait' $MODEL_PATH 100
    #FUNC_EXAMPLE 'Example Test(Python) ' './bin/examples/run_async_model_output' $MODEL_PATH 100
}

TEST_BITMATCH_PYTHON()
{
    MODEL_PATH=$1
    
    PYTHON_PATH=$(which python3)
    PYTHON_BITMATCH_TEST='./python_package/examples/bitmatch.py'

    if [ $TEST_SERVICE = 'off' ]; then
        FUNC_BITMATCH_PYTHON 'Python Bitmatch Test (basic-inference)' $PYTHON_PATH $PYTHON_BITMATCH_TEST $MODEL_PATH 30 async 1
        #FUNC_BITMATCH_PYTHON 'Python Bitmatch Test (basic-inference)' $PYTHON_PATH $PYTHON_BITMATCH_TEST $MODEL_PATH 30 batch 1 
        ignore() {
            echo ''
        }
    else
        FUNC_BITMATCH_PYTHON 'Python Bitmatch Test (basic-inference)' $PYTHON_PATH $PYTHON_BITMATCH_TEST $MODEL_PATH 30 async 1
        #FUNC_BITMATCH_PYTHON 'Python Bitmatch Test (basic-inference)' $PYTHON_PATH $PYTHON_BITMATCH_TEST $MODEL_PATH 30 batch 1
        #FUNC_BITMATCH_PYTHON 'Python Bitmatch Test (multi-process)' $PYTHON_PATH $PYTHON_BITMATCH_TEST $MODEL_PATH 30 async 5
        #FUNC_BITMATCH_PYTHON 'Python Bitmatch Test (multi-process)' $PYTHON_PATH $PYTHON_BITMATCH_TEST $MODEL_PATH 30 batch 5
    fi
}

TEST_EXAMPLE()
{
    MODEL_PATH=$1
    FUNC_EXAMPLE 'Example Test ' './bin/examples/run_sync_model' $MODEL_PATH 100
    FUNC_EXAMPLE 'Example Test ' './bin/examples/run_sync_model_bound' $MODEL_PATH 100
    FUNC_EXAMPLE 'Example Test ' './bin/examples/run_async_model' $MODEL_PATH 100
    FUNC_EXAMPLE 'Example Test ' './bin/examples/run_async_model_thread' $MODEL_PATH 100
    FUNC_EXAMPLE 'Example Test ' './bin/examples/run_async_model_wait' $MODEL_PATH 100
    FUNC_EXAMPLE 'Example Test ' './bin/examples/run_async_model_output' $MODEL_PATH 100
    FUNC_EXAMPLE 'Example Test ' './bin/examples/display_async_wait' $MODEL_PATH 1000 # multi-model single-thread async (wait)
    FUNC_EXAMPLE 'Example Test ' './bin/examples/display_async_pipe' $MODEL_PATH 1000 # multi-model single-thread async (wait-pipe)

}


# Test model
TEST_MODEL()
{
    TARGET_MODEL_PATH=$1
    echo "Model Path:" $TARGET_MODEL_PATH | tee -a $LOG_FILE
    TEST_UNITTEST $TARGET_MODEL_PATH
    TEST_RUN_MODEL $TARGET_MODEL_PATH 
    TEST_BITMATCH $TARGET_MODEL_PATH
    TEST_PYTHON $TARGET_MODEL_PATH
    TEST_BITMATCH_PYTHON $TARGET_MODEL_PATH
    TEST_EXAMPLE $TARGET_MODEL_PATH
}

# Test cli
TEST_CLI()
{
    
    COMMAND='./bin/dxrt-cli'
    FUNC_CLI 'CLI status' $COMMAND '-s' ''
    FUNC_CLI 'CLI info' $COMMAND '-i' ''
    FUNC_CLI 'CLI help' $COMMAND '-h' ''
}



for M_PATH in "${MODEL_PATHS[@]}"; do
    TEST_MODEL $M_PATH
    echo " " | tee -a $LOG_FILE
done

echo "TEST CLI" | tee -a $LOG_FILE
TEST_CLI

echo "End Time: " $(date +"%y%m%d_%H:%M:%S") | tee -a $LOG_FILE
echo " " | tee -a $LOG_FILE

if ((PASS_TEST_COUNT == ALL_TEST_COUNT)); then
    RESULT=$(echo Test Result '('$PASS_TEST_COUNT'/'$ALL_TEST_COUNT')' All Success)
    echo $RESULT | tee -a $LOG_FILE
    exit 0
else
    RESULT=$(echo Test Result '('$PASS_TEST_COUNT'/'$ALL_TEST_COUNT')' Failure)
    echo $RESULT | tee -a $LOG_FILE
    exit -1
fi