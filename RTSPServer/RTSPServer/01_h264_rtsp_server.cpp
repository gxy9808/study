#include <iostream>

#include "base/Logging.h"
#include "net/UsageEnvironment.h"
#include "base/ThreadPool.h"
#include "net/EventScheduler.h"
#include "net/Event.h"
#include "net/RtspServer.h"
#include "net/MediaSession.h"
#include "net/InetAddress.h"
#include "net/H264FileMediaSource.h"
#include "net/H264RtpSink.h"


int main(int argc, char* argv[])
{
    if(argc !=  2)
    {
        std::cout<<"Usage: "<<argv[0]<<" <h264 file>"<<std::endl;
        return -1;
    }

    //Logger::setLogFile("xxx.log");
    Logger::setLogLevel(Logger::LogWarning);

    // 创建事件调度器
    EventScheduler* scheduler = EventScheduler::createNew(EventScheduler::POLLER_SELECT);
    // 创建线程池
    ThreadPool* threadPool = ThreadPool::createNew(2);
    // 创建环境变量（将事件调度器和线程池填加到环境变量中）
    UsageEnvironment* env = UsageEnvironment::createNew(scheduler, threadPool);

    // 创建RTSP服务器的套接字地址（IP地址和端口）
    Ipv4Address ipAddr("0.0.0.0", 8554);  
    // 创建RTSP服务器
    RtspServer* server = RtspServer::createNew(env, ipAddr);
    // 创建一个新的媒体会话
    MediaSession* session = MediaSession::createNew("live");  
    // 创建一个音频流资源（生产者）
    MediaSource* mediaSource = H264FileMediaSource::createNew(env, argv[1]);
    // 把视频流资源加入到RtpSink中（消费者）
    RtpSink* rtpSink = H264RtpSink::createNew(env, mediaSource);
    // 把RtpSink加入到媒体会话资源的第一路中（track0）
    session->addRtpSink(MediaSession::TrackId0, rtpSink);
    //session->startMulticast();  //多播

    // RTSP服务器添加当前的媒体会话
    server->addMeidaSession(session);
    // RTSP服务器运行
    server->start();

    std::cout<<"Play the media using the URL \""<<server->getUrl(session)<<"\""<<std::endl;

    // 开始调度
    env->scheduler()->loop();

    return 0;
}