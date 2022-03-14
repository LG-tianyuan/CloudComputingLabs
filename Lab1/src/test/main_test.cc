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
#include "sudoku.h"
using namespace std;

pthread_mutex_t visit_buf = PTHREAD_MUTEX_INITIALIZER;    //缓冲区锁
pthread_mutex_t lock_print = PTHREAD_MUTEX_INITIALIZER;   //打印锁
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;          
pthread_cond_t full = PTHREAD_COND_INITIALIZER;           
pthread_cond_t *print_order;                              //控制输出顺序
pthread_t *tid;                                           //解题线程
pthread_t file_thread;                                    //读文件线程

vector<char*> A;      //输入的问题数据
int total = 0;       //解决问题总数
char **buf;          //缓冲区，存放题目
int n_pthread;       //线程数量
int cnt = 0;         //缓冲区剩余题目个数
int use_index = 0;        
int fill_index = 0;       
int cur_print = 0;      //当前打印者编号
int **ans;          //答案
int len;            //数独问题数量
int ans_in;
int A_in;
int cos_wait;
int pro_wait;
int buf_size;

int64_t now()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000000 + tv.tv_usec;
}

void init(int n,int m)
{
    n_pthread = n;
    ans_in = 0;
    A_in = 0;
    total = 0;
    cur_print = 0;
    cos_wait = 0;
    pro_wait = 0;
    buf_size = m;

    //缓冲区开辟n行81列
    buf = (char **)malloc(buf_size * sizeof(char *));
    for (int i = 0; i < buf_size; i++)
        buf[i] = (char *)malloc(81);

    ans = (int **)malloc(sizeof(int *) * len);
    for (int i = 0; i < len; i++) 
      ans[i] = (int *)malloc(sizeof(int) * (81));

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
    //printf("1\n");
    free(tid);
    //printf("2\n");
    for (int i = 0; i < n_pthread; ++i)
        free(buf[i]);
    free(buf);
    //printf("3\n");
    // for (int i=0;i<len;i++)
    //   free(A[i]);
    // printf("4\n");
}

//accept input
void inputfile() {
  char puzzle[128];
  FILE *fp;
  string prefix = "";
  string name;
  // "ctrl D" to end input
  while (getline(cin, name)) {
    char *tmp_name = new char[40];
    name = prefix + name;
    strcpy(tmp_name, name.c_str());
    fp = fopen(tmp_name, "r");
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
        A.push_back(B);
      }
    }
    fclose(fp);
  }
  len = A.size();
}

//生产
void put(char* value)
{
    buf[fill_index] = value;
    // for(int i=0;i<81;i++)
    // {
    //   buf[fill_index][i] = value[i];
    //   printf("%c",buf[fill_index][i]);
    // }
    // printf("\n");
    fill_index = (fill_index + 1) % n_pthread;
    cnt++;
    //printf("Put successfully!\n");
}

//消费
char *get()
{
    char *tmp = buf[use_index];
    // for(int i=0;i<81;i++)
    // {
    //   printf("%c",tmp[i]);
    // }
    // printf("\n");
    use_index = (use_index + 1) % n_pthread;
    cnt--;
    //printf("Get successfully!\n");
    return tmp;
}

//解题
void *solver(void *arg)
{
    int *tmp = new int[81];
    while (total + cos_wait < len)
    {
        pthread_mutex_lock(&visit_buf);
        while (cnt == 0){
            if(total + cos_wait >= len)//防止解题任务完成后线程继续陷入睡眠
            {
                pthread_mutex_unlock(&visit_buf);
                pthread_exit(NULL);
            }
            cos_wait++;
            pthread_cond_wait(&full, &visit_buf);
            cos_wait--;
        }

        total++;                              //当前数据的行数
        int myturn = (total - 1) % n_pthread; //对应的打印顺序

        //读取buf中的一个数独问题
        char *data = get();


        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&visit_buf);

        for (int i = 0; i < 81; i++){
            tmp[i] = data[i] - '0';
        }

        //解决问题
        solve_sudoku_dancing_links1(tmp);

        //按顺序输出
        pthread_mutex_lock(&lock_print);
        while (myturn != cur_print){    //没有轮到则在自己的条件变量上等待
            pthread_cond_wait(&print_order[myturn], &lock_print);
        }

        for (int i = 0; i < 81; ++i)
        {
            //printf("%d",tmp[i]);
            ans[ans_in][i] = tmp[i];
        }
        //printf("\n");
        ans_in++;

        cur_print = (cur_print + 1) % n_pthread;                     //下一个该打印的编号
        pthread_cond_signal(&print_order[(myturn + 1) % n_pthread]); //唤醒下一个，如果对方在睡的话
        pthread_mutex_unlock(&lock_print);
    }
}

void *produce(void *arg)
{
    while(A_in + pro_wait < len){//生产者
        pthread_mutex_lock(&visit_buf);
        while (cnt == buf_size){   
            if(A_in + pro_wait >= len)
            {
                pthread_mutex_unlock(&visit_buf);
                pthread_exit(NULL);
            } 
            pro_wait++;
            pthread_cond_wait(&empty, &visit_buf);
            pro_wait--;
        }
        put(A[A_in]);
        A_in++;
        pthread_cond_signal(&full);
        pthread_mutex_unlock(&visit_buf);
    }
}

int main(int argc, char *argv[])
{
    int num = 16;
    if (argv[1] != NULL)
    {
        num = atoi(argv[1])>1 ? atoi(argv[1]) : num;
    }
    int pro_num = num/2;
    if (argv[2] != NULL)
    {
        pro_num = atoi(argv[2])>0 ? atoi(argv[2]) : pro_num;
        if(pro_num >= num) 
            pro_num = 1;
    }
    int b_size = num;
    if (argv[3] != NULL)
    {
        b_size = atoi(argv[3])>0 ? atoi(argv[3]) : num;
    }
    inputfile();
    init(num,b_size);
    int64_t start = now();

    for (int i = 0; i < pro_num; ++i){
        pthread_create(&tid[i], NULL, produce, NULL);        //解题
    }

    for (int i = pro_num; i < num; ++i){
        pthread_create(&tid[i], NULL, solver, NULL);        
    }

    for (int i = 0; i < n_pthread; ++i){
        pthread_join(tid[i], NULL);
    }

    int64_t end = now();
    double sec = (end-start)/1000000.0;
    printf("%f sec, %f ms each\n", sec, 1000*sec/len);

    // for (int i = 0; i < len; i++) 
    // {
    //   for (int j = 0; j < 81; j++) 
    //       printf("%d", ans[i][j]);
    //   printf("\n");
    // }

    program_end();
    return 0;
}
