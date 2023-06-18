#ifndef _TCP_SERVER_H_
#define _TCP_SERVER_H_
#include <map>

#include "net/Acceptor.h"
#include "net/UsageEnvironment.h"
#include "net/InetAddress.h"
#include "net/TcpConnection.h"

//class TcpConnection;

// TCP服务器 （RTSP服务器的基类）
class TcpServer
{
public:
    virtual ~TcpServer();

    void start();  // 服务器启动

protected:
    TcpServer(UsageEnvironment* env, const Ipv4Address& addr);
    virtual void handleNewConnection(int connfd) = 0;  // 纯虚函数
    //virtual void handleDisconnection(int sockfd);

private:
    static void newConnectionCallback(void* arg, int connfd);
    //static void disconnectionCallback(void* arg, int sockfd);

protected:
    UsageEnvironment* mEnv;
    Acceptor* mAcceptor;          // 接收端 即服务器端
    Ipv4Address mAddr;            // 套接字地址
//    std::map<int, TcpConnection*> mTcpConnections;
};

#endif //_TCP_SERVER_H_