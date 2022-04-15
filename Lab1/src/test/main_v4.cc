/*
 * @Author: LG.tianyuan
 * @Date: 2022-03-16  
*/

#include <iostream>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <malloc.h>
#include <pthread.h>
#include <sys/time.h>
#include <queue>
#include "sudoku.h"
using namespace std;

pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;   //打印锁
pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER;    //互斥访问文件名称队列锁
pthread_mutex_t data_lock = PTHREAD_MUTEX_INITIALIZER;    //互斥访问数据区
pthread_mutex_t res_lock = PTHREAD_MUTEX_INITIALIZER;     //互斥访问结果区
pthread_cond_t *print_order;                              //控制结果存储顺序
pthread_cond_t F_full = PTHREAD_COND_INITIALIZER;         //文件队列不空条件变量
pthread_cond_t A_full = PTHREAD_COND_INITIALIZER;         //数据队列不空条件变量
pthread_cond_t R_full = PTHREAD_COND_INITIALIZER;         //答案队列不空条件变量
pthread_t *tid;                                           //解题线程
pthread_t file_thread;                                    //接收文件名称输入线程
pthread_t data_thread;                                    //从文件中读取数据线程
pthread_t out_thread;                                     //打印结果线程

queue<char*> F;     //接收文件名称输入
queue<char*> A;     //输入的问题数据
queue<int*> R;      //问题答案
int total;      //解决问题总数
int n_pthread;      //线程数量       
int cur_print;  //当前打印者编号
int len;            //数独问题数量      
bool file_flag;     //接收输入结束标志
bool data_flag;     //数据加载完毕标志
int solved_flag;   //解题完毕标志

int64_t now()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000000 + tv.tv_usec;
}

void init(int n)
{
    n_pthread = n;
    total = 0;
    cur_print = 0;
    file_flag = data_flag = false;
    solved_flag = 0;

    //n个条件变量控制输出
    print_order = (pthread_cond_t *)malloc(n * sizeof(pthread_cond_t));

    //n个线程号
    tid = (pthread_t *)malloc(n * sizeof(pthread_t));

    //初始化 输出顺序的条件变量
    for (int i = 0; i < n_pthread; i++){
        print_order[i] = PTHREAD_COND_INITIALIZER;
    }
}

//释放malloc的空间
void program_end()
{
    free(print_order);
    free(tid);
}

//accept input
void *inputfile(void *arg) {
  char name[40];
  // "ctrl D" to end input
  while (scanf("%s", name)!=EOF) {
    char *tmp_name = (char *)malloc(40);
    strcpy(tmp_name, name);
    pthread_mutex_lock(&file_lock);
    F.push(tmp_name);
    pthread_cond_signal(&F_full);
    pthread_mutex_unlock(&file_lock);
  }
  file_flag = true;
  pthread_cond_signal(&F_full);
  // printf("File input out!\n");
}

void *load_data(void *arg)
{
    char puzzle[128];
    while(1)
    {   
        pthread_mutex_lock(&file_lock);
        while(F.empty())
        {
            if(file_flag)
            {
                data_flag = true;
                // printf("Load data out!\n");
                for(int i = 0; i < n_pthread; ++i){//唤醒处于睡眠状态的消费者使其退出
                    pthread_cond_signal(&A_full);
                }
                pthread_mutex_unlock(&file_lock);
                pthread_exit(NULL);
            }
            pthread_cond_wait(&F_full,&file_lock);
        } 
        char *name;
        name = F.front();
        F.pop();
        pthread_mutex_unlock(&file_lock);
        FILE *fp;
        fp = fopen(name, "r");
        if (fp == NULL) {
          printf("Failed to open file!\n");
          exit(0);
        }
        while (fgets(puzzle, sizeof puzzle, fp) != NULL) {
            if (strlen(puzzle) >= 81) {
                char *B;
                B = (char *)malloc(81);
                for (int i = 0; i < 81; i++) {
                    B[i] = puzzle[i];
                }
                pthread_mutex_lock(&data_lock);
                A.push(B);
                pthread_cond_signal(&A_full);
                pthread_mutex_unlock(&data_lock);
                len++;//计数
            }
        }
        free(name); //及时释放空间
    }
}

//解题
void *solver(void *arg)
{
    while (1)
    {
        pthread_mutex_lock(&data_lock);
        while (A.empty()){
            if(data_flag)//防止解题任务完成后线程继续陷入睡眠
            {
                pthread_mutex_unlock(&data_lock);
                solved_flag += 1;
                pthread_exit(NULL);
            }
            pthread_cond_wait(&A_full, &data_lock);
        }

        total++;                              //当前数据的行数
        int myturn = (total - 1) % n_pthread; //对应的打印顺序

        char *data = A.front();
        A.pop();

        pthread_mutex_unlock(&data_lock);

        int *tmp;
        tmp = (int *)malloc(81*sizeof(int));
        for (int i = 0; i < 81; i++){
            tmp[i] = data[i] - '0';
        }

        free(data);
        //解决问题
        solve_sudoku_dancing_links1(tmp);

        //按顺序输出
        pthread_mutex_lock(&print_lock);
        while (myturn != cur_print){    //没有轮到则在自己的条件变量上等待
            pthread_cond_wait(&print_order[myturn], &print_lock);
        }

        pthread_mutex_lock(&res_lock);
        R.push(tmp);
        pthread_cond_signal(&R_full);
        pthread_mutex_unlock(&res_lock);
        // for (int i = 0; i < 81; ++i)
        // {
        //     printf("%d",tmp[i]);
        // }
        // printf("\n");

        cur_print = (cur_print + 1) % n_pthread;                     //下一个该打印的编号
        pthread_cond_signal(&print_order[(myturn + 1) % n_pthread]); //唤醒下一个，如果对方在睡的话
        pthread_mutex_unlock(&print_lock);
    }
}

void* output(void* args)
{
    while(1)
    {
        int* res;
        pthread_mutex_lock(&res_lock);
        while(R.empty())
        {
            if(solved_flag == n_pthread)
            {
                pthread_mutex_lock(&res_lock);
                pthread_exit(NULL);
            }
            pthread_cond_wait(&R_full,&res_lock);
        }
        res = R.front();
        R.pop();
        pthread_mutex_unlock(&res_lock);
        for (int i = 0; i < 81; ++i)
        {
            printf("%d",res[i]);
        }
        printf("\n");
        free(res);
    }
}

int main(int argc, char *argv[])
{
    int cpu_num=sysconf(_SC_NPROCESSORS_CONF);
    int num= cpu_num>1 ? cpu_num-1 : 1;
    if (argv[1] != NULL)
    {
        num = atoi(argv[1])>0 ? atoi(argv[1]) : num;
    }
    init(num);
    int64_t start = now();
    for (int i = 0; i < n_pthread; ++i){
        pthread_create(&tid[i], NULL, solver, NULL);        //解题
    }

    pthread_create(&file_thread, NULL, inputfile, NULL);
    pthread_create(&data_thread, NULL, load_data, NULL);
    pthread_create(&out_thread, NULL, output, NULL);

    for (int i = 0; i < n_pthread; ++i){
        pthread_join(tid[i], NULL);
    }

    int64_t end = now();
    double sec = (end-start)/1000000.0;
    printf("%f sec %f ms each\n", sec, 1000*sec/len);

    program_end();
    return 0;
}
