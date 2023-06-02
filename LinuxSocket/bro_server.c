// UDP服务器广播通信

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

    // 2.设置广播属性
    int op = 1; // 1表示允许广播
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &op, sizeof(op));

    // 3.创建一个广播地址
    struct sockaddr_in broaddr; 
    broaddr.sin_family = AF_INET;  // 地址族 IPV4
    inet_pton(AF_INET, "192.168.233.255", &broaddr.sin_addr.s_addr); // 广播的IP地址
    broaddr.sin_port = htons(9999); // 指定端口， 服务器要往该端口发送数据
   
    // 3.通信
    int num = 0;
    while (1)
    {
        char sendBuf[128] = {0}; // 发送缓存区
        //printf("Please send broadcast data:\n");
        // 从输入缓冲区读取数据到sendBuf中
        //fgets(sendBuf, sizeof(sendBuf), stdin);  
        sprintf(sendBuf,"%d\n",num++);
        // 发送数据
        sendto(fd, sendBuf, strlen(sendBuf)+1, 0, (struct sockaddr*)&broaddr, sizeof(broaddr));
        printf("send broadcast data:%s\n", sendBuf); 
        sleep(1);
    }
    close(fd);
    return 0;
}