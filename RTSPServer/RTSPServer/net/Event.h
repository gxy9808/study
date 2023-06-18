#ifndef _EVENT_H_
#define _EVENT_H_

#include "base/Logging.h"

typedef void (*EventCallback)(void*);

// 触发事件类
class TriggerEvent  
{
public:
    static TriggerEvent* createNew(void* arg);
    static TriggerEvent* createNew();

    TriggerEvent(void* arg);
    ~TriggerEvent() {  };

    void setArg(void* arg) { mArg = arg; }
    void setTriggerCallback(EventCallback cb) { mTriggerCallback = cb; }  // 设置触发事件回调
    void handleEvent();

private:
    void* mArg;
    EventCallback mTriggerCallback;  // 触发事件回调
};

// 定时事件
class TimerEvent
{
public:
    static TimerEvent* createNew(void* arg);
    static TimerEvent* createNew();

    TimerEvent(void* arg);
    ~TimerEvent() { }

    void setArg(void* arg) { mArg = arg; }
    void setTimeoutCallback(EventCallback cb) { mTimeoutCallback = cb; }
    void handleEvent();

private:
    void* mArg;
    EventCallback mTimeoutCallback;
};

// IO事件
class IOEvent
{
public:
    enum IOEventType  // IO事件类型
    {
        EVENT_NONE = 0,
        EVENT_READ = 1,
        EVENT_WRITE = 2,
        EVENT_ERROR = 4,
    };
    
    static IOEvent* createNew(int fd, void* arg);
    static IOEvent* createNew(int fd);

    IOEvent(int fd, void* arg);
    ~IOEvent() { }

    int getFd() const { return mFd; }
    int getEvent() const { return mEvent; }
    void setREvent(int event) { mREvent = event; }
    void setArg(void* arg) { mArg = arg; }

    void setReadCallback(EventCallback cb) { mReadCallback = cb; };   // 设置读事件回调
    void setWriteCallback(EventCallback cb) { mWriteCallback = cb; }; // 设置写事件回调
    void setErrorCallback(EventCallback cb) { mErrorCallback = cb; }; // 设置错误事件回调

    void enableReadHandling() { mEvent |= EVENT_READ; }
    void enableWriteHandling() { mEvent |= EVENT_WRITE; }
    void enableErrorHandling() { mEvent |= EVENT_ERROR; }
    void disableReadeHandling() { mEvent &= ~EVENT_READ; }
    void disableWriteHandling() { mEvent &= ~EVENT_WRITE; }
    void disableErrorHandling() { mEvent &= ~EVENT_ERROR; }

    bool isNoneHandling() const { return mEvent == EVENT_NONE; }
    bool isReadHandling() const { return (mEvent & EVENT_READ) != 0; }
    bool isWriteHandling() const { return (mEvent & EVENT_WRITE) != 0; }
    bool isErrorHandling() const { return (mEvent & EVENT_ERROR) != 0; };

    void handleEvent();

private:
    int mFd;
    void* mArg;
    int mEvent;
    int mREvent;
    EventCallback mReadCallback;
    EventCallback mWriteCallback;
    EventCallback mErrorCallback;
};

#endif //_EVENT_H_