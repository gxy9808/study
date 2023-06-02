// poll 服务器端通信
#include<stdio.h>  
#include<stdlib.h> 
#include<string.h>
#include<unistd.h>

#include<sys/types.h>
#include<sys/socket.h>
#include <arpa/inet.h>
#include<poll.h>


int main(){

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

    // 初始化检测的文件描述符数组
    struct pollfd fds[1024];
    for (int i = 0; i < 1024; i++)
    {
        fds[i].fd = -1; // 方便判断该文件描述符是否可用
        fds[i].events = POLLIN; // 检测文件描述符的读事件
    }
    // 把监听的文件描述符加到该参数数组中
    fds[0].fd = lfd;

    int nfds = 0; // 参数数组中最后一个有效元素的下标
    
    while (1)
    {
        // 调用poll系统函数，让内核检测哪些文件描述符有数据（rdset从用户态拷贝到内核态）
        int ret = poll(fds, nfds+1, -1);
        if(ret==-1)
        {
            perror("poll");
            exit(-1);
        } else if(ret == 0){
            continue; // 继续检测
        } else if(ret > 0){
            // 检测到有文件描述符的有事件发生，即检测events中是否包含POLLIN
            if(fds[0].revents & POLLIN) {
                // 表示有新的客户端连接进来
                struct sockaddr_in clientaddr;
                int len = sizeof(clientaddr);
                int cfd = accept(lfd,(struct sockaddr*)&clientaddr, &len);
                
                // 将通信的文件描述符加入到文件描述符数组中
                for (int i = 1; i < 1024; i++)
                {
                    if(fds[i].fd == -1) {
                        fds[i].fd = cfd;
                        fds[i].events = POLLIN;  // 默认检测读事件
                        nfds = nfds > i ? nfds : i;  // 更新最大文件描述符的索引
                        //printf("nfds = %d\n", nfds);
                        break;
                    }
                }
            }
            // 遍历文件描述符数组，检测剩余的文件描述符是否有事件发生 （从lfd+1开始，因为监听的文件描述符在数组开头）
            for (int i = 1; i <= nfds ; i++) // i为数组索引
            {   
                if((fds[i].fd!= -1) && (fds[i].revents & POLLIN)){
                    // 该文件描述符有读事件发生，对应的客户端有数据发送
                    char buf[1024] = {0};
                    int len = read(fds[i].fd, buf, sizeof(buf));
                    if(len == -1){
                        perror("read");
                        exit(-1);
                    } else if(len==0){
                        // 客户端断开连接
                        printf("client is closed\n");
                        close(fds[i].fd);
                        // 检查关闭的客户端是否为索引nfds下对应的描述符，若是，需要更新nfds
                        if(i==nfds){
                            for (int j = nfds-1; j>=0; j--)
                            {
                                if(fds[j].fd!=-1){
                                    nfds = j;
                                    break;
                                }
                            }
                        }
                        fds[i].fd = -1; // 已断开连接，应将该文件描述符清空
                    } else if(len > 0){
                        // 收到数据
                        printf("read buf = %s\n", buf);
                        write(fds[i].fd, buf, strlen(buf)+1); // 把字符串文件结束符也返回
                    }

                }
            }
        }
    }
    close(lfd);
    return 0;
    
}; 


/* 
bug：
检测文件描述符是否有事件发生时，使用了 fds[i].events & POLLIN 对读事件进行检测，
导致读取客户端数据时只能读取一次 并开始出现堵塞

 */