// UDP客户端通信

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

    // 服务器地址信息
    struct sockaddr_in serveraddr; 
    serveraddr.sin_family = AF_INET;  
    inet_pton(AF_INET, "192.168.233.129", &serveraddr.sin_addr.s_addr); // 服务器IP地址，点分十进制→网络字节序
    serveraddr.sin_port = htons(9999); 

    // 2.通信
    int num = 0;
    while (1)
    {

        char sendbuf[128] = {0};
        sprintf(sendbuf, "num:%d\n", num++);

        // 发送数据
        sendto(fd, sendbuf, strlen(sendbuf), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr));

        // 接收数据
        int len = sizeof(serveraddr);
        memset(sendbuf, 0, sizeof(sendbuf));
        int num = recvfrom(fd, sendbuf, sizeof(sendbuf), 0, (struct sockaddr*)&serveraddr, &len);   
        
        printf("receive message from client:%s\n", sendbuf);
        sleep(1);

    }
    close(fd);
    
    return 0;
}