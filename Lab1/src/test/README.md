A backtracking sudoku solver, four algorithms are shown.

test cases are from http://people.csse.uwa.edu.au/gordon/sudokumin.php

### Some instructions

- `main_original.cc`: 原始的单线程实现
- `main.cc`: first version, 多线程（生产者+消费者）实现，实现方式是先将文件中的数据读入，再创建多线程求解，一个生产者用于将数据放到缓冲区，多个消费者获取数据求解问题。
- `main_test.cc`: 用于测试`main.cc`的参数设置对性能的影响，包括生产者数量、消费者数量、缓冲区大小等，对应测试shell程序为`test.sh`
- `main_v2.cc`: second version，多线程实现，同样是生产者+消费者，创建一个线程用于接收文件输入，一个线程用于读取文件中的数据，一个线程用于将数据放到缓冲区（生产者），多个线程用于从缓冲区中读取数据求解问题。
- `main_v3.cc`: third version，多线程实现，同样是生产者+消费者，创建一个线程用于接收文件输入，一个线程用于读取文件中的数据，多个线程用于读取数据求解问题，在第二个版本的基础上删除了“读取数据放到缓冲区”的线程，减少冗余操作
- `main_v3.cc`: fourth version，在第三个版本的基础上增加一个专门用于输出的线程
- `test.sh`: 用于测试以上main文件的脚本
- `report`: 对`main.cc`和`main_v3.cc`简单的性能测试报告
- `lab.sh`: 更新的测试脚本
- `Lab1.sh`: some modifications

```sh
#screen -S $1 -X stuff "stdbuf -oL ./${Exe_Prog} > ./${Result}^M"
cmd="cat ${test_data_txt} | ./${Exe_Prog} > ./${Result}"
eval ${cmd}
```