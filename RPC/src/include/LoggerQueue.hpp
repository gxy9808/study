#pragma once

#include <queue>
#include <mutex>
#include <thread>    // 对应 pthread_mutex_t
#include <condition_variable>   // 对应 pthread_condition_t

using namespace std;

// 异步写日志的日志队列
template <class T>
class LoggerQueue
{
public:
    // 多个worker线程都会写日志queue
    void push(const T &data) 
    {
        lock_guard<mutex> lock(mutex_);  // 加锁
        queue_.push(data);
        condition_.notify_one();  // 条件变量唤醒其他线程来写日志
    }  // 出中括号，把锁释放掉

    // 一个线程读日志queue，写日志文件
    T pop()  
    {
        unique_lock<mutex> lock(mutex_);
        while (queue_.empty())
        {
            //日志队列为空，线程进入wait（并把持有的锁释放）
            condition_.wait(lock);  
        }
        T ret = queue_.front();
        queue_.pop();
        return ret;
    }

private:
    queue<T> queue_;
    mutex mutex_;
    condition_variable condition_;   // 条件变量
};