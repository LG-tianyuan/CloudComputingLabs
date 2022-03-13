#! /bin/bash
test_data="test_group"
NUM_PRO=$2
NUM_THREAD=$1
run_cmd="cat ${test_data} | ./sudoku_solve ${NUM_THREAD} ${NUM_PRO}"
eval ${run_cmd}