#ifndef _SOCKET_H_
#define _SOCKET_H_
#include <string>
#include <stdint.h>

#include "net/InetAddress.h"

// 建立TCP连接的套接字类
class  TcpSocket
{
public:
    explicit TcpSocket(int sockfd) :
        mSockfd(sockfd) { }

    ~TcpSocket();

    int fd() const { return mSockfd; }  
    bool bind(Ipv4Address& addr);  // 绑定
    bool listen(int backlog);      // 监听
    int accept();                  // 接受客户端连接
    void setReuseAddr(int on);     // 设置地址可复用

private:
    int mSockfd;
};

#endif //_SOCKET_H_