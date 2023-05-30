#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "locker.h"
using namespace std;


// 阻塞队列类: 封装了生产者-消费者模型  push成员是生产者，pop成员是消费者
template <class T>
class block_queue
{
public:
    // 构造函数，初始化私有成员
    block_queue(int max_size = 1000)
    {
        if(max_size <= 0)
            exit(-1);
        
        // 构造函数创建循环数组
        m_max_size = max_size;
        m_array = new T[max_size];
        m_size = 0;
        m_front = -1;
        m_back = -1;

    }

    ~block_queue()
    {
        m_mutex.lock();
        if(m_array != NULL)
            delete [] m_array;

        m_mutex.unlock();
    }

    void clear()
    {
        m_mutex.lock();
        m_size = 0;
        m_front = -1;
        m_back = -1;
        m_mutex.unlock();
    }

    // 判断队列是否满
    bool full()
    {
        m_mutex.lock();
        if(m_size >= m_max_size)
        {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    // 判断队列是否为空
    bool empty()
    {
        m_mutex.lock();
        if(m_size == 0)
        {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }
    // 返回队首元素
    bool front(T &value)
    {
        m_mutex.lock();
        if(m_size == 0)
        {
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_front];
        m_mutex.unlock();
        return true;
    }

    // 返回队尾元素
    bool back(T &value)
    {
        m_mutex.lock();
        if(m_size == 0)
        {
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_back];
        m_mutex.unlock();
        return true;
    }

    // 返回队列当前长度
    int size()
    {
        int tmp = 0;
        m_mutex.lock();
        tmp = m_size;

        m_mutex.unlock();
        return tmp;
    }

    // 返回队列的最大长度
    int max_size()
    {
        int tmp = 0;
        m_mutex.lock();
        tmp = m_max_size;

        m_mutex.unlock();
        return tmp;
    }

    // 往队列添加元素，需要将所有使用队列的线程先唤醒
    // 当有元素push进队列，相当于生产者生产了一个元素；若当前没有线程等待条件变量,则唤醒无意义
    bool push(const T &item)
    {
        m_mutex.lock();
        if (m_size >= m_max_size)
        {
            m_cond.broadcast();  // 以广播的方式唤醒线程
            m_mutex.unlock();
            return false;
        }

        // 将新增数据放在循环数组的对应位置
        m_back = (m_back + 1) % m_max_size;
        m_array[m_back] = item;
        m_size++;

        m_cond.broadcast();  // 唤醒线程
        m_mutex.unlock();

        return true;
    }

    // pop时，如果当前队列没有元素,将会等待条件变量
    bool pop(T& item)
    {
        m_mutex.lock();
        // 多个消费者的时候，这里要是用while而不是if
        while (m_size <= 0)
        {
            // 当重新抢到互斥锁，pthread_cond_wait返回为0
            if(!m_cond.wait(m_mutex.get()))
            {
                m_mutex.unlock();
                return false;
            }
        }

        // 取出队列的首元素
        m_front = (m_front+1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
        
    }
    // ps:增加了超时处理， 在pthread_cond_wait基础上增加了等待的时间，只指定时间内能抢到互斥锁即可
    bool pop(T& item, int ms_timeout)
    {
        struct timespec t = {0, 0};
        struct timeval now = {0, 0};

        gettimeofday(&now, NULL);
        m_mutex.lock();
        if (m_size <= 0)
        {
            t.tv_sec = now.tv_sec + ms_timeout /1000;
            t.tv_nsec = (ms_timeout % 1000) * 1000;
            if (!m_cond.timewait(m_mutex.get(), t))
            {
                m_mutex.unlock();
                return false;
            }
        }

        if(m_size <= 0)
        {
            m_mutex.unlock();
            return false;
        }

        if(m_size <= 0)
        {
            m_mutex.unlock();
            return false;
        }
        
        // 取出队列的首元素
        m_front = (m_front+1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }

private:
    locker m_mutex;  // 互斥锁
    cond m_cond;     // 条件变量

    T *m_array;      // 循环数组（用于实现阻塞队列）
    int m_size;      // 阻塞队列的当前大小
    int m_max_size;  // 阻塞队列的最大长度
    int m_front;
    int m_back;

};

#endif