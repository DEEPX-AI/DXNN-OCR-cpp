#!/bin/bash
LOOPS=50
MODEL_PATH='/mnt/regression_storage/ci-data/bitmatch_data_set/M1A/YOLOv7_512-YOLOV7-4/YOLOv7_512.dxnn'

function do_run() {
    # TODO - get model path
    run_model -m $MODEL_PATH -b -l 30 &
}

if [[ $# -eq 0 ]]; then
    PROCESS=5
else
    PROCESS=$1
fi

pids=()

for (( loop=1; loop<=LOOPS; loop++ )) 
do
    for (( i=1; i<=PROCESS; i++ ))
    do
        echo "Execute Process $i"
        do_run
        pid=$!
        pids+=($pid)
        echo "Process ID : $pid"
    done

    sleep 0.1

    echo "Send SIGINT to each process ${pids[@]}"
    for pid in "${pids[@]}"
    do
        kill -SIGINT $pid
    done
    pids=()
done

## TODO - Verify function to devcie pass / fail via bit match 
#SOURCE_DIR='../../'
#tc_bit_match.sh $SOURCE_DIR $MODEL_PATH

echo "Teminate shell script"