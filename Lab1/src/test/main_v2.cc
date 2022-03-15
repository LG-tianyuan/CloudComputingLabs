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
#include <unistd.h>
#include <stdlib.h>
#include <list>
#include <vector>
#include <queue>
#include "sudoku.h"
using namespace std;

pthread_mutex_t visit_buf = PTHREAD_MUTEX_INITIALIZER;    //缓冲区锁
pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;   //打印锁
pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER;     //互斥访问文件名称队列锁
pthread_mutex_t data_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;          //缓冲区为空条件变量
pthread_cond_t full = PTHREAD_COND_INITIALIZER;           //缓冲区不空条件变量
pthread_cond_t *print_order;                              //控制输出顺序
pthread_cond_t F_full = PTHREAD_COND_INITIALIZER;;        //文件队列不空条件变量
pthread_cond_t A_full = PTHREAD_COND_INITIALIZER;;        //数据队列不空条件变量
pthread_t *tid;                                           //解题线程
pthread_t file_thread;                                    //接收文件名称输入线程
pthread_t data_thread;                                    //从文件中读取数据线程
pthread_t pro_thread;                                     //生产者线程

queue<char*> F;     //接收文件名称输入
queue<char*> A;      //输入的问题数据
int total = 0;       //解决问题总数
char **buf;          //缓冲区，存放题目
int n_pthread;       //线程数量
int cnt = 0;         //缓冲区剩余题目个数
int use_index = 0;        
int fill_index = 0;       
int cur_print = 0;      //当前打印者编号
int len;            //数独问题数量      
bool file_flag;     //接收输入结束标志
bool data_flag;     //数据加载完毕标志
bool pro_flag;      //生产数据结束标志

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
    file_flag = data_flag = pro_flag = false;

    //缓冲区开辟n行81列
    buf = (char **)malloc(n * sizeof(char *));
    for (int i = 0; i < n_pthread; i++)
        buf[i] = (char *)malloc(81);

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
    for (int i = 0; i < n_pthread; ++i)
    {
        free(buf[i]);
    }
    free(buf);
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
                pthread_cond_signal(&A_full);
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

//生产
void put(char* value)
{
    buf[fill_index] = value;
    fill_index = (fill_index + 1) % n_pthread;
    cnt++;
}

//消费
char *get()
{
    char *tmp = buf[use_index];
    use_index = (use_index + 1) % n_pthread;
    cnt--;
    return tmp;
}

void *produce(void *arg)//生产者
{
    while(1)
    {
        pthread_mutex_lock(&data_lock);
        while(A.empty())
        {
            if(data_flag)
            {
                pro_flag = true;
                // printf("Produce out!\n");
                for(int i = 0; i < n_pthread; ++i){//唤醒处于睡眠状态的消费者使其退出
                    pthread_cond_signal(&full);
                }
                pthread_mutex_unlock(&data_lock);
                pthread_exit(NULL);
            }
            pthread_cond_wait(&A_full,&data_lock);
        }
        pthread_mutex_lock(&visit_buf);
        while (cnt == n_pthread){    
            pthread_cond_wait(&empty, &visit_buf);
        }
        put(A.front());
        A.pop();
        pthread_cond_signal(&full);
        pthread_mutex_unlock(&visit_buf);
        pthread_mutex_unlock(&data_lock);
    }
}

//解题
void *solver(void *arg)
{
    int tmp[81];
    while (1)
    {
        pthread_mutex_lock(&visit_buf);
        while (cnt == 0){
            if(pro_flag)//防止解题任务完成后线程继续陷入睡眠
            {
                pthread_mutex_unlock(&visit_buf);
                pthread_exit(NULL);
            }
            pthread_cond_wait(&full, &visit_buf);
        }

        total++;                              //当前数据的行数
        int myturn = (total - 1) % n_pthread; //对应的打印顺序

        char *data = get();

        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&visit_buf);

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

        for (int i = 0; i < 81; ++i)
        {
            printf("%d",tmp[i]);
        }
        printf("\n");

        cur_print = (cur_print + 1) % n_pthread;                     //下一个该打印的编号
        pthread_cond_signal(&print_order[(myturn + 1) % n_pthread]); //唤醒下一个，如果对方在睡的话
        pthread_mutex_unlock(&print_lock);
    }
}

int main(int argc, char *argv[])
{
    init(6);
    int64_t start = now();
    for (int i = 0; i < n_pthread; ++i){
        pthread_create(&tid[i], NULL, solver, NULL);        //解题
    }

    pthread_create(&file_thread, NULL, inputfile, NULL);
    pthread_create(&data_thread, NULL, load_data, NULL);
    pthread_create(&pro_thread, NULL, produce, NULL);

    for (int i = 0; i < n_pthread; ++i){
        pthread_join(tid[i], NULL);
    }

    int64_t end = now();
    double sec = (end-start)/1000000.0;
    // printf("%f sec %f ms each\n", sec, 1000*sec/len);

    program_end();
    return 0;
}
