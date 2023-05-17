#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <exception> //异常
#include <semaphore.h>


using namespace std;

//线程同步机制封装类

// 互斥锁类 
class locker {
public:
    locker() {
        if(pthread_mutex_init(&m_mutex, NULL) !=0) {
            throw exception();
        }
    }

    ~locker() {
        pthread_mutex_destroy(&m_mutex);
    }

    // 上锁
    bool lock() {
        return pthread_mutex_lock(&m_mutex)==0;
    }
    // 解锁
    bool unlock() {
        return pthread_mutex_unlock(&m_mutex)==0;
    }

    // 获取互斥量成员
    pthread_mutex_t* get(){
        return &m_mutex;
    }
    
private:
    pthread_mutex_t m_mutex;  // 互斥量（锁）（互斥锁能保证线程安全）

};

//条件变量类
class cond
{
public:
    cond() {
        if(pthread_cond_init(&m_cond, NULL)!=0)
            throw exception();
    }
    ~cond() {
        pthread_cond_destroy(&m_cond);
    }

    // pthread_cond_wait函数：当函数被调用发生阻塞的时候，会对互斥锁进行解锁；当调用完解除阻塞时，会继续执行 重新加锁
    bool wait(pthread_mutex_t * mutex) { 
        return pthread_cond_wait(&m_cond, mutex) == 0;
    }

    bool timewait(pthread_mutex_t * mutex, struct timespec t) {
        return pthread_cond_timedwait(&m_cond, mutex, &t) == 0;
    }

    // 让条件变量增加，让一个或多个线程唤醒
    bool signal() {
        return pthread_cond_signal(&m_cond) ==0;
    }

    // broadcast 将所有线程唤醒
    bool broadcast() {
        return pthread_cond_broadcast(&m_cond) ==0;
    }
private:
    pthread_cond_t m_cond;  // 条件变量
};

// 信号量类
class sem{
public:
    sem() {
        if(sem_init(&m_sem, 0, 0)!=0){  // 第二个参数： 0 用在线程， 非0 用在进程
            throw exception();
        }
    }

    sem(int num) {
        if(sem_init(&m_sem, 0, num)!=0){
            throw exception();
        }
    }

    ~sem() {
        sem_destroy(&m_sem);
    }

    // 等待信号量
    bool wait() {
        return sem_wait(&m_sem)==0;  // 对信号量加锁，调用一次对信号量的值-1
    }

    // 增加信号量
    bool post() {
        return sem_post(&m_sem)==0;  // 对信号量解锁（唤醒），调用一次对信号量的值+1
    }

private:
    sem_t m_sem;
};





#endif
