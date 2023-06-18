#include <sys/eventfd.h>
#include <unistd.h>
#include <stdint.h>

#include "net/EventScheduler.h"
#include "net/poller/SelectPoller.h"
#include "net/poller/PollPoller.h"
#include "net/poller/EPollPoller.h"
#include "base/Logging.h"
#include "base/New.h"

static int createEventFd()
{
    // 创建一个文件描述符，用于事件通知
    int evtFd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtFd < 0)
    {
        LOG_ERROR("failed to create event fd\n");
        return -1;
    }

    return evtFd;
}

EventScheduler* EventScheduler::createNew(PollerType type)
{
    if(type != POLLER_SELECT && type != POLLER_POLL && type != POLLER_EPOLL)
        return NULL;

    int evtFd = createEventFd();
    if (evtFd < 0)
        return NULL;

    //return new EventScheduler(type, evtFd);
    return New<EventScheduler>::allocate(type, evtFd);
}

EventScheduler::EventScheduler(PollerType type, int fd) :
    mQuit(false),
    mWakeupFd(fd)
{
    switch (type)
    {
    case POLLER_SELECT:
        mPoller = SelectPoller::createNew();
        break;

    case POLLER_POLL:
        mPoller = PollPoller::createNew();
        break;
    
    case POLLER_EPOLL:
        mPoller = EPollPoller::createNew();
        break;

    default:
        _exit(-1);
        break;
    }

    // 创建一个定时器管理器
    mTimerManager = TimerManager::createNew(mPoller);
    // 创建一个用于唤醒调度器的IO事件
    mWakeIOEvent = IOEvent::createNew(mWakeupFd, this);
    // IO事件可读回调
    mWakeIOEvent->setReadCallback(handleReadCallback);
    // 设置IO事件可读
    mWakeIOEvent->enableReadHandling();
    // IO多路复用轮询者添加用于唤醒调度器的IO事件
    mPoller->addIOEvent(mWakeIOEvent);

    mMutex = Mutex::createNew();
}

EventScheduler::~EventScheduler()
{
    mPoller->removeIOEvent(mWakeIOEvent);
    ::close(mWakeupFd);

    //delete mWakeIOEvent;
    //delete mTimerManager;
    //delete mPoller;
    Delete::release(mWakeIOEvent);
    Delete::release(mTimerManager);
    Delete::release(mPoller);
    Delete::release(mMutex);
}

// 添加触发事件
bool EventScheduler::addTriggerEvent(TriggerEvent* event)
{
    mTriggerEvents.push_back(event);

    return true;
}

Timer::TimerId EventScheduler::addTimedEventRunAfater(TimerEvent* event, Timer::TimeInterval delay)
{
    Timer::Timestamp when = Timer::getCurTime();
    when += delay;
    
    return mTimerManager->addTimer(event, when, 0);
}

Timer::TimerId EventScheduler::addTimedEventRunAt(TimerEvent* event, Timer::Timestamp when)
{
    return mTimerManager->addTimer(event, when, 0);
}
  
// 事件调度器添加计时器的轮回事件
Timer::TimerId EventScheduler::addTimedEventRunEvery(TimerEvent* event, Timer::TimeInterval interval)
{
    Timer::Timestamp when = Timer::getCurTime();
    when += interval;

    return mTimerManager->addTimer(event, when, interval);
}

bool EventScheduler::removeTimedEvent(Timer::TimerId timerId)
{
    return mTimerManager->removeTimer(timerId);
}

bool EventScheduler::addIOEvent(IOEvent* event)
{
    return mPoller->addIOEvent(event);
}

bool EventScheduler::updateIOEvent(IOEvent* event)
{
    return mPoller->updateIOEvent(event);
}

bool EventScheduler::removeIOEvent(IOEvent* event)
{
    return mPoller->removeIOEvent(event);
}

// 对要调度的事件进行循环
void EventScheduler::loop()  
{
    while(mQuit != true)
    {
        this->handleTriggerEvents(); // 处理触发事件
        mPoller->handleEvent();      // 多路复用处理IO事件
        this->handleOtherEvent();    
    }
}

// 唤醒
void EventScheduler::wakeup()
{
    uint64_t one = 1;
    int ret;
    ret = ::write(mWakeupFd, &one, sizeof(one));
}

// 处理触发事件
void EventScheduler::handleTriggerEvents()
{
    if(!mTriggerEvents.empty())
    {
        for(std::vector<TriggerEvent*>::iterator it = mTriggerEvents.begin();
            it != mTriggerEvents.end(); ++it)
        {
            (*it)->handleEvent();
        }

        mTriggerEvents.clear();
    }
}

// 唤醒fd读事件回调
void EventScheduler::handleReadCallback(void* arg)
{
    if(!arg)
        return;

    EventScheduler* scheduler = (EventScheduler*)arg;
    scheduler->handleRead();   // 处理唤醒fd的读事件
}

// 处理唤醒fd的读事件
void EventScheduler::handleRead()
{
    uint64_t one;
    while(::read(mWakeupFd, &one, sizeof(one)) > 0);
}

// 在当前线程中将回调加入到回调队列中
void EventScheduler::runInLocalThread(Callback callBack, void* arg)
{
    MutexLockGuard mutexLockGuard(mMutex);
    mCallBackQueue.push(std::make_pair(callBack, arg));
}

// 处理其他事件
void EventScheduler::handleOtherEvent()
{
    MutexLockGuard mutexLockGuard(mMutex);
    while(!mCallBackQueue.empty())
    {
        std::pair<Callback, void*> event = mCallBackQueue.front();
        event.first(event.second);
    }
}