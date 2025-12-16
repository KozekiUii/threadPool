#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#ifdef __cplusplus
extern "C"
{
#endif

    // 定义线程池结构体（不透明指针，隐藏实现细节）
    typedef struct ThreadPool ThreadPool;

    /**
     * @brief 创建线程池并初始化
     * @param min 最小线程数
     * @param max 最大线程数
     * @param queueSize 任务队列容量
     * @return ThreadPool* 成功返回线程池指针，失败返回 NULL
     */
    ThreadPool* threadPoolCreate(int min, int max, int queueSize);

    /**
     * @brief 销毁线程池
     * @param pool 线程池指针
     * @return int 成功返回 0，失败返回 -1
     */
    int threadPoolDestroy(ThreadPool* pool);

    /**
     * @brief 给线程池添加任务
     * @param pool 线程池指针
     * @param func 任务函数指针
     * @param arg 任务函数参数
     */
    void threadPoolAdd(ThreadPool* pool, void (*func)(void*), void* arg);

    /**
     * @brief 获取线程池中工作的线程的个数
     * @param pool 线程池指针
     * @return int 忙线程数量
     */
    int threadPoolBusyNum(ThreadPool* pool);

    /**
     * @brief 获取线程池中活着的线程的个数
     * @param pool 线程池指针
     * @return int 存活线程数量
     */
    int threadPoolAliveNum(ThreadPool* pool);

#ifdef __cplusplus
}
#endif

#endif // _THREADPOOL_H