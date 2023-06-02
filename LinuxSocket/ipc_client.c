// 本地套接字通信 -TCP客户端
#include<stdio.h>  
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<sys/un.h>

int main() {

    unlink("client.sock");

    // 1.创建套接字
    int cfd = socket(AF_LOCAL, SOCK_STREAM, 0);

    if(cfd == -1){
        perror("socket");
        exit(-1);
    }

    // 2. 套接字绑定本地套接字文件
    struct sockaddr_un addr; // 本地地址结构
    addr.sun_family = AF_LOCAL; // 地址族协议
    strcpy(addr.sun_path, "client.sock"); // 指定套接字文件
    int ret = bind(cfd, (struct sockaddr*)&addr, sizeof(addr)); // 绑定成功之后，指定的sun_path中的套接字文件会自动生成
    if(ret == -1){
        perror("bind");
        exit(-1);
    }
    
    // 3.连接服务器
    struct sockaddr_un seraddr;
    seraddr.sun_family = AF_LOCAL;
    strcpy(seraddr.sun_path, "server.sock"); 
    ret = connect(cfd,(struct sockaddr*)&seraddr, sizeof(seraddr));
    if(ret == -1){
        perror("connetc");
        exit(-1);
    }

    // 打印服务器的套接字文件名称
    printf("server socket filename:%s\n", seraddr.sun_path);

    // 4.与服务端通信
    int num = 0;
    
    while (1)
    {   
        // 发送数据
        char sendBuf[128] = {0};
        sprintf(sendBuf,"%d\n",num++);
        send(cfd, sendBuf, sizeof(sendBuf), 0);
        printf("send data to server:%s\n", sendBuf);

        // 接收数据
        char recvBuf[128] = {0};
        int len = recv(cfd, recvBuf, sizeof(recvBuf),0);
        if(len==-1){
            perror("recv:");
            exit(-1);
        }else if (len==0){
            printf("server is closed..\n");
            break;
        }else if(len>0){
            printf("receive data from server:%s\n", recvBuf);
        } 
        sleep(1); 
    }
    close(cfd);
    return 0;
}


