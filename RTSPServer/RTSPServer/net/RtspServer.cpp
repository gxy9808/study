#include <algorithm>
#include <assert.h>
#include <stdio.h>

#include "net/RtspServer.h"
#include "base/New.h"

// 创建一个RTSP服务器对象
RtspServer* RtspServer::createNew(UsageEnvironment* env, Ipv4Address& addr)
{
    //return new RtspServer(env, addr);
    return New<RtspServer>::allocate(env, addr);
}

// RTSP对象初始化
RtspServer::RtspServer(UsageEnvironment* env, const Ipv4Address& addr) :
    TcpServer(env, addr)
{
    mTriggerEvent = TriggerEvent::createNew(this);
    mTriggerEvent->setTriggerCallback(triggerCallback); // 设置回调

    mMutex = Mutex::createNew();
}

RtspServer::~RtspServer()
{
    //delete mTriggerEvent;
    //delete mMutex;

    Delete::release(mTriggerEvent);
    Delete::release(mMutex);
}

// 处理新的RTSP连接
void RtspServer::handleNewConnection(int connfd)
{
    RtspConnection* conn = RtspConnection::createNew(this, connfd);  // 创建RTSP连接
    conn->setDisconnectionCallback(disconnectionCallback, this);     // 断开连接的回调
    mConnections.insert(std::make_pair(connfd, conn));               // RTSP连接表记录当前建立的RTSP连接
}

// RTSP断开连接的回调函数
void RtspServer::disconnectionCallback(void* arg, int sockfd)
{
    RtspServer* rtspServer = (RtspServer*)arg;
    rtspServer->handleDisconnection(sockfd);
}

// 断开建立的sockfd连接
void RtspServer::handleDisconnection(int sockfd)  
{
    MutexLockGuard mutexLockGuard(mMutex);             // 加锁
    mDisconnectionlist.push_back(sockfd);              // 将要断开的连接加入到取消队列中
    mEnv->scheduler()->addTriggerEvent(mTriggerEvent); // 触发事件被激活，回调执行断开连接
}

// 添加媒体会话
bool RtspServer::addMeidaSession(MediaSession* mediaSession)  
{
    if(mMediaSessions.find(mediaSession->name()) != mMediaSessions.end())
        return false;

    mMediaSessions.insert(std::make_pair(mediaSession->name(), mediaSession));

    return true;
}

// 查找某媒体会话资源，并返回（查找不到返回空）
MediaSession* RtspServer::loopupMediaSession(std::string name)
{
    std::map<std::string, MediaSession*>::iterator it = mMediaSessions.find(name);
    if(it == mMediaSessions.end())
        return NULL;
    
    return it->second;
}

std::string RtspServer::getUrl(MediaSession* session)
{
    char url[200];

    snprintf(url, sizeof(url), "rtsp://%s:%d/%s", sockets::getLocalIp().c_str(),
                mAddr.getPort(), session->name().c_str());

    return std::string(url);
}

// 触发回调（执行断开连接）
void RtspServer::triggerCallback(void* arg)
{
    RtspServer* rtspServer = (RtspServer*)arg;
    rtspServer->handleDisconnectionList();
}

// 断开RTSP连接， 将在断开连接表中的RTSP连接断开
void RtspServer::handleDisconnectionList() 
{
    MutexLockGuard mutexLockGuard(mMutex);  // 加锁

    for(std::vector<int>::iterator it = mDisconnectionlist.begin(); it != mDisconnectionlist.end(); ++it)
    {
        int sockfd = *it;
        std::map<int, RtspConnection*>::iterator _it = mConnections.find(sockfd);
        assert(_it != mConnections.end());
        //delete _it->second;
        Delete::release(_it->second);
        mConnections.erase(sockfd);
    }

    mDisconnectionlist.clear();
}