
// epoll的LT模式（水平触发）----epoll的默认方式
#include<stdio.h>  
#include<stdlib.h> 
#include<string.h>
#include<unistd.h>

#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<sys/epoll.h>


int main() {

    // 创建socket套接字
    int lfd = socket(PF_INET, SOCK_STREAM,0);

    struct sockaddr_in serveraddr; // 服务器socket地址结构体
    serveraddr.sin_family = AF_INET;  // 指定地址族 IPV4
    serveraddr.sin_addr.s_addr = INADDR_ANY; // 指定网络地址 0.0.0.0 表示绑定以太网卡和无线网卡等多个网卡的IP地址，当有客户端连接时，都可以获取到IP地址（仅服务器端可以这样使用，客户端不行）
    serveraddr.sin_port = htons(9999); // 指定并转换端口，主机字节序→网络字节序

    // 绑定本地IP地址和端口
    int ret = bind(lfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    if(ret==-1)
    {
        perror("bind");
        exit(-1);
    }

    // 监听
    ret = listen(lfd, 8);  
    if(ret==-1)
    {
        perror("listen");
        exit(-1);
    }

    // 创建一个epoll实例，在内核中创建一块空间，包含两个数据：1.需要检测的文件描述符信息（红黑树实现） 2.就绪列表，存放检测到数据发送改变的文件描述符信息（双向链表实现）
    int epfd = epoll_create(100);

    struct epoll_event epev; // 定义一个epoll检测事件
    // 对lfd的epoll检测事件
    epev.events = EPOLLIN; // 检测文件描述符的读事件
    epev.data.fd = lfd;
    // 将监听的文件描述符相关的检测信息添加到epoll实例中
    epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &epev);

    // epoll_wait函数的传出参数：接收内核检测后发生变化的文件描述符信息，存放于数组中
    struct epoll_event epevs[1024];

    while (1)
    {
        // ******检测发生变化的文件描述符
        // ******LT模式下，当要检测的文件描述符的接收缓冲区数据未读取完，epoll检测后会一直通知
        int ret =epoll_wait(epfd, epevs, 1024, -1 );  //返回发送变化的文件描述符的个数
        if(ret==-1)
        {
            perror("epoll_wait");
            exit(-1);
        }
        printf("ret =  %d\n",ret);

        // 遍历发生改变的文件描述符
        for (int i = 0; i < ret; i++)
        {
            int curfd = epevs[i].data.fd; //当前的发生改变的文件描述符
            if(epevs[i].data.fd == lfd) // 有客户端连接，监听的文件描述符发生变化
            {
                struct sockaddr_in clientaddr; // 客户端地址信息
                int len = sizeof(clientaddr);
                char clientIP[16] ={0};
                // 接收客户端连接 返回cfd
                int cfd = accept(lfd,(struct sockaddr*)&clientaddr, &len); 

                // 打印客户端信息
                printf("client IP:%s, Port：%d\n",
                    inet_ntop(AF_INET, &clientaddr.sin_addr.s_addr, clientIP, sizeof(clientIP)),
                    ntohs(clientaddr.sin_port));
                memset(clientIP, 0, sizeof(clientIP));
                // 创建对cfd的epoll检测事件
                epev.events = EPOLLIN;  // epev.events = EPOLLIN | EPOLLOUT
                epev.data.fd = cfd;
                // 将通信文件描述符cfd添加到epoll实例中
                epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &epev);
            } else {
                /* if(epev.events & EPOLLOUT) // 检测到写事件发生 （暂没实现）
                {
                    continue;
                } */
                // 读事件
                // 通信文件描述符发生变化，与客户端进行数据通信
                char buf[6] = {0};
                
                int len = read(curfd, buf, sizeof(buf));
                if(len == -1){
                    perror("read");
                    exit(-1);
                } else if(len==0){
                    // 客户端断开连接
                    printf("client is closed\n");
                    close(curfd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, curfd, NULL);
                } else if(len > 0){
                    // 收到数据
                    printf("receive data from client:%s\n", buf);
                    write(curfd, buf, strlen(buf)+1); // 把字符串文件结束符也返回
                }
            }
        }
    }
    close(lfd);
    close(epfd); // 关闭epoll实例
    return 0;
}

