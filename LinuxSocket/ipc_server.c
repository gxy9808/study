
// 本地套接字通信 -TCP服务器端
#include<stdio.h>  
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<sys/un.h>

int main() {

    // 每次将套接字文件删除后，重新绑定
    unlink("server.sock");

    // 1.创建监听的套接字
    int lfd = socket(AF_LOCAL, SOCK_STREAM, 0);

    if(lfd == -1){
        perror("socket");
        exit(-1);
    }

    // 2. 监听的套接字绑定本地套接字文件
    struct sockaddr_un addr; // 本地地址结构
    addr.sun_family = AF_LOCAL; // 地址族协议
    strcpy(addr.sun_path, "server.sock"); // 拷贝套接字文件的路径
    int ret = bind(lfd, (struct sockaddr*)&addr, sizeof(addr));
    if(ret == -1){
        perror("bind");
        exit(-1);
    }
    
    // 3.监听
    ret = listen(lfd, 8);
    if(ret == -1){
        perror("listen");
        exit(-1);
    }

    // 4.等待并接受连接请求
    struct sockaddr_un cliaddr;
    int len = sizeof(cliaddr);
    int cfd = accept(lfd, (struct sockaddr*)&cliaddr, &len);
    if(cfd==-1){
        perror("accept:");
        exit(-1);
    }

    // 打印客户端的套接字文件名称
    printf("client socket filename:%s\n", cliaddr.sun_path);

    // 5.与客户端通信
    while (1)
    {
        char recvBuf[128] = {0};
        memset(recvBuf, 0, sizeof(recvBuf)); // 清空接收缓存
        // 读取客户端的数据
        int len = recv(cfd, recvBuf, sizeof(recvBuf),0);

        if(len==-1){
            perror("recv:");
            exit(-1);
        }else if (len==0){
            printf("client is closed..\n");
            break;
        }else if(len>0){
            printf("receive data from client:%s\n", recvBuf);
            send(cfd, recvBuf, len, 0);
        }  
    }
    close(cfd);
    close(lfd);
    return 0;
}

