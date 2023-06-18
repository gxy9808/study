#ifndef _USAGEENVIRONMENT_H_
#define _USAGEENVIRONMENT_H_
#include "net/EventScheduler.h"
#include "base/ThreadPool.h"

// RTSP服务器的环境配置
class UsageEnvironment
{
public:
    static UsageEnvironment* createNew(EventScheduler* scheduler, ThreadPool* threadPool);
    
    // 环境配置初始化，封装事件调度器和线程池
    UsageEnvironment(EventScheduler* scheduler, ThreadPool* threadPool);
    ~UsageEnvironment();

    EventScheduler* scheduler();   // 事件调度器
    ThreadPool* threadPool();      // 线程池

private:
    EventScheduler* mScheduler;
    ThreadPool* mThreadPool;
};

#endif //_USAGEENVIRONMENT_H_