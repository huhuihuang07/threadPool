#include "threadPool.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>

void *threadPool_create(int min_thr_num, int max_thr_num, int queue_max_size)
{
    threadPool_t *pool = NULL;

    do
    {
        if (NULL == (pool = ((threadPool_t *)malloc(sizeof(threadPool_t)))))
        {
            fprintf(stderr, "malloc threadPool false;\n");
            break;
        }

        pool->min_thr_num = min_thr_num;
        pool->max_thr_num = max_thr_num;
        pool->busy_thr_num = pool->wait_exit_thr_num = 0;
        pool->live_thr_num = min_thr_num;
        pool->queue_front = pool->queue_rear = pool->queue_size = 0;
        pool->queue_max_size = queue_max_size;
        pool->shutdown = 0;

        if (NULL == (pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * max_thr_num)))
        {
            fprintf(stderr, "malloc threads false;\n");
            break;
        }

        memset(pool->threads, 0, sizeof(pthread_t) * max_thr_num);

        if (NULL == (pool->task_queue = (threadPool_Task_t *)malloc(sizeof(threadPool_Task_t) * queue_max_size)))
        {
            fprintf(stderr, "malloc task_queue false;\n");
            break;
        }

        for (int i = 0; i < queue_max_size; ++i)
        {
            memset(pool->task_queue + i, 0, sizeof(threadPool_Task_t));
        }

        if (pthread_mutex_init(AddrOffset(pool, threadPool_t, lock), NULL) != 0 ||
            pthread_mutex_init(AddrOffset(pool, threadPool_t, thread_counter), NULL) != 0 ||
            pthread_cond_init(AddrOffset(pool, threadPool_t, queue_not_full), NULL) != 0 ||
            pthread_cond_init(AddrOffset(pool, threadPool_t, queue_not_empty), NULL) != 0)
        {
            fprintf(stderr, "init lock or cond false;\n");
            break;
        }

        for (int i = 0; i < min_thr_num; i++)
        {
            pthread_create(pool->threads + i, NULL, threadPool_thread, pool);
            fprintf(stdout, "starting thread 0x%x\n", pool->threads[i]);
        }

        pthread_create(AddrOffset(pool, threadPool_t, admin_tid), NULL, threadPool_admin_thread, pool);
        return pool;

    } while (0);

    threadPool_free(pool);
    return NULL;
}

int threadPool_free(void *threadPool)
{
    if (NULL == threadPool)
    {
        return -1;
    }

    threadPool_t *pool = (threadPool_t *)(threadPool);

    if (NULL != pool->task_queue)
    {
        free(pool->task_queue);
    }

    if (NULL != pool->threads)
    {
        free(pool->threads);

        pthread_mutex_lock(AddrOffset(pool, threadPool_t, lock));

        pthread_mutex_destroy(AddrOffset(pool, threadPool_t, lock));

        pthread_mutex_lock(AddrOffset(pool, threadPool_t, thread_counter));

        pthread_mutex_destroy(AddrOffset(pool, threadPool_t, thread_counter));

        pthread_cond_destroy(AddrOffset(pool, threadPool_t, queue_not_empty));

        pthread_cond_destroy(AddrOffset(pool, threadPool_t, queue_not_full));
    }

    free(pool);

    return 0;
}

int threadPool_destroy(void *threadPool)
{
    if (NULL == threadPool)
    {
        return -1;
    }

    threadPool_t *pool = (threadPool_t *)(threadPool);

    pool->shutdown = 1;

    pthread_join(pool->admin_tid, NULL);

    for (int i = 0; i < pool->live_thr_num; ++i)
    {
        pthread_cond_broadcast(AddrOffset(pool, threadPool_t, queue_not_empty));
    }

    for (int i = 0; i < pool->live_thr_num; ++i)
    {
        pthread_join(pool->threads[i], NULL);
    }

    return threadPool_free(threadPool);
}

int is_thread_alive(pthread_t tid)
{
    return pthread_kill(tid, 0) == ESRCH ? 0 : 1;
}

