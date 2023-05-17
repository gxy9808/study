#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "locker.h"
#include "threadpool.h"
#include <signal.h>
#include "http_conn.h"
#include "lst_timer.h"
#include "log.h"

//测试命令：./webbench -c 1000 -t 5 http://192.168.233.129:10000/index.html

#define MAX_FD 65535    // 最大的文件描述符个数
#define MAX_EVENT_NUMBER 10000 // 监听的最大的事件数量

static int pipefd[2];   // 管道文件描述符 0为读，1为写


//需要修改的数据库信息,登录名,密码,库名
string user = "root";
string password = "gxy123";
string databasename = "mywebdb";

// 初始设置参数
int Port = 9006;   // 默认
int sql_num = 8;   // 数据库连接池数量,默认8
int close_log = 0; // 关闭日志,默认不关闭
int TRIGMode = 0;  // 触发组合模式,默认listenfd LT + connfd LT
int log_write = 0;  // 日志写入方法， 默认同步

char* doc_root;

// 信号处理，添加信号捕捉
void addsig(int sig, void(handler)(int)){
    struct sigaction sa;           // 注册信号的一个参数
    memset(&sa, '\0', sizeof(sa)); // 清空
    sa.sa_handler = handler;       // 指定回调函数
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
    assert(sigaction( sig, &sa, NULL ) != -1 );  //
}

// 向管道写数据的信号捕捉回调函数
void sig_to_pipe(int sig){
    int save_errno = errno;
    int msg = sig;
    send( pipefd[1], ( char* )&msg, 1, 0 );
    errno = save_errno;
}

// 添加文件描述符到epoll中
extern void addfd(int epollfd,  int fd, bool one_shot, bool et);
// 从epoll中删除文件描述符
extern void removefd(int epollfd,  int fd);
// 在epoll中修改文件描述符
extern void modfd(int epollfd, int fd, int ev);
// 文件描述符设置非阻塞操作
extern void set_nonblocking(int fd);

