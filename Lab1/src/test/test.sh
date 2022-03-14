#! /bin/bash
test_data="test_group"
NUM_PRO=$2
NUM_THREAD=$1
BUF_SIZE=$3
run_cmd="cat ${test_data} | ./sudoku_solve ${NUM_THREAD} ${NUM_PRO} ${BUF_SIZE}"
eval ${run_cmd}