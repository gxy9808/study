
#include<stdio.h>  
#include<stdlib.h> 
#include<string.h>
#include<unistd.h>

#include<sys/types.h>
#include<sys/socket.h>
#include <arpa/inet.h>
#include<sys/time.h>
#include<sys/select.h>


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

    // 创建一个fd_set的集合，存放的是需要检测的文件描述符
    fd_set rdset, temp;   // temp为rdset的拷贝临时变量
    FD_ZERO(&rdset); // 把集合中的文件描述符全部置为0
    FD_SET(lfd, &rdset); // 把要监听的文件描述符置为1
    int maxfd = lfd;  //最大的文件描述符，初始化为需要监听的文件描述符
    
    while (1)
    {
        temp = rdset; // 让内核检测temp（因为内核检测返回后rdset会发生改变，可能会使原先要检测的fd置零）
        // 调用select系统函数，让内核检测哪些文件描述符有数据（rdset从用户态拷贝到内核态）
        int ret = select(maxfd+1, &temp, NULL, NULL,NULL);
        if(ret==-1)
        {
            perror("select");
            exit(-1);
        } else if(ret == 0){
            continue; // 继续检测
        } else if(ret > 0){
            // 检测到有文件描述符的对应的缓冲区数据发生改变

            // 判断经内核检测后rdset中监听的文件描述符是否为1
            if(FD_ISSET(lfd,&temp)) {
                // 表示有新的客户端连接进来
                struct sockaddr_in clientaddr;
                int len = sizeof(clientaddr);
                int cfd = accept(lfd,(struct sockaddr*)&clientaddr, &len);
                // 将用于通信的文件描述符加入到集合中
                FD_SET(cfd, &rdset);
                // 更新最大的文件描述符
                maxfd = maxfd > cfd ? maxfd : cfd;
            }
            // 遍历整个集合，看剩余的文件描述符是否发生变化 （从lfd+1开始，因为监听的文件描述符在最前面）
            for (int i = lfd + 1; i <= maxfd ; i++)
            {   // 判断文件描述符i在内核返回的rdset里标志位是否为1
                if(FD_ISSET(i, &temp)){
                    // 说明该文件描述符对应的客户端发来了数据
                    char buf[1024] = {0};
                    int len = read(i, buf, sizeof(buf));
                    if(len == -1){
                        perror("read");
                        exit(-1);
                    } else if(len==0){
                        // 客户端断开连接
                        printf("client is closed\n");
                        close(i);
                        FD_CLR(i, &rdset); // 已断开连接，应将该文件描述符清空
                    } else if(len > 0){
                        // 收到数据
                        printf("read buf = %s\n", buf);
                        write(i, buf, strlen(buf)+1); // 把字符串文件结束符也返回
                    }

                }
            }
        }
    }
    close(lfd);
    
};