int main(int argc, char* argv[]) {

    /* if(argc <= 1) {
        printf("按照如下格式运行：%s port_number\n", basename(argv[0]));
        exit(-1);
    } */

    // 创建一个数组用于保存所有的客户端信息
    http_conn* users = new http_conn[MAX_FD];
    // root文件夹路径

    char server_path[200];
    getcwd(server_path, 200);  // 复制当前工作目录的绝对路径到buffer中
    char root[6] = "/root";
    doc_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(doc_root, server_path);
    strcat(doc_root, root);  // 字符串连接

    // 初始化日志
    if (close_log == 0)
    {
        if(log_write == 1)  // 异步日志
            Log::get_instance()->init("./ServerLog", close_log, 2000, 800000, 800);
        else                // 同步日志
            Log::get_instance()->init("./ServerLog", close_log, 2000, 800000, 0);
    } 
    
    // 初始化数据库连接池
    connection_pool* connPool = connection_pool::GetInstance();
    connPool->init("localhost", user, password, databasename, 3306, sql_num, close_log);
    // 初始化数据库读取表
    users->initmysql_result(connPool);

    // 创建线程池，初始化线程池
    threadpool<http_conn> * pool = NULL; //(任务为http连接的任务)
    try{
        pool = new threadpool<http_conn>(connPool);
    } catch(...){
        exit(-1);
    }

    // 获取端口号
    int port = Port;
    //int port = atoi(argv[1]);   // argv[0]是程序名

    // socket通信
    // 创建一个用于监听的套接字
    int listenfd = socket(AF_INET, SOCK_STREAM,0);
    if(listenfd == -1) 
    {
        perror("socket"); 
        exit(-1);
    }
    // 设置端口复用
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 绑定本地IP和端口
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    int ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    if(ret==-1)
    {
        perror("bind");
        exit(-1);
    }

    // 监听
    listen(listenfd, 5);

    // 创建epoll对象 和 事件数组 （IO多路复用，同时检测多个事件）
    epoll_event events[MAX_EVENT_NUMBER];  // 结构体数组，接收检测后的数据
    int epollfd = epoll_create(5);

    // 将监听的文件描述符添加到epoll对象中
    addfd(epollfd, listenfd, false, false);  // 监听文件描述符不需要 ONESHOT & ET

    http_conn::m_epollfd = epollfd; // 设置http_conn的静态成员-该epoll对象 （静态成员 类共享）

    // *** 创建管道 ***
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    if(ret == -1){
        perror("socketpair");
        exit(-1);
    }
    set_nonblocking(pipefd[1]);               // 写管道非阻塞
    addfd(epollfd, pipefd[0], false, false);    // epoll检测读管道

    // 设置信号处理函数
    addsig(SIGPIPE, SIG_IGN);       // SIGPIE信号
    addsig(SIGALRM, sig_to_pipe);   // 定时器信号
    addsig(SIGTERM, sig_to_pipe);   // SIGTERM 关闭服务器
    
    alarm(TIMESLOT);                // 定时产生SIGALRM信号

    bool stop_server = false;       // 关闭服务器标志位
    bool timeout = false;           // 定时器周期已到
    
    // 主线程循环检测是否有事件发生
    while (!stop_server)
    {   
        // 检测是否有事件发生（文件描述符变化）
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1); // 内核检测事件发生后返回结果存放于事件数组events中
        if((num < 0) && (errno != EINTR)){  // num<0或者不是被中断
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        // 循环遍历事件数组
        for (int i = 0; i < num; i++)
        {
            int sockfd = events[i].data.fd;
            // *****当前事件检测的是监听fd发生变化，说明客户端连接进来
            if(sockfd == listenfd) {

                struct sockaddr_in client_address; // 客户端地址信息
                socklen_t client_addrlen = sizeof(client_address);
                // 接受客户端连接 
                int connfd = accept(listenfd,(struct sockaddr*)&client_address, &client_addrlen); 

                if (connfd < 0)
                {
                    LOG_ERROR("%s:errno is:%d", "accept error", errno);
                    continue;;
                }
                // 超过服务器的最大连接数
                if(http_conn::m_user_count >= MAX_FD) {
                    // 给客户端发送信息：服务器忙碌
                    close(connfd);
                    LOG_ERROR("%s", "Internal server busy");
                    continue;
                }
                // 将新的客户的数据初始化，放到数组中
                users[connfd].init(connfd, client_address, doc_root, TRIGMode, close_log, user, password, databasename);
            }
            // 读管道有数据，SIGALRM 或 SIGTERM信号触发
            // *****处理定时信号***********
            else if(sockfd == pipefd[0] && (events[i].events & EPOLLIN)){  
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if(ret == -1){
                    LOG_ERROR("%s", "Deal Siginal failure");
                    continue;
                }else if(ret == 0){
                    LOG_ERROR("%s", "Deal Siginal failure");
                    continue;
                }else{
                    for(int i = 0; i < ret; ++i){
                        switch (signals[i]) // 字符ASCII码
                        {
                        case SIGALRM:
                        // 用timeout变量标记有定时任务需要处理，但不立即处理定时任务
                        // 这是因为定时任务的优先级不是很高，我们优先处理其他更重要的任务。
                            timeout = true;
                            break;
                        case SIGTERM:
                            stop_server = true;
                            break;
                        }
                    }
                }
            }
            // *****事件检测到的是通信的fd发生变化
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))  
            {
                // 对方异常断开或者错误等事件
                users[sockfd].close_conn();
                http_conn::m_timer_lst.del_timer(users[sockfd].timer);  // 移除其对应的定时器
            }
            else if(events[i].events & EPOLLIN) {
                // 通信的fd发生变化，且检测到有读事件发生
                if(users[sockfd].read()) { // 一次性把所有数据都读完
                    LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
                    // 添加到线程池的工作队列中
                    pool->append(users + sockfd);
                }else {
                    //服务器端关闭连接，移除对应的定时器
                    users[sockfd].close_conn();
                    http_conn::m_timer_lst.del_timer(users[sockfd].timer);  // 移除其对应的定时器
                }
            } else if(events[i].events & EPOLLOUT ){ // 检测到写事件
                // 当检测到缓冲区没有满可以写数据时，则一次性写完所有数据发给客户端
                if(users[sockfd].write()) { 
                    LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
                }
                else
                {
                    users[sockfd].close_conn();
                    http_conn::m_timer_lst.del_timer(users[sockfd].timer);  // 移除其对应的定时器   
                }     
            } 
        }
        // 最后处理定时事件，I/O事件有更高的优先级。当然，这样做将导致定时任务不能精准的按照预定的时间执行。
        if(timeout) {
            // 定时处理任务，实际上就是调用tick()函数
            http_conn::m_timer_lst.tick();
            LOG_INFO("%s", "timer tick");
            // 因为一次 alarm 调用只会引起一次SIGALARM 信号，所以我们要重新定时，以不断触发 SIGALARM信号。
            alarm(TIMESLOT);
            timeout = false;    // 重置timeout
        }
    }
    close(epollfd);
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    delete []users;
    delete pool;
    return 0;
}


/* 
    oneshot指的某socket对应的fd事件最多只能被检测一次，不论你设置的是读写还是异常。
因为可能存在这种情况：如果epoll检测到了读事件，数据读完交给一个子线程去处理，如果
该线程处理的很慢，在此期间epoll在该socket上又检测到了读事件，则又给了另一个线程去处理,
则在同一时间会存在两个工作线程操作同一个socket。
ET模式指的是：数据第一次到的时刻才通知，其余时刻不再通知。如果读完了又来了新数据，epoll继续通知。
ET模式下可以通知很多次。监听socket不用设置为oneshot是因为只存在一个主线程去操作这个监听socket。
*/