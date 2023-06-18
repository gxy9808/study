#include "net/InetAddress.h"

Ipv4Address::Ipv4Address()
{

}

Ipv4Address::Ipv4Address(std::string ip, uint16_t port) : // 套接字地址构造函数
    mIp(ip),
    mPort(port)
{
    mAddr.sin_family = AF_INET;		  
    mAddr.sin_addr.s_addr = inet_addr(ip.c_str()); 
    mAddr.sin_port = htons(port);
}

void Ipv4Address::setAddr(std::string ip, uint16_t port)  // 设置套接字地址
{
    mIp = ip;
    mPort = port;
    mAddr.sin_family = AF_INET;		  
    mAddr.sin_addr.s_addr = inet_addr(ip.c_str()); 
    mAddr.sin_port = htons(port);
}

std::string Ipv4Address::getIp()  // 获取IP地址
{
    return mIp;
}

uint16_t Ipv4Address::getPort()  // 获取端口
{
    return mPort;
}

struct sockaddr* Ipv4Address::getAddr()
{
    return (struct sockaddr*)&mAddr;
}
