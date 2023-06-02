// TCP服务器端通信

#include<stdio.h>  // 标准输入输出头文件
#include<stdlib.h> // 标准库头文件，常用系统函数
#include<string.h>

#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>


int main(){

    // 1.创建一个用于监听的套接字socket
    int sfd = socket(AF_INET, SOCK_STREAM,0);
    if(sfd == -1) // 如果创建失败，打印错误
    {
        perror("socket"); // 将上一个函数发生错误的原因输出到标准设备,先打印字符串，然后加错误原因字符串
        exit(-1);
    }

    // 端口复用：1.防止服务器重启时之前绑定的端口还未释放；2.防止程序突然退出而系统没有释放端口
    // 端口复用设置的时机是在服务器绑定端口之前
    int optval = 1;  //端口复用的值 1：可以复用
    setsockopt(sfd,SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // 查看端口 netstat -anp |grep 9999

    // 2.绑定本地IP地址和端口
    struct sockaddr_in serveraddr; // 创建服务器IPv4的socket地址结构体 （结构体sockaddr表示通用的socket地址，sockaddr_in表示专用的socket地址）
    serveraddr.sin_family = AF_INET;  // 指定地址族 IPV4
    inet_pton(AF_INET, "192.168.233.129", &serveraddr.sin_addr.s_addr); // 指定本地的网络地址，点分十进制→网络字节序
    //serveraddr.sin_addr.s_addr = INADDR_ANY; // 指定网络地址 0.0.0.0 表示绑定以太网卡和无线网卡等多个网卡的IP地址，当有客户端连接时，都可以获取到IP地址（仅服务器端可以这样使用，客户端不行）
    serveraddr.sin_port = htons(9999); // 指定并转换端口，主机字节序→网络字节序

    int ret = bind(sfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    
    if(ret==-1)
    {
        perror("bind");
        exit(-1);
    }

    // 3.监听, 监听的套接字开始工作,没有客户端连接时处于堵塞等待
    ret = listen(sfd, 8);  // 参数backlog=8 未连接的和已连接的客户端数目的和的最大值
    if(ret==-1)
    {
        perror("listen");
        exit(-1);
    }

    // 4.接收客户端连接
    struct sockaddr_in clientaddr;
    int len = sizeof(clientaddr);
    // 接收客户端连接后，得到用于通信的套接字
    int cfd = accept(sfd, (struct sockaddr*)&clientaddr, &len);  // 第二个参数是传出参数

    if(cfd==-1)
    {
        perror("accept:");
        exit(-1);
    }

    // 输出客户端信息
    char clientIP[16];  // 客户端IP地址
    inet_ntop(AF_INET, &clientaddr.sin_addr.s_addr, clientIP, sizeof(clientIP)); // 将客户端IP地址由网络字节序转换成点分十进制， 存储到字符串数组（第三个参数）中
    unsigned short clientPort = ntohs(clientaddr.sin_port);  // 客户端端口由网络序转为字节序
    printf("***********************************\n");
    printf("client ip is %s, port is %d\n",clientIP,clientPort);

    // 5. 通信
    char recvBuf[1024] = {0}; // 服务器接收缓存区
    char sendBuf[1024] = {0}; // 服务器发送缓存区
    while (1)
    {
        memset(recvBuf, 0, sizeof(recvBuf)); // 清空接收缓存
        // 获取客户端的数据
        int len = read(cfd, recvBuf, sizeof(recvBuf));

        if(len==-1){
            perror("read:");
            exit(-1);
        }else if (len>0)
        {
            printf("receive message from client:%s\n", recvBuf);
        }else if(len==0){
            // 表示客户端断开连接
            printf("clinet is closed...\n");
            break;
        }
        // char* data = "hello,I am server";
        // 给客户端发送数据
        memset(sendBuf, 0, sizeof(sendBuf));  // 清空发送缓冲区
        strcpy(sendBuf,recvBuf);  // 把接收缓冲区的数据给发送缓冲区
        len = write(cfd, sendBuf, strlen(sendBuf));
        if (len == -1) {
            perror("write");
            break;
        } else {
            printf("send message to client: %s\n", sendBuf);
        }
    }
    // 关闭文件描述符
    close(sfd);
    close(cfd);
    return 0;
}