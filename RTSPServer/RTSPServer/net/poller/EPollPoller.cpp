#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "net/poller/EPollPoller.h"
#include "base/Logging.h"
#include "base/New.h"

static const int InitEventListSize = 16;
static const int epollTimeout = 10000;

EPollPoller* EPollPoller::createNew()
{
    //return new EPollPoller();
    return New<EPollPoller>::allocate();
}

EPollPoller::EPollPoller() :
    mEPollEventList(InitEventListSize)
{
    mEPollFd = ::epoll_create1(EPOLL_CLOEXEC);
}

EPollPoller::~EPollPoller()
{
    ::close(mEPollFd);
}

bool EPollPoller::addIOEvent(IOEvent* event)
{
    return updateIOEvent(event);
}

bool EPollPoller::updateIOEvent(IOEvent* event)
{
    struct epoll_event epollEvt;
    int fd = event->getFd();

    memset(&epollEvt, 0, sizeof(epollEvt));
    epollEvt.data.fd = fd;
    if(event->isReadHandling())
        epollEvt.events |= EPOLLIN;
    if(event->isWriteHandling())
        epollEvt.events |= EPOLLOUT;
    if(event->isErrorHandling())
        epollEvt.events |= EPOLLERR;

    IOEventMap::iterator it = mEventMap.find(fd);
    if(it != mEventMap.end())
    {
        epoll_ctl(mEPollFd, EPOLL_CTL_MOD, fd, &epollEvt);
    }
    else
    {
        epoll_ctl(mEPollFd, EPOLL_CTL_ADD, fd, &epollEvt);
        mEventMap.insert(std::make_pair(fd, event));
        if(mEventMap.size() >= mEPollEventList.size())
            mEPollEventList.resize(mEPollEventList.size() * 2);
    }

    return true;
}

bool EPollPoller::removeIOEvent(IOEvent* event)
{
    int fd = event->getFd();
    IOEventMap::iterator it = mEventMap.find(fd);
    if(it == mEventMap.end())
        return false;
    
    epoll_ctl(mEPollFd, EPOLL_CTL_DEL, fd, NULL);
    mEventMap.erase(fd);

    return true;
}

// 处理事件
void EPollPoller::handleEvent()
{
    int nums, fd, event, revent;
    IOEventMap::iterator it;

    // 内核检测有事件发生的文件描述符
    nums = epoll_wait(mEPollFd, &*mEPollEventList.begin(), mEPollEventList.size(), epollTimeout);
    if(nums < 0)
    {
        LOG_DEBUG("epoll wait err\n");
        return;
    }

    // 遍历有事件发生的文件描述符
    for(int i = 0; i < nums; ++i)
    {
        revent = 0;
        fd = mEPollEventList.at(i).data.fd;    // 从epoll事件表中找到对应的fd
        event = mEPollEventList.at(i).events;  // 从epoll事件表中找到对应发生的事件
        if(event & EPOLLIN || event & EPOLLPRI || event & EPOLLRDHUP)
            revent |= IOEvent::EVENT_READ;
        if(event & EPOLLOUT)
            revent |= IOEvent::EVENT_WRITE;
        if(event & EPOLLERR)
            revent |= IOEvent::EVENT_ERROR;

        it = mEventMap.find(fd);
        assert(it != mEventMap.end());

        it->second->setREvent(revent);    // 设置事件属性
        mEvents.push_back(it->second);    // 加入事件表中
    }

    for(std::vector<IOEvent*>::iterator it = mEvents.begin(); it != mEvents.end(); ++it)
        (*it)->handleEvent();
    
    mEvents.clear();
}

