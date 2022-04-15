#ifndef THREADPOOL_H
#define THREADPOOL_H

/*封装线程池中需要执行的任务对象*/
typedef struct worker
{
    void *(*process) (void *arg);	//回调函数，任务运行时会调用此函数
    void *arg;	//回调函数的参数
    struct worker *next;	//任务队列中的下一个任务
} CThread_worker;

/*线程池结构*/
typedef struct
{
    pthread_mutex_t queue_lock;	//队列锁
    pthread_cond_t queue_ready;	//队列状态
    CThread_worker *queue_head;	//链表结构，线程池中所有等待任务
    int shutdown;	//是否销毁线程池
    pthread_t *tid;	//线程id
    int max_thread_num;	//线程池中允许的活动线程最大数目
    int cur_queue_size;	//当前等待队列的任务数目
} CThread_pool;

/*线程池初始化*/
void pool_init(int max_thread_num);
/*往线程池中加入任务对象*/
int pool_add_worker(void *(*process) (void *arg), void *arg);
/*线程池销毁*/
int pool_destroy();

#endif