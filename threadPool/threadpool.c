#include "threadpool.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define NUMBER 2 // 每次创建和销毁的线程个数

// 内部函数声明
static void* worker(void* arg);
static void* manager(void* arg);
static void threadExit(ThreadPool* pool);

// 任务队列结构体
typedef struct Task
{
	void (*function)(void* arg);
	void* arg;
} Task;

// 线程池结构体
struct ThreadPool
{
	Task* taskQ; // 任务队列

	int queueCapacity; // 容量
	int queueSize;	   // 当前任务个数
	int queueFront;	   // 队头 -> 取数据
	int queueRear;	   // 队尾 -> 放数据

	pthread_t managerID;  // 管理者线程ID
	pthread_t* threadIDs; // 工作者线程ID
	int minNum;			  // 最小线程数量
	int maxNum;			  // 最大线程数量
	int busyNum;		  // 忙线程的个数
	int liveNum;		  // 存活线程的个数
	int exitNum;		  // 要销毁的线程个数

	pthread_mutex_t mutexPool; // 锁整个的线程池
	pthread_mutex_t mutexBusy; // 只锁 busyNum 变量
	pthread_cond_t notFull;	   // 任务队列是否满，控制生产者的条件变量
	pthread_cond_t notEmpty;   // 任务队列是否空，控制消费者的条件变量

	int shutdown; // 是否销毁线程池, 1为销毁，0为不销毁
};

ThreadPool* threadPoolCreate(int min, int max, int queueSize)
{
	ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
	/*这里的do while 用得很精髓，只要有一步失败就跳出循环，统一释放资源*/
	do
	{
		if (pool == NULL)
		{
			perror("malloc threadpool failed");
			break;
		}

		pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * (size_t)max);
		if (pool->threadIDs == NULL)
		{
			perror("malloc threadIDs fail");
			break;
		}
		memset(pool->threadIDs, 0, sizeof(pthread_t) * (size_t)max);
		pool->minNum = min;
		pool->maxNum = max;
		pool->busyNum = 0;
		pool->liveNum = min; // 和最小个数相等
		pool->exitNum = 0;

		if (pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
			pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
			pthread_cond_init(&pool->notEmpty, NULL) != 0 ||
			pthread_cond_init(&pool->notFull, NULL) != 0)
		{
			perror("mutex or condition init fail");
			break;
		}

		// 任务队列
		pool->taskQ = (Task*)malloc(sizeof(Task) * (size_t)queueSize);
		pool->queueCapacity = queueSize;
		pool->queueSize = 0;
		pool->queueFront = 0;
		pool->queueRear = 0;

		pool->shutdown = 0;

		// 创建管理者线程
		pthread_create(&pool->managerID, NULL, manager, pool);
		// 创建min个工作线程
		for (int i = 0; i < min; i++)
		{
			pthread_create(&pool->threadIDs[i], NULL, worker, pool);
		}
		return pool;
	} while (0);

	// 出现异常统一释放资源，先释放内部成员，再释放容器
	if (pool && pool->threadIDs)
		free(pool->threadIDs);
	if (pool && pool->taskQ)
		free(pool->taskQ);
	if (pool)
		free(pool);

	return NULL;
}

int threadPoolDestroy(ThreadPool* pool)
{
	if (pool == NULL)
	{
		return -1;
	}

	// 关闭线程池
	pool->shutdown = 1;
	// 阻塞回收管理者线程
	pthread_join(pool->managerID, NULL);
	// 唤醒阻塞的消费者线程
	for (int i = 0; i < pool->liveNum; i++)
	{
		pthread_cond_signal(&pool->notEmpty);
	}

	// 回收工作线程资源
	if (pool->threadIDs)
	{
		for (int i = 0; i < pool->maxNum; i++)
		{
			if (pool->threadIDs[i] != 0)
			{
				pthread_join(pool->threadIDs[i], NULL);
			}
		}
	}

	// 释放堆内存
	if (pool->taskQ)
	{
		free(pool->taskQ);
		pool->taskQ = NULL;
	}
	if (pool->threadIDs)
	{
		free(pool->threadIDs);
		pool->threadIDs = NULL;
	}

	pthread_mutex_destroy(&pool->mutexPool);
	pthread_mutex_destroy(&pool->mutexBusy);
	pthread_cond_destroy(&pool->notEmpty);
	pthread_cond_destroy(&pool->notFull);

	free(pool);

	return 0;
}

