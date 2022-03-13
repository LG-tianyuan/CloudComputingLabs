/*
 * @Author: Firefly
 * @Date: 2020-03-08 13:56:54
 * @Descripttion:
 * @LastEditTime: 2020-03-30 21:59:54
 */

#include <stdlib.h>
#include <sys/time.h>
#include <iostream>
#include "inputfile.h"
#include "sudoku.h"
#include "threadpool.h"
using namespace std;


int len;

int **data;

int **ans;

double sec;

pthread_mutex_t stop;

int64_t now1()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000000 + tv.tv_usec;
}

bool (*solve)(int*) = tosolve;

void work(int cnt) {
  int *tmp = new int[81];  //注意没有释放 内存

  for (int i = 0; i < 81; i++) tmp[i] = data[cnt][i];
  if (solve(tmp)) {
    for (int i = 0; i < 81; i++) ans[cnt][i] = tmp[i];
  } else {

  }
}

int main() {
  intputfile();

  ans = (int **)malloc(sizeof(int *) * len);
  for (int i = 0; i < len; i++) ans[i] = (int *)malloc(sizeof(int) * (81));

  // int64_t start = now1();

  pthread_mutex_init(&stop, NULL);
  pthread_mutex_lock(&stop);  // 确保所有的线程都被创建

  int cntThread = 4;  // 设置线程数量
    if (len < cntThread) {
    cntThread = len;
  }

  Threadpool *pool = new Threadpool(cntThread, len, work);

  pthread_mutex_lock(&stop);

  // int64_t end = now1();
  // double sec = (end-start)/1000000.0;
  // printf("%f sec %f ms\n", sec, 1000*sec/len);

  // for (int i = 0; i < len; i++) {
  //   for (int j = 0; j < 81; j++) printf("%d", ans[i][j]);
  //   cout << endl;
  // }

}