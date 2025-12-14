#include "threadpool.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// 任务结构体
typedef struct Task
{
	void (*function)(void* arg);
	void* arg;
}Task;

// 线程池结构体
struct ThreadPool
{
	Task* taskQ;			// 任务队列

	int queueCapacity;		// 容量
	int queueSize;			// 当前任务个数
	int queueFront;			// 对头 -> 取数据
	int queueRear;			// 队尾 -> 放数据

	pthread_t managerID;	// 管理者线程ID
	pthread_t *threadIDs;	// 工作者线程ID
	int minNum;				// 最小线程数量
	int maxNum;				// 最大线程数量
	int busyNum;			// 忙的线程的个数
	int liveNum;			// 存活的线程个数
	int exitNum;			// 要销毁的线程个数

	pthread_mutex_t mutexPool;	// 锁整个的线程池
	pthread_mutex_t mutexBusy;	// 锁busyNum变量
	pthread_cond_t notFull;		// 任务队列是否满
	pthread_cond_t notEmpty;	// 任务队列是否空

	int shutdown;			// 是否销毁线程池, 1为销毁，0为不销毁
};

ThreadPool* threadPoolCreate(int min, int max, int queueSize)
{
	ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
	do
	{
		if (pool == NULL)
		{
			perror("malloc threadpool failed");
			break;
		}

		pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max);
		if (pool->threadIDs == NULL)
		{
			perror("malloc threadIDs fail");
			break;
		}
		memset(pool->threadIDs, 0, sizeof(pthread_t)*max);
		pool->minNum = min;
		pool->maxNum = max;
		pool->busyNum = 0;
		pool->liveNum = min; // 和最小个数相等
		pool->exitNum = 0;

		if( pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
			pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
			pthread_cond_init(&pool->notEmpty, NULL) != 0 ||
			pthread_cond_init(&pool->notFull, NULL) != 0)
		{
			perror("mutex or condition init fail");
			break;
		}

		// 任务队列
		pool->taskQ = (Task*)malloc(sizeof(Task) * queueSize);
		pool->queueCapacity = queueSize;
		pool->queueSize = 0;
		pool->queueFront = 0;
		pool->queueRear = 0;

		pool->shutdown = 0;

		// 创建线程
		pthread_create(pool->managerID, NULL, manager, pool);
		for (int i = 0; i < min; i++)
		{
			pthread_create(pool->threadIDs, NULL, worker, pool);
		}
		return pool;
	} while (0);

	if (pool && pool->threadIDs) free(pool->threadIDs);
	if (pool && pool->taskQ) free(pool->taskQ);
	if (pool) free(pool);

	return NULL;
}

int threadPoolDestroy(ThreadPool* pool)
{
	if (pool == NULL)
	{
		return -1;
	}

}

void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg)
{
}

int threadPoolBusyNum(ThreadPool* pool)
{
	return 0;
}

int threadPoolAliveNum(ThreadPool* pool)
{
	return 0;
}

void* worker(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;
	while (1)
	{
		pthread_mutex_lock(&pool->mutexPool);
		// 当前任务队列是否为空
		while (pool->queueSize == 0 && !pool->shutdown)
		{
			// 阻塞工作线程
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

			// 判断是否要销毁线程
			if (pool->exitNum > 0)
			{
				pool->exitNum--;
				if (pool->liveNum > pool->minNum)
				{
					pool->liveNum--;
					pthread_mutex_unlock(&pool->mutexPool);
					threadExit(pool);
				}
			}
		}
	}
	return NULL;
}

void* manager(void* arg)
{
	return NULL;
}

void threadExit(ThreadPool* pool)
{
	pthread_t tid = pthread_self();
	for (int i = 0; i < pool->maxNum; i++)
	{
		if (pthread_equal(pool->threadIDs[i], tid)
		{
			pool.
		}
	}
}