void *threadPool_thread(void *threadPool)
{
    threadPool_t *pool = (threadPool_t *)threadPool;

    for (;;)
    {
        pthread_mutex_lock(AddrOffset(pool, threadPool_t, lock));

        while ((0 == pool->queue_size) && (!pool->shutdown))
        {
            fprintf(stdout, "thread 0x%x is waiting\n", pthread_self());
            pthread_cond_wait(AddrOffset(pool, threadPool_t, queue_not_empty), AddrOffset(pool, threadPool_t, lock));
            if (pool->wait_exit_thr_num > 0)
            {
                pool->wait_exit_thr_num--;
                if (pool->live_thr_num > pool->min_thr_num)
                {
                    fprintf(stdout, "thread 0x%x is exiting\n", pthread_self());
                    pool->live_thr_num--;
                    pthread_mutex_unlock(AddrOffset(pool, threadPool_t, lock));
                    pthread_exit(NULL);
                }
            }
        }

        if (pool->shutdown)
        {
            pthread_mutex_unlock(AddrOffset(pool, threadPool_t, lock));
            fprintf(stdout, "thread 0x%x is exiting\n", pthread_self());
            pthread_exit(NULL);
        }

        threadPool_Task_t task;
        task.arg = pool->task_queue[pool->queue_front].arg;
        task.func = pool->task_queue[pool->queue_front].func;
        pool->queue_front = (pool->queue_front + 1) % pool->queue_max_size;

        pool->queue_size--;

        pthread_cond_broadcast(AddrOffset(pool, threadPool_t, queue_not_full));

        pthread_mutex_unlock(AddrOffset(pool, threadPool_t, lock));

        pthread_mutex_lock(AddrOffset(pool, threadPool_t, thread_counter));

        pool->busy_thr_num++;

        pthread_mutex_unlock(AddrOffset(pool, threadPool_t, thread_counter));

        fprintf(stdout, "thread 0x%x start working\n", pthread_self());

        (*(task.func))(task.arg);

        fprintf(stdout, "thread 0x%x end working\n", pthread_self());

        pthread_mutex_lock(AddrOffset(pool, threadPool_t, thread_counter));

        pool->busy_thr_num--;

        pthread_mutex_unlock(AddrOffset(pool, threadPool_t, thread_counter));
    }

    pthread_exit(NULL);
}

void *threadPool_admin_thread(void *threadPool)
{
    threadPool_t *pool = (threadPool_t *)(threadPool);

    while (!pool->shutdown)
    {
        fprintf(stdout, "admin ---------------------\n");

        Sleep(DEFAULT_TIME);

        pthread_mutex_lock(AddrOffset(pool, threadPool_t, lock));

        int queue_size = pool->queue_size;

        int live_thr_num = pool->live_thr_num;

        pthread_mutex_unlock(AddrOffset(pool, threadPool_t, lock));

        pthread_mutex_lock(AddrOffset(pool, threadPool_t, thread_counter));

        int busy_thr_num = pool->busy_thr_num;

        pthread_mutex_unlock(AddrOffset(pool, threadPool_t, thread_counter));

        fprintf(stdout, "admin busy live -%d--%d-\n", busy_thr_num, live_thr_num);

        if ((queue_size >= MIN_WAIT_TASK_NUM) && (live_thr_num < pool->max_thr_num))
        {
            fprintf(stdout, "admin add thread ------------\n");

            pthread_mutex_lock(AddrOffset(pool, threadPool_t, lock));

            for (int i = 0, add = 0; (i < pool->max_thr_num) && (add < DEFAULT_THREAD_NUM) && (pool->live_thr_num < pool->max_thr_num); ++i)
            {
                if ((0 == pool->threads[i]) || (!is_thread_alive(pool->threads[i])))
                {
                    pthread_create(pool->threads + i, NULL, threadPool_thread, threadPool);
                    fprintf(stdout, "new thread 0x%x -----------\n", pool->threads[i]);
                    add++;
                    pool->live_thr_num++;
                }
            }

            pthread_mutex_unlock(AddrOffset(pool, threadPool_t, lock));
        }

        if (((busy_thr_num << 1) < live_thr_num) && (live_thr_num > pool->min_thr_num))
        {
            pthread_mutex_lock(AddrOffset(pool, threadPool_t, lock));

            pool->wait_exit_thr_num = DEFAULT_THREAD_NUM;

            pthread_mutex_unlock(AddrOffset(pool, threadPool_t, lock));

            for (int i = 0; i < DEFAULT_THREAD_NUM; ++i)
            {
                fprintf(stdout, "admin clear -------\n");

                pthread_cond_signal(AddrOffset(pool, threadPool_t, queue_not_empty));
            }
        }
    }

    pthread_exit(NULL);
}

int threadPool_add_task(void *threadPool, pFunc function, void *arg)
{
    threadPool_t *pool = (threadPool_t *)(threadPool);

    pthread_mutex_lock(AddrOffset(pool, threadPool_t, lock));

    while ((pool->queue_size == pool->queue_max_size) && (!pool->shutdown))
    {
        pthread_cond_wait(AddrOffset(pool, threadPool_t, queue_not_full), AddrOffset(pool, threadPool_t, lock));
    }

    if (pool->shutdown)
    {
        pthread_mutex_unlock(AddrOffset(pool, threadPool_t, lock));
        return -1;
    }

    if (NULL != pool->task_queue[pool->queue_rear].arg)
    {
        free(pool->task_queue[pool->queue_rear].arg);
    }

    pool->task_queue[pool->queue_rear].arg = arg;
    pool->task_queue[pool->queue_rear].func = function;
    pool->queue_rear = (pool->queue_rear + 1) % pool->queue_max_size;

    pool->queue_size++;

    pthread_cond_signal(AddrOffset(pool, threadPool_t, queue_not_empty));
    pthread_mutex_unlock(AddrOffset(pool, threadPool_t, lock));

    return 0;
}