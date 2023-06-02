// TCP客户端通信

#include<stdio.h>  
#include<stdlib.h> 
#include<string.h>

#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>


int main(){

    // 1.创建用于通信的套接字
    int cfd = socket(AF_INET, SOCK_STREAM, 0);

    if(cfd==-1)
    {
        perror("socket");
        exit(-1);
    }

    // 2.连接服务器端
    struct sockaddr_in severaddr; // 服务器socket地址结构体
    severaddr.sin_family = AF_INET;
    inet_pton(AF_INET, "192.168.233.129", &severaddr.sin_addr.s_addr);  // 服务器的IP地址 由点分字符串转为网络字节序
    severaddr.sin_port = htons(9999); // 转换服务器端口

    // 调用connect函数后进行TCP三次握手
    int ret = connect(cfd, (struct sockaddr*)&severaddr, sizeof(severaddr));

    if(ret==-1)
    {
        perror("connect:");
        exit(-1);
    }

    // 3.通信
    char sendBuf[1024] = {0}; 
    char recvBuf[1024] = {0};
    while (1)
    {
        //char * data = "hello,i am client";
    
        //给服务器发送数据，采用回射机制
        memset(sendBuf, 0, sizeof(sendBuf));  // 在发送和接收数据之前要清空之前的数组

        printf("请输入发送数据：\n");
        scanf("%s",sendBuf);
        //fgets(sendBuf, sizeof(sendBuf), stdin);  // 从输入流stdin即输入缓冲区中读取sendBuf大小的字符到字符数组sendbuf

        write(cfd,sendBuf, strlen(sendBuf));

        //sleep(1);

        // 获取（读）服务器端的数据
        memset(recvBuf, 0, sizeof(recvBuf)); // 清空接收区缓存
        int retval = read(cfd, recvBuf, sizeof(recvBuf));
        if(retval==-1)
        {
            perror("client read:");
            exit(-1);
        }else if(retval > 0)
        {
            printf("receive message from server:%s\n", recvBuf);
        }
        else if(retval == 0)
        {
            // 表示服务器断开连接
            printf("server is closed...\n");
            break;
        }
        sleep(1);
    }
    // 关闭连接
    close(cfd);
    return 0;
}