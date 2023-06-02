// UDP服务器多播通信

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

    // 2.设置多播属性， 设置外出接口
    struct in_addr imr_multiaddr; // 多播用的地址结构
    // 初始化多播地址
    inet_pton(AF_INET,"239.9.9.10", &imr_multiaddr.s_addr);
    setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &imr_multiaddr, sizeof(imr_multiaddr));

    // 3.初始化客户端的地址信息
    struct sockaddr_in cliaddr; 
    cliaddr.sin_family = AF_INET;  
    inet_pton(AF_INET, "239.9.9.10", &cliaddr.sin_addr.s_addr); // 多播的IP地址转换
    cliaddr.sin_port = htons(9999); 
   
    // 3.通信
    int num = 0;
    while (1)
    {
        char sendBuf[128] = {0}; // 发送缓存区 
        sprintf(sendBuf,"%d\n",num++);
        //  给多播地址发送数据
        sendto(fd, sendBuf, strlen(sendBuf)+1, 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
        printf("send multicast data:%s\n", sendBuf); 
        sleep(1);
    }
    close(fd);
    return 0;
}