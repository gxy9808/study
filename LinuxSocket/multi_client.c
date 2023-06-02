// 客户端接收多播数据
#include <arpa/inet.h>
#include<stdio.h>  
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include <unistd.h>

int main() {

    // 1.创建一个通信的套接字socket
    int fd = socket(PF_INET, SOCK_DGRAM,0);

    if(fd == -1) {
        perror("socket");
        exit(-1);
    }

    // 2.客户端绑定本地的IP地址和端口
    struct sockaddr_in addr; 
    addr.sin_family = AF_INET;  
    //inet_pton(AF_INET, "239.9.9.10", &addr.sin_addr.s_addr); // 广播地址
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9999); 

    int ret = bind(fd, (struct sockaddr*)&addr, sizeof(addr));

    if(ret==-1)
    {
        perror("bind");
        exit(-1);
    }

    // 客户端加入多播组
    struct ip_mreq op; // 设置网卡加入多播组数据结构
    inet_pton(AF_INET,"239.9.9.10", &op.imr_multiaddr.s_addr); // 指定组播的IP地址
    op.imr_interface.s_addr = INADDR_ANY; // (设置加入多播组的的网卡ip)指定本地的IP地址
    setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &addr, sizeof(addr));

    // 3.通信
    int num = 0;
    while (1)
    {
        char recvbuf[128] = {0};
        // 接收数据
        int num = recvfrom(fd, recvbuf, sizeof(recvbuf), 0, NULL, NULL); 
        printf("receive multicast message :%s\n", recvbuf); // 不同IP地址的客户端主机绑定多播端口并加入多播组后，可以接收服务器的多播数据
    }
    close(fd);
    return 0;
}


