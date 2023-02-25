#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "threadPool.h"

typedef struct
{
    int a;
    int b;
} Param_t;

#define ELEM_SIZE(a) (sizeof((a)) / sizeof((*(a))))

void *do_add(void *p)
{
    Param_t *param = (Param_t *)(p);

    fprintf(stdout, "do add %d + %d = %d\n", param->a, param->b, param->a + param->b);

    return NULL;
}

void *do_sub(void *p)
{
    Param_t *param = (Param_t *)(p);

    fprintf(stdout, "do sub %d - %d = %d\n", param->a, param->b, param->a - param->b);

    return NULL;
}

void *do_mul(void *p)
{
    Param_t *param = (Param_t *)(p);

    fprintf(stdout, "do mul %d * %d = %d\n", param->a, param->b, param->a * param->b);

    return NULL;
}

void *do_div(void *p)
{
    Param_t *param = (Param_t *)(p);

    if (param->b)
    {
        fprintf(stdout, "do div %d / %d = %f\n", param->a, param->b, (float)(param->a) / (float)(param->b));
    }
    else
    {
        fprintf(stdout, "Error divisor cannot be 0\n");
    }

    return NULL;
}

void *do_work(void *threadPool)
{
    threadPool_t *pool = (threadPool_t *)(threadPool);

    pFunc array[] = {do_add, do_sub, do_mul, do_div};

    while (100 != rand())
    {
        Param_t *param = (Param_t *)(malloc(sizeof(Param_t)));

        param->a = rand();

        param->b = rand();

        threadPool_add_task(pool, array[rand() % ELEM_SIZE(array)], param);
    }

    threadPool_destroy(threadPool);

    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    srand((unsigned)time(NULL));

    threadPool_t *pool = threadPool_create(10, 100, 100);

    pthread_t pid = 0;

    pthread_create(&pid, NULL, do_work, pool);

    return pthread_join(pid, NULL);
}