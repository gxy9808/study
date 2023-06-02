// UDP服务器端通信

#include<stdio.h>  
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include <arpa/inet.h>
#include<unistd.h>

int main() {

    // 1.创建一个通信的套接字socket
    int fd = socket(PF_INET, SOCK_DGRAM,0);

    if(fd == -1) {
        perror("socket");
        exit(-1);
    }

    // 创建服务器IPv4的socket地址结构体
    struct sockaddr_in serveraddr; 
    serveraddr.sin_family = AF_INET;  // 地址族 IPV4
    inet_pton(AF_INET, "192.168.233.129", &serveraddr.sin_addr.s_addr); // 本地的网络地址，点分十进制→网络字节序
    serveraddr.sin_port = htons(9999); // 转换端口，主机字节序→网络字节序
    // 2.绑定
    int ret = bind(fd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    if(ret==-1)
    {
        perror("bind");
        exit(-1);
    }

    // 3.通信
    while (1)
    {
        struct sockaddr_in clientaddr;
        int len = sizeof(clientaddr);

        char recvbuf[128] = {0};
        // 接收数据
        int num = recvfrom(fd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*)&clientaddr, &len);   
        char clientIP[16];
        // 打印客户端信息
        printf("client IP:%s, Port：%d\n",
            inet_ntop(AF_INET, &clientaddr.sin_addr.s_addr, clientIP, sizeof(clientIP)),
            ntohs(clientaddr.sin_port));
        
        printf("receive message from client:%s\n", recvbuf);

        // 发送数据
        sendto(fd, recvbuf, strlen(recvbuf)+1, 0, (struct sockaddr*)&clientaddr, sizeof(clientaddr));

        // sizeof计算的是分配的数组所占的内存空间的大小，不受里面存储的内容影响；strlen计算的是字符串的长度，以’\0’为字符串结束标志。
        // recvfrom要接收一段未知长度的字符串，所以是指定用于接收的缓冲区及缓冲区大小；而sendto要发送一段字符串，并指定发送数据的长度。

    }
    close(fd);
    
    return 0;
}