#ifndef _POLLER_H_
#define _POLLER_H_
#include <map>

#include "net/Event.h"

// IO多路复用方式（轮询方式）
class Poller
{
public:
    virtual ~Poller();

    virtual bool addIOEvent(IOEvent* event) = 0;
    virtual bool updateIOEvent(IOEvent* event) = 0;
    virtual bool removeIOEvent(IOEvent* event) = 0;
    virtual void handleEvent() = 0;

protected:
    Poller();

protected:
    typedef std::map<int, IOEvent*> IOEventMap;
    IOEventMap mEventMap;            // IO事件表
};

#endif //_POLLER_H_