// TCP多进程并发服务器通信

#define _XOPEN_SOURCE  // 解决sigaction结构报错
#include<stdio.h>  
#include<stdlib.h> 
#include<string.h>

#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include<signal.h>
#include<wait.h>
#include <errno.h>

void recyleChild(int arg) {
    while (1)
    {
        int ret = waitpid(-1, NULL, WNOHANG);
        if(ret == -1){  // 所有子进程都回收了
            break;
        }else if(ret == 0) {
            // 还有子进程活着
            break;
        }else  // ret > 0
        {
            // 子进程被回收
            printf("子进程 %d 被回收了\n", ret); // 进程号
        }
    }
}

int main(){

    struct sigaction act;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    act.sa_handler = recyleChild;
    // 注册信号捕捉
    sigaction(SIGCHLD, &act, NULL);

    // 创建套接字socket
    int lfd = socket(AF_INET, SOCK_STREAM,0);
    if(lfd == -1) 
    {
        perror("socket"); 
        exit(-1);
    }

    // 绑定本地IP地址和端口
    struct sockaddr_in serveraddr; // 创建服务器socket地址结构体 
    serveraddr.sin_family = AF_INET;  // 指定地址族 IPV4
    inet_pton(AF_INET, "192.168.233.129", &serveraddr.sin_addr.s_addr); // 点分十进制→网络字节序
    //serveraddr.sin_addr.s_addr = INADDR_ANY; 
    serveraddr.sin_port = htons(9999); // 转换端口

    int ret = bind(lfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    
    if(ret==-1)
    {
        perror("bind");
        exit(-1);
    }

    // 监听创建的套接字，等待与客户端连接
    ret = listen(lfd, 8);  
    if(ret==-1)
    {
        perror("listen");
        exit(-1);
    }

    // 不断循环等待客户端连接
    while (1)
    {
        struct sockaddr_in clientaddr;
        int len = sizeof(clientaddr);
        // 接收客户端连接后，得到用于通信的套接字
        int cfd = accept(lfd, (struct sockaddr*)&clientaddr, &len);  // 第二个参数是传出参数

        if(cfd==-1)
        {
            if(errno == EINTR) {   // 被捕捉的信号中断，进行下次循环 可继续获取客户端连接（防止父进程结束后不能连接）
                continue;
            }
            perror("accept:");
            exit(-1);
        }

        // 对于每一个连接，创建一个子进程跟客户端通信
        pid_t pid = fork();
        if(pid == 0){

            // 子进程：获取客户端信息
            char clientIP[16];  // 客户端IP地址
            inet_ntop(AF_INET, &clientaddr.sin_addr.s_addr, clientIP, sizeof(clientIP)); // 将客户端IP地址由网络字节序转换成点分十进制， 存储到字符串数组（第三个参数）中
            unsigned short clientPort = ntohs(clientaddr.sin_port);  // 客户端端口由网络序转为字节序
            printf("***********************************\n");
            printf("client ip is %s, port is %d\n",clientIP,clientPort);

            // 通信
            char recvBuf[1024] = {0}; // 服务器接收缓存区
            while (1)
            {
                memset(recvBuf, 0, sizeof(recvBuf)); // 清空接收缓存
                // 获取客户端的数据
                int len = read(cfd, &recvBuf, sizeof(recvBuf));

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

                // 给客户端发送数据
                len = write(cfd, recvBuf, strlen(recvBuf));
                /* if (len == -1) {
                    perror("write");
                    break;
                } else {
                    printf("send message to client: %s\n", recvBuf);
                } */
            } 
            close(cfd);  // 关闭用于通信的文件描述符
            exit(0);  // 退出当前子进程
        }
    }
    close(lfd);
    return 0;
}