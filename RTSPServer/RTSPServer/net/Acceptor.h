#ifndef _ACCEPTOR_H_
#define _ACCEPTOR_H_
#include "net/UsageEnvironment.h" 
#include "net/Event.h"
#include "net/InetAddress.h"
#include "net/TcpSocket.h"

// 服务器端接收连接的类
class Acceptor
{
public:
    typedef void(*NewConnectionCallback)(void* data, int connfd); // 定义新连接回调的函数指针

    static Acceptor* createNew(UsageEnvironment* env, const Ipv4Address& addr);

    Acceptor(UsageEnvironment* env, const Ipv4Address& addr);
    ~Acceptor();

    bool listenning() const { return mListenning; }
    void listen();
    void setNewConnectionCallback(NewConnectionCallback cb, void* arg);  // 设置新的连接回调

private:
    static void readCallback(void*);  // 读事件回调
    void handleRead();

private:
    UsageEnvironment* mEnv;
    Ipv4Address mAddr;          // 套接字地址
    IOEvent* mAcceptIOEvent;    // 接受连接时的IO事件
    TcpSocket mSocket;          // 建立连接的socket
    bool mListenning;
    NewConnectionCallback mNewConnectionCallback;  // 新连接的回调
    void* mArg;
};

#endif //_ACCEPTOR_H_