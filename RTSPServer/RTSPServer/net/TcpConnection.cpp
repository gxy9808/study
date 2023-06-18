#include "net/TcpConnection.h"
#include "net/SocketsOps.h"
#include "base/Logging.h"
#include "base/New.h"

#include <unistd.h>
#include <stdlib.h>


// 初始化一个TCP连接，在RTSP连接初始化时被初始化
TcpConnection::TcpConnection(UsageEnvironment* env, int sockfd) :
    mEnv(env),
    mSocket(sockfd),
    mDisconnectionCallback(NULL),
    mArg(NULL)
{
    mTcpConnIOEvent = IOEvent::createNew(sockfd, this);
    mTcpConnIOEvent->setReadCallback(readCallback);
    mTcpConnIOEvent->setWriteCallback(writeCallback);
    mTcpConnIOEvent->setErrorCallback(errorCallback);
    mTcpConnIOEvent->enableReadHandling(); //默认只开启读
    mEnv->scheduler()->addIOEvent(mTcpConnIOEvent);   // 事件调度添加IO事件
}

TcpConnection::~TcpConnection()
{
    mEnv->scheduler()->removeIOEvent(mTcpConnIOEvent);
    //delete mTcpConnIOEvent;
    Delete::release(mTcpConnIOEvent);
}

void TcpConnection::setDisconnectionCallback(DisconnectionCallback cb, void* arg)
{
    mDisconnectionCallback = cb;
    mArg = arg;
}

void TcpConnection::enableReadHandling()
{
    if(mTcpConnIOEvent->isReadHandling())
        return;
    
    mTcpConnIOEvent->enableReadHandling();
    mEnv->scheduler()->updateIOEvent(mTcpConnIOEvent);
}

void TcpConnection::enableWriteHandling()
{
    if(mTcpConnIOEvent->isWriteHandling())
        return;
    
    mTcpConnIOEvent->enableWriteHandling();
    mEnv->scheduler()->updateIOEvent(mTcpConnIOEvent);
}

void TcpConnection::enableErrorHandling()
{
    if(mTcpConnIOEvent->isErrorHandling())
        return;

    mTcpConnIOEvent->enableErrorHandling();
    mEnv->scheduler()->updateIOEvent(mTcpConnIOEvent);
}

void TcpConnection::disableReadeHandling()
{
    if(!mTcpConnIOEvent->isReadHandling())
        return;

    mTcpConnIOEvent->disableReadeHandling();
    mEnv->scheduler()->updateIOEvent(mTcpConnIOEvent);
}   

void TcpConnection::disableWriteHandling()
{
    if(!mTcpConnIOEvent->isWriteHandling())
        return;

    mTcpConnIOEvent->disableWriteHandling();
    mEnv->scheduler()->updateIOEvent(mTcpConnIOEvent);
}

void TcpConnection::disableErrorHandling()
{
    if(!mTcpConnIOEvent->isErrorHandling())
        return;

    mTcpConnIOEvent->disableErrorHandling();
    mEnv->scheduler()->updateIOEvent(mTcpConnIOEvent);
}

void TcpConnection::handleRead()
{
    // 从文件描述符中读取数据 （从文件描述符缓存区中读取数据到分散的内存块iovec中）
    int ret = mInputBuffer.read(mSocket.fd()); 

    if(ret == 0)
    {
        LOG_DEBUG("client disconnect\n");
        handleDisconnection();   // 断开连接
        return;
    }
    else if(ret < 0)
    {
        LOG_ERROR("read err\n");
        handleDisconnection();
        return;
    }

    /* 先取消读 */
    //this->disableReadeHandling();

    handleReadBytes();
}

// 清空读缓冲区
void TcpConnection::handleReadBytes()
{
    LOG_DEBUG("default read handle\n");
    mInputBuffer.retrieveAll();
}

void TcpConnection::handleWrite()
{
    LOG_DEBUG("default wirte handle\n");
    mOutBuffer.retrieveAll();
}

void TcpConnection::handleError()
{
    LOG_DEBUG("default error handle\n");
}

// IO多路轮询，读回调的入口
void TcpConnection::readCallback(void* arg)
{
    TcpConnection* tcpConnection = (TcpConnection*)arg;
    tcpConnection->handleRead();   // 处理读事件
}

// IO多路轮询，写回调的入口
void TcpConnection::writeCallback(void* arg)
{
    TcpConnection* tcpConnection = (TcpConnection*)arg;
    tcpConnection->handleWrite();
}

void TcpConnection::errorCallback(void* arg)
{
    TcpConnection* tcpConnection = (TcpConnection*)arg;
    tcpConnection->handleError();
}

// 处理断开连接：执行断开连接的回调，传给RTSPserver的回调中，关闭对应的fd
void TcpConnection::handleDisconnection()  
{
    if(mDisconnectionCallback)
        mDisconnectionCallback(mArg, mSocket.fd());
}