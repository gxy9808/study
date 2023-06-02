// 客户端接收广播通信
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

    // 2.客户端绑定广播的IP地址和端口
    struct sockaddr_in addr; 
    addr.sin_family = AF_INET;  
    inet_pton(AF_INET, "192.168.233.255", &addr.sin_addr.s_addr); // 广播地址
    //addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9999); 

    int ret = bind(fd, (struct sockaddr*)&addr, sizeof(addr));

    if(ret==-1)
    {
        perror("bind");
        exit(-1);
    }

    // 2.通信
    int num = 0;
    while (1)
    {
        char recvbuf[128] = {0};
        // 接收数据
        int num = recvfrom(fd, recvbuf, sizeof(recvbuf), 0, NULL, NULL); // 客户端从指定的广播端口进行数据的读取,client的IP地址是局域网内的即可(所以可以使用INADDR_ANY)，但端口必须是server广播使用的端口(所以一定要绑定广播端口  
        printf("receive broadcast message :%s\n", recvbuf); // 不同IP地址的客户端主机绑定广播端口后，都可以接收服务器的广播数据
    }
    close(fd);
    return 0;
}