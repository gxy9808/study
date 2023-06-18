#include "base/ThreadPool.h"
#include "base/Logging.h"
#include "base/New.h"

ThreadPool* ThreadPool::createNew(int num)
{
    //return new ThreadPool(num);
    return New<ThreadPool>::allocate(num);
}

ThreadPool::ThreadPool(int num) :
    mThreads(num),
    mQuit(false)
{
    mMutex = Mutex::createNew();
    mCondition = Condition::createNew();

    createThreads();
}

ThreadPool::~ThreadPool()
{
    cancelThreads();
    //delete mMutex;
    //delete mCondition;
    Delete::release(mMutex);
    Delete::release(mCondition);
}

void ThreadPool::addTask(ThreadPool::Task& task)
{
    MutexLockGuard mutexLockGuard(mMutex);
    mTaskQueue.push(task);  // 当前任务加入到任务队列中
    mCondition->signal();
}

void ThreadPool::handleTask()
{
    while(mQuit != true)
    {
        Task task;
        {
            MutexLockGuard mutexLockGuard(mMutex);
            if(mTaskQueue.empty())   // 如果任务队列为空， 阻塞
                mCondition->wait(mMutex);
        
            if(mQuit == true)
                break;

            if(mTaskQueue.empty())
                continue;

            // 队列中取出一个任务
            task = mTaskQueue.front();

            mTaskQueue.pop(); // 出队列
        }

        task.handle();
    }
}

void ThreadPool::createThreads()
{
    MutexLockGuard mutexLockGuard(mMutex);

    for(std::vector<MThread>::iterator it = mThreads.begin(); it != mThreads.end(); ++it)
        (*it).start(this);
}

void ThreadPool::cancelThreads()
{
    MutexLockGuard mutexLockGuard(mMutex);

    mQuit = true;
    mCondition->broadcast();
    for(std::vector<MThread>::iterator it = mThreads.begin(); it != mThreads.end(); ++it)
        (*it).join();

    mThreads.clear();
}

void ThreadPool::MThread::run(void* arg)
{
    ThreadPool* threadPool = (ThreadPool*)arg;
    threadPool->handleTask();    
}