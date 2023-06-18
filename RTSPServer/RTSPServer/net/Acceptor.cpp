#include "net/Acceptor.h"
#include "net/SocketsOps.h"
#include "base/Logging.h"
#include "base/New.h"

Acceptor* Acceptor::createNew(UsageEnvironment* env, const Ipv4Address& addr)
{
    //return new Acceptor(env, addr);
    return New<Acceptor>::allocate(env, addr);
}

Acceptor::Acceptor(UsageEnvironment* env, const Ipv4Address& addr) :
    mEnv(env),
    mAddr(addr),
    mSocket(sockets::createTcpSock()),
    mNewConnectionCallback(NULL)
{
    mSocket.setReuseAddr(1);
    mSocket.bind(mAddr);   // 绑定
    mAcceptIOEvent = IOEvent::createNew(mSocket.fd(), this);  // 接收客户端连接的IO事件
    mAcceptIOEvent->setReadCallback(readCallback);            // 设置读回调
    mAcceptIOEvent->enableReadHandling();
}

Acceptor::~Acceptor()
{
    if(mListenning)
        mEnv->scheduler()->removeIOEvent(mAcceptIOEvent);

    //delete mAcceptIOEvent;
    Delete::release(mAcceptIOEvent);
}

// 服务器端进行监听
void Acceptor::listen()  
{
    mListenning = true;
    mSocket.listen(1024);  // socket进行监听
    // 添加IO事件调度，为建立连接的IO事件
    mEnv->scheduler()->addIOEvent(mAcceptIOEvent);  // 第20行
}

// 服务器端设置接收连接回调
void Acceptor::setNewConnectionCallback(NewConnectionCallback cb, void* arg)
{
    mNewConnectionCallback = cb;
    mArg = arg;
}

void Acceptor::readCallback(void* arg)
{
    Acceptor* acceptor = (Acceptor*)arg;
    acceptor->handleRead();
}

void Acceptor::handleRead()
{
    int connfd = mSocket.accept();  // 建立连接的fd
    LOG_DEBUG("client connect: %d\n", connfd);
    if(mNewConnectionCallback)
        mNewConnectionCallback(mArg, connfd); // 进入建立新连接的回调
}
