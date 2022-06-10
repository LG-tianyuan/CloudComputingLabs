#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <assert.h>
#include "threadpool.h"
/*
*线程池里所有运行和等待的任务都是一个CThread_worker,所有任务都在一个链表里
*/

void *thread_routine (void *arg);

static CThread_pool *pool = NULL;
void pool_init (int max_thread_num)
{
    pool = (CThread_pool *) malloc (sizeof (CThread_pool));
    pthread_mutex_init (&(pool->queue_lock), NULL);
    pthread_cond_init (&(pool->queue_ready), NULL);
    pool->queue_head = NULL;
    pool->max_thread_num = max_thread_num;
    pool->cur_queue_size = 0;
    pool->shutdown = 0;
    pool->tid = (pthread_t *) malloc (max_thread_num * sizeof (pthread_t));
    int i = 0;
    for (i = 0; i < max_thread_num; i++)
    {  
        pthread_create (&(pool->tid[i]), NULL, thread_routine, NULL);
    }
}

/*往线程池中加入任务对象*/
int pool_add_worker (void *(*process) (void *arg), void *arg)
{
    //新建一个任务
    CThread_worker *newworker = (CThread_worker *) malloc (sizeof (CThread_worker));
    newworker->process = process;
    newworker->arg = arg;
    newworker->next = NULL; 
    pthread_mutex_lock (&(pool->queue_lock));
    /*将任务加入到等待队列中*/
    CThread_worker *member = pool->queue_head;
    if (member != NULL)
    {
        while (member->next != NULL)
            member = member->next;
        member->next = newworker;
    }
    else
    {
        pool->queue_head = newworker;
    }
    assert (pool->queue_head != NULL);
    pool->cur_queue_size++;
    pthread_mutex_unlock (&(pool->queue_lock));
    //任务队列不为空，唤醒一个等待线程进行处理
    pthread_cond_signal (&(pool->queue_ready));
    return 0;
}

/*销毁线程池，等待队列中的任务不会再被执行，但是正在运行的线程会执行完任务后再退出*/
int pool_destroy ()
{
    if (pool->shutdown)
        return -1;
    pool->shutdown = 1;
    /*唤醒所有等待线程*/
    pthread_cond_broadcast (&(pool->queue_ready));
    /*阻塞等待线程退出*/
    int i;
    for (i = 0; i < pool->max_thread_num; i++)
        pthread_join (pool->tid[i], NULL);
    free (pool->tid);
    /*销毁任务队列*/
    CThread_worker *head = NULL;
    while (pool->queue_head != NULL)
    {
        head = pool->queue_head;
        pool->queue_head = pool->queue_head->next;
        free (head);
    }
    pthread_mutex_destroy(&(pool->queue_lock));
    pthread_cond_destroy(&(pool->queue_ready));
    free (pool);
    pool=NULL;
    return 0;
}

//任务接口函数，各子线程统一调用这个函数，函数内部检查调用任务队列中的实际任务函数指针。
void * thread_routine (void *arg)
{
    printf ("starting thread 0x%lx/n", pthread_self ());
    while (1)
    {
        pthread_mutex_lock (&(pool->queue_lock));
        while (pool->cur_queue_size == 0 && !pool->shutdown)
        {
            pthread_cond_wait (&(pool->queue_ready), &(pool->queue_lock));
        }
        if (pool->shutdown)
        {
            pthread_mutex_unlock (&(pool->queue_lock));
            pthread_exit (NULL);
        }
        assert (pool->cur_queue_size != 0);
        assert (pool->queue_head != NULL);
        
        /*任务队列减1，取出链表中的头元素*/
        pool->cur_queue_size--;
        CThread_worker *worker = pool->queue_head;
        pool->queue_head = worker->next;
        pthread_mutex_unlock (&(pool->queue_lock));
        /*调用回调函数，执行任务*/
        (*(worker->process)) (worker->arg);
        free (worker);
        worker = NULL;
    }
    pthread_exit (NULL);
}