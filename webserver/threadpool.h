
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <pthread.h>
#include <exception>
#include <cstdio>
#include "locker.h"
#include "sql_connection_pool.h"

using namespace std;

// 线程池类，定义成模板类，方便代码复用，模板参数T是任务类
template<typename T>
class threadpool
{
public:
    // 构造函数
    threadpool(connection_pool *connPool, int thread_number = 8, int max_requests = 1000);

    // 析构函数
    ~threadpool();
    
    // //主线程往队列中添加任务
    bool append(T* request);

private:
    static void* worker(void* arg); // 子线程的工作函数
    void run(); // 线程池运行函数

private:
    int m_thread_number;  // 线程的数量

    pthread_t * m_threads; // 线程池数组，大小为m_thread_number

    // 请求队列中最多允许的等待处理的请求数量
    int m_max_requests;

    // 请求队列
    list<T*> m_workqueue; // 工作队列所有线程共享

    // 互斥锁 ：保护请求队列的互斥锁，因为线程们（包括主线程）共享这个请求队列，从这个队列取任务，所以要线程同步，保证线程的独占式访问
    locker m_queuelocker;
    
    // 信号量： 用来判断是由有任务需要处理
    sem m_queuestat;

    connection_pool *m_connPool;  //数据库

    // 是否结束线程
    bool m_stop;
};

template<typename T>
threadpool<T>::threadpool(connection_pool *connPool, int thread_number, int max_requests):
    m_connPool(connPool), m_thread_number(thread_number),m_max_requests(max_requests),
    m_stop(false), m_threads(NULL) {
    // 线程数量和最大请求量不能为0
    if((thread_number<=0 || (max_requests <= 0)))
        throw exception();
    // 创建线程池数组
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads) throw exception();

    // 创建thread_number个子线程，并设置为线程脱离
    for (int i = 0; i < thread_number; i++)
    {
        printf("create the %dth thread\n", i);
        // 创建第i个线程失败
        if(pthread_create(m_threads + i, NULL, worker, this)!=0){ // c++中worker必须为静态函数
            delete []m_threads;
            throw exception();
        }
        // 设置线程脱离
        if(pthread_detach(m_threads[i])){
            delete []m_threads;
            throw exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool() {

    delete[] m_threads;
    m_stop = true;  // 停止线程
}

// 主线程向请求队列中添加任务
template<typename T>
bool threadpool<T>::append(T* request) { 
    // 上锁 ：主线程添加请求队列，此时其他线程不能操作队列
    m_queuelocker.lock();
    if(m_workqueue.size() > m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    // 请求队列中添加新的任务请求
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post(); // 信号量增加（ 通知子线程来任务了）
    return true;
}

// 子线程的工作函数
template<typename T>
void* threadpool<T>::worker(void* arg) {

    threadpool* pool = (threadpool *)arg;
    pool->run();
    return pool;
}

//子线程操作请求队列（获取）,并工作
template<typename T>
void threadpool<T>::run() {  
    while (!m_stop){
        // 从队列中取任务（工作队列为空就等待挂起，工作队列不为空才能获取任务）
        m_queuestat.wait(); // 如果有信号量，则不阻塞，值-1； 没有则阻塞等待
        m_queuelocker.lock(); //获取任务时，要使用互斥锁，保证对资源的独占式访问
        // 任务队列为空
        if(m_workqueue.empty()){ 
            m_queuelocker.unlock();
            continue;
        }
        // 取出任务
        T* request = m_workqueue.front();
        m_workqueue.pop_front();  // 移出队列
        m_queuelocker.unlock();
        if(!request) continue;  // 没有获取到任务

        //**********处理http请求的入口函数************************
        //**********执行任务（工作），这里不用锁，并发执行**********
        connectionRAII mysqlcon(&request->mysql, m_connPool);
        request->process(); 
        
    }
}
#endif 

/* 
单生产者多消费者的生产者消费者模型。
主线程是生产者，线程池中的子线程是消费者，互斥锁是解决互斥问题的，信号量是解决同步问题的。
互斥关系体现在：任务请求队列其实是共享资源，主线程将新任务加入到队列中这一操作和若干个子线程从队列中取数据的操作是互斥的。
同步体现在：当任务队列中没有任务时，必须主线程先放入任务，再子线程取任务。
主线程append的时候进行了post操作，其实就是执行了同步信号量的V操作，而子线程的wait就是执行了P操作 */

