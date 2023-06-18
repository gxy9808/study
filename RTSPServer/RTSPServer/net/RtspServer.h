#ifndef _RTSPSERVER_H_
#define _RTSPSERVER_H_
#include <map>
#include <vector>
#include <string>

#include "net/TcpServer.h"
#include "net/UsageEnvironment.h"
#include "net/RtspConnection.h"
#include "net/MediaSession.h"
#include "net/Event.h"
#include "base/Mutex.h"

class RtspConnection;  // 前向声明

// RTSP服务器类
class RtspServer : public TcpServer
{
public:
    static RtspServer* createNew(UsageEnvironment* env, Ipv4Address& addr);  // RTSP服务器创建方法

    RtspServer(UsageEnvironment* env, const Ipv4Address& addr);  // RTSP服务器初始化方法
    virtual ~RtspServer();                                       // 析构

    UsageEnvironment* envir() const { return mEnv; }
    bool addMeidaSession(MediaSession* mediaSession);            // RTSP服务器添加媒体会话（音视频流）
    MediaSession* loopupMediaSession(std::string name);          // 查找某媒体会话资源，并返回  
    std::string getUrl(MediaSession* session);                   // 获取当前RTSP连接的url

protected:
    virtual void handleNewConnection(int connfd);                // 处理新的RTSP连接（connfd）
    static void disconnectionCallback(void* arg, int sockfd);    // 断开RTSP连接的回调函数
    void handleDisconnection(int sockfd);                        // 断开RTSP连接
    static void triggerCallback(void*);                          // 触发事件的回调
    void handleDisconnectionList();                              // 处理要断开的RTSP连接列表

private:
    std::map<std::string, MediaSession*> mMediaSessions;         // 音视频流会话表，用map进行映射
    std::map<int, RtspConnection*> mConnections;                 // 建立的RTSP连接表
    std::vector<int> mDisconnectionlist;                         // 要断开的RTSP连接的容器
    TriggerEvent* mTriggerEvent;                                 // 触发事件
    Mutex* mMutex;                                               // 互斥锁
};

#endif //_RTSPSERVER_H_