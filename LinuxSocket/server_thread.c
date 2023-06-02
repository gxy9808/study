// TCP通信 多线程实现并发服务器

#include<stdio.h>  
#include<stdlib.h> 
#include<string.h>

#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>  // 线程

// 参数结构体:封装需要传入子线程创建函数的参数
struct sockInfo  
{
    int fd;          // 用于通信的文件描述符
    pthread_t tid;   // 线程号
    struct sockaddr_in addr;  // 客户端地址信息
};

// 规定最多有128个客户端连接 
struct sockInfo sockthreads[128];


// working函数接受 sockInfo结构体的参数
void* working(void* arg) {

    // 将void*类型转为sockInfo指针
    struct sockInfo* pinfo = (struct sockInfo*)arg;

    // 子线程和客户端通信
    // 获取客户端信息
    char clientIP[16];  // 客户端IP地址
    inet_ntop(AF_INET, &pinfo->addr.sin_addr.s_addr, clientIP, sizeof(clientIP)); // 将客户端IP地址由网络字节序转换成点分十进制， 存储到字符串数组（第三个参数）中
    unsigned short clientPort = ntohs(pinfo->addr.sin_port);  // 客户端端口由网络序转为字节序
    printf("***********************************\n");
    printf("client ip is %s, port is %d\n",clientIP,clientPort);

    // 接收客户端数据并返回
    char recvBuf[1024] = {0}; // 服务器接收缓存区
    while (1)
    {
        memset(recvBuf, 0, sizeof(recvBuf)); // 清空接收缓存
        // 获取客户端的数据
        int len = read(pinfo->fd, &recvBuf, sizeof(recvBuf));

        if(len==-1){
            perror("read:");
            exit(-1);
        }else if (len>0){
            printf("receive message from client:%s\n", recvBuf);
        }else if(len==0){
         // 表示客户端断开连接
            printf("clinet is closed...\n");
            break;
        }

        // 将接收转为大写字符串发给客户端
        for (int i = 0; i < len; i++)
        {
            recvBuf[i] = toupper(recvBuf[i]);
        }
        

        // 给客户端发送数据
        len = write(pinfo->fd, recvBuf, strlen(recvBuf));
        /* if (len == -1) {
            perror("write");
            break;
        } else {
            printf("send message to client: %s\n", recvBuf);
        } */
    } 
    close(pinfo->fd);  // 关闭用于通信的文件描述符
    return NULL;

}
 
int main(){

    // 创建用于监听的套接字
    int lfd = socket(AF_INET, SOCK_STREAM,0);
    if(lfd == -1) 
    {
        perror("socket"); 
        exit(-1);
    }


    // 绑定IP地址和端口
    struct sockaddr_in serveraddr; // 服务器socket地址结构体 
    serveraddr.sin_family = AF_INET;  
    inet_pton(AF_INET, "192.168.233.129", &serveraddr.sin_addr.s_addr); // 点分十进制→网络字节序
    serveraddr.sin_port = htons(9999); 

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

    // 初始化结构体参数数组的数据
    int max = sizeof(sockthreads)/sizeof(sockthreads[0]);
    for (int i = 0; i < max; i++)
    {
        bzero(&sockthreads[i], sizeof(sockthreads[i])); // 把所有sockInfo都初始化为0
        sockthreads[i].fd = -1;  // -1为无效文件描述符，说明此文件描述符可用
        sockthreads[i].tid = -1;
    }
    
    // 循环等待客户端连接, 一旦一个客户端连接进来，就创建一个子线程进行通信
    while (1)
    {
        struct sockaddr_in clientaddr;  // 客户端socket地址结构
        int len = sizeof(clientaddr);
        // 接收客户端连接后，得到用于通信的套接字
        int cfd = accept(lfd, (struct sockaddr*)&clientaddr, &len);  
        if(cfd==-1)
        {
            perror("accept:");
            exit(-1);
        }
        // 创建参数结构体指针
        struct sockInfo *pinfo; 
        for (int i = 0; i < max; i++)
        {
            // 从这个数组中寻找一个可用的sockInfo元素
            if(sockthreads[i].fd == -1)
            {    pinfo = &sockthreads[i];  // pinfo为指针，取数组元素的地址
                 break;
            }
            if(i==max-1){
                sleep(1);
                i = 0;//i--;
            }
        }

        pinfo->fd = cfd;
        memcpy(&pinfo->addr, &clientaddr, len);   // 内存资源拷贝
        
        
        //pthread_t tid;  // 线程号： 子线程创建之后，tid才会有值
        // 创建子线程
        //pthread_create(&tid, NULL, working, NULL);
        pthread_create(&pinfo->tid, NULL, working, pinfo);

        // 设置线程分离， 让当前线程结束后释放资源，不需要父线程回收
        pthread_detach(pinfo->tid);
    }
    close(lfd);
    return 0;
}