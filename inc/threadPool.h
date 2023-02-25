#pragma once

#include <pthread.h>

#define DEFAULT_TIME 1000u
#define DEFAULT_THREAD_NUM 10u
#define MIN_WAIT_TASK_NUM 5u

#define AddrOffset(p, type, member) \
    ((void *)((char *)(p) + offsetof(type, member)))

typedef void *(*pFunc)(void *);

// 任务
typedef struct
{
    pFunc func;
    void *arg;
} threadPool_Task_t;

typedef struct
{
    /* 线程池管理 */
    pthread_mutex_t lock;           // 锁住整个结构体
    pthread_mutex_t thread_counter; // 用于使用忙线程数时的锁
    pthread_cond_t queue_not_full;  // 条件变量，任务队列不为满
    pthread_cond_t queue_not_empty; // 条件变量，任务队列不为空
    pthread_t *threads;             // 存放线程的tid,实际上就是管理线程数组
    pthread_t admin_tid;            // 管理者线程tid
    threadPool_Task_t *task_queue;  // 任务队列
    /* 线程池信息 */
    int min_thr_num;       // 线程池中最小线程数
    int max_thr_num;       // 线程池中最大线程数
    int live_thr_num;      // 线程池中存活线程数
    int busy_thr_num;      // 忙线程，正在工作的线程
    int wait_exit_thr_num; // 需要销毁的线程数
    /* 任务队列信息 */
    int queue_front; // 队头
    int queue_rear;  // 队尾
    int queue_size;  // 队列大小
    /* 存在的任务数 */
    int queue_max_size; // 队列能容纳的最大任务数
    /* 线程池状态 */
    int shutdown; // 1 为关闭 0 为打开
} threadPool_t;

void *threadPool_create(int min_thr_num, int max_thr_num, int queue_max_size);
int threadPool_free(void *threadPool);
int threadPool_destroy(void *threadPool);
void *threadPool_thread(void *threadPool);
void *threadPool_admin_thread(void *threadPool);
int threadPool_add_task(void *threadPool, pFunc function, void *arg);
int is_thread_alive(pthread_t tid);