void threadPoolAdd(ThreadPool* pool, void (*func)(void*), void* arg)
{
	pthread_mutex_lock(&pool->mutexPool);
	while (pool->queueSize == pool->queueCapacity && !pool->shutdown)
	{
		// 阻塞生产者线程
		pthread_cond_wait(&pool->notFull, &pool->mutexPool);
	}
	if (pool->shutdown)
	{
		pthread_mutex_unlock(&pool->mutexPool);
		return;
	}
	// 添加任务
	// 环形队列逻辑
	pool->taskQ[pool->queueRear].function = func;
	pool->taskQ[pool->queueRear].arg = arg;
	pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
	pool->queueSize++;

	pthread_cond_signal(&pool->notEmpty);
	pthread_mutex_unlock(&pool->mutexPool);
}

int threadPoolBusyNum(ThreadPool* pool)
{
	pthread_mutex_lock(&pool->mutexBusy);
	int busyNum = pool->busyNum;
	pthread_mutex_unlock(&pool->mutexBusy);

	return busyNum;
}

int threadPoolAliveNum(ThreadPool* pool)
{
	pthread_mutex_lock(&pool->mutexPool);
	int liveNum = pool->liveNum;
	pthread_mutex_unlock(&pool->mutexPool);

	return liveNum;
}

/**
 * @brief 消费者线程任务函数
 *
 * 循环从任务队列中取出任务并执行。
 * 当队列为空时阻塞等待，当收到销毁信号或退出信号时结束线程。
 *
 * @param arg 线程池结构体指针 (ThreadPool*)
 * @return void* 线程结束返回NULL
 */
static void* worker(void* arg)
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

		// 判断线程池是否被关闭
		if (pool->shutdown)
		{
			pthread_mutex_unlock(&pool->mutexPool);
			threadExit(pool);
		}

		// 从任务队列中取出一个任务
		Task task;
		task.function = pool->taskQ[pool->queueFront].function;
		task.arg = pool->taskQ[pool->queueFront].arg;

		// 移动头节点
		// 环形队列逻辑
		pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
		pool->queueSize--;
		// 解锁，通知生产者
		pthread_cond_signal(&pool->notFull);
		pthread_mutex_unlock(&pool->mutexPool);

		printf("thread %ld start working...\n", pthread_self());

		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexBusy);

		if (task.function)
		{
			task.function(task.arg);
		}

		free(task.arg);
		task.arg = NULL;

		printf("thread %ld end working...\n", pthread_self());

		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexBusy);
	}
	return NULL;
}

/**
 * @brief 管理者线程任务函数
 *
 * 周期性检测线程池状态，动态调整线程数量。
 * - 当任务堆积过多时，增加工作线程。
 * - 当空闲线程过多时，减少工作线程。
 *
 * @param arg 线程池结构体指针 (ThreadPool*)
 * @return void* 线程结束返回NULL
 */
static void* manager(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;
	while (!pool->shutdown)
	{
		// 每隔3s检测一次
		sleep(3);

		// 取出线程池中任务的数量和当前线程的数量
		pthread_mutex_lock(&pool->mutexPool);
		int queueSize = pool->queueSize;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&pool->mutexPool);

		// 添加线程
		// 任务的个数> 存活的线程个数 && 存活的线程数< 最大线程数
		if (queueSize > liveNum && liveNum < pool->maxNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			int counter = 0;
			for (int i = 0; i < pool->maxNum && counter < NUMBER && pool->liveNum < pool->maxNum; i++)
			{
				if (pool->threadIDs[i] == 0)
				{
					pthread_create(&pool->threadIDs[i], NULL, worker, pool);
					counter++;
					pool->liveNum++;
				}
			}
			pthread_mutex_unlock(&pool->mutexPool);
		}

		// 销毁线程，一次销毁NUMBER个线程
		// 忙的线程*2 < 存活的线程 && 存活的线程 > 最小线程数
		if (pool->busyNum * 2 < pool->liveNum && pool->liveNum > pool->minNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool);
			// 让工作线程自杀
			for (int i = 0; i < NUMBER; i++)
			{
				pthread_cond_signal(&pool->notEmpty);
			}
		}
	}

	return NULL;
}

/**
 * @brief 线程退出清理函数
 *
 * 将当前线程ID从线程池的threadIDs数组中移除（置为0），
 * 并调用pthread_exit结束当前线程。
 *
 * @param pool 线程池结构体指针
 */
static void threadExit(ThreadPool* pool)
{
	pthread_t tid = pthread_self();
	for (int i = 0; i < pool->maxNum; i++)
	{
		if (pthread_equal(pool->threadIDs[i], tid))
		{
			pool->threadIDs[i] = 0;
			printf("threadExit() called, %ld exiting...\n", tid);
			break;
		}
	}
	pthread_exit(NULL);
}
