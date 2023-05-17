#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h> 
#include <string.h> 
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/uio.h>
#include "locker.h"
#include <assert.h>
#include "lst_timer.h"
#include "log.h"
#include <mysql/mysql.h>

#include <map>
#include "sql_connection_pool.h"


// 前向声明
class sort_timer_lst;
class util_timer;

#define COUT_OPEN 1
const bool ET = true;   // addfd函数中的标志，是否设置为ET边沿触发
#define TIMESLOT 5      // 定时器周期：秒

class http_conn
{
public:

    static const int READ_BUFFER_SIZE = 2048;   // 读缓冲区的大小
    static const int WRITE_BUFFER_SIZE = 2048;  // 写缓冲区的大小
    static const int FILENAME_LEN = 200;        // 文件名的最大长度

    // 定义HTTP请求的一些状态信息
    enum METHOD   // HTTP请求方法，这里只支持GET （枚举类型）
    {
        GET = 0, 
        POST, 
        HEAD, 
        PUT, 
        DELETE, 
        TRACE, 
        OPTIONS, 
        CONNECT,
        PATH
    };
 
    // 解析客户端请求时，主状态机的状态   
    enum CHECK_STATE 
    { 
        CHECK_STATE_REQUESTLINE = 0,  // CHECK_STATE_REQUESTLINE:当前正在分析请求行
        CHECK_STATE_HEADER,           // CHECK_STATE_HEADER:当前正在分析头部字段
        CHECK_STATE_CONTENT           // CHECK_STATE_CONTENT:当前正在解析请求体
    };
    
    //服务器处理HTTP请求的可能结果，报文解析的结果   
    enum HTTP_CODE 
    { 
        NO_REQUEST,         // NO_REQUEST : 请求不完整，需要继续读取客户数据
        GET_REQUEST,        // GET_REQUEST : 表示获得了一个完成的客户请求
        BAD_REQUEST,        // BAD_REQUEST : 表示客户请求语法错误
        NO_RESOURCE,        // NO_RESOURCE : 表示服务器没有资源
        FORBIDDEN_REQUEST,  // FORBIDDEN_REQUEST : 表示客户对资源没有足够的访问权限
        FILE_REQUEST,       // FILE_REQUEST : 文件请求,获取文件成功
        INTERNAL_ERROR,     // INTERNAL_ERROR : 表示服务器内部错误
        CLOSED_CONNECTION   // CLOSED_CONNECTION : 表示客户端已经关闭连接了
    };
    
    // 从状态机的三种可能状态，即行的读取状态，分别表示 
    enum LINE_STATUS 
    { 
        LINE_OK = 0,  // 1.读取到一个完整的行
        LINE_BAD,     // 2.行出错
        LINE_OPEN     // 3.行数据尚且不完整
    };

    // 定义HTTP响应的一些状态信息
    const char* ok_200_title = "OK";
    const char* error_400_title = "Bad Request";
    const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
    const char* error_403_title = "Forbidden";
    const char* error_403_form = "You do not have permission to get file from this server.\n";
    const char* error_404_title = "Not Found";
    const char* error_404_form = "The requested file was not found on this server.\n";
    const char* error_500_title = "Internal Error";
    const char* error_500_form = "There was an unusual problem serving the requested file.\n";

public:
    // 构造函数 和 析构函数
    http_conn(){};
    ~http_conn(){};

public:
    //void init(int sockfd, const sockaddr_in & addr);  // 初始化套接字地址，函数内部会调用私有方法init
    void init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,  // 初始化新接收的连接
              int close_log, string user, string passwd, string sqlname);
    void close_conn();  // 关闭连接
    void process();     // 处理客户端请求, 对请求进行响应
    bool read();        // 非阻塞的读
    bool write();       // 非阻塞的写
    void def_fd();      // 定时器回调函数，被tick()调用

    sockaddr_in *get_address() { return &m_address; }   // 返回通信的socket地址

    void initmysql_result(connection_pool *connPool);  // 载入数据库表：将数据库中的用户名和密码载入到服务器的map中来,同步线程初始化数据库读取表
    //void initresultFile(connection_pool *connPool);    // CGI使用线程池初始化数据库表

private:

    void init();                    // 初始化连接其余的信息

    HTTP_CODE process_read();                  // 解析HTTP请求报文 （包含解析下面三部分）
    HTTP_CODE parse_request_line(char * text); // 解析请求首行
    HTTP_CODE parse_headers(char * text);      // 解析请求头部
    HTTP_CODE parse_content(char * text);      // 解析请求体
    LINE_STATUS parse_line();                  // 解析某一行（得到从状态机的状态）
    bool process_write(HTTP_CODE ret);         // 向m_write_buf写入响应报文数据
    HTTP_CODE do_request();                    // 解析具体的请求信息

    char* get_line() {                         // get_line用于将指针向后偏移，指向未处理的字符
        return m_read_buf + m_start_line; }    // m_start_line是已经解析的字符

    bool add_response(const char* format, ... );    // 写HTTP响应的一般函数（往写缓冲区中写入待发送的数据） 
    bool add_content(const char* content);          // 在HTTP响应中添加消息体内容
    bool add_content_type();                        // 在HTTP响应中添加消息体的数据格式
    bool add_status_line(int status, const char* title); // 在HTTP响应中添加状态行
    bool add_headers(int content_length );          // 在HTTP响应中添加消息头
    bool add_content_length( int content_length);   // 在HTTP响应中添加消息体长度
    bool add_linger();                              // 在HTTP响应中添加HTTP连接状态
    bool add_blank_line();                          // 在HTTP响应中添加空行

    void unmap();

public:
    static int m_epollfd;               // 所有的socket上的事件都被注册到同一个epoll对象中
    static int m_user_count;            // 统计用户的数量
    static int m_request_cnt;           // 接收到的请求次数
    static sort_timer_lst m_timer_lst;  // 定时器链表
    //static locker m_timer_lst_locker;  // 定时器链表互斥锁
    util_timer* timer;                   // 定时器
    MYSQL *mysql;                        // 数据库

private:

    int m_sockfd;           // 该HTTP连接的socket
    sockaddr_in m_address;  //通信的socket地址

    char m_read_buf[READ_BUFFER_SIZE];  // 读缓冲区
    int m_read_idx;       // 标识读缓冲区中已经读入的客户端数据的最后一个字节的下一个位置
    
    int m_checked_index;  // 当前正在分析的字符在读缓冲区的位置
    int m_start_line;     // 当前正在解析的行的起始位置

    char m_write_buf[ WRITE_BUFFER_SIZE ];  // 写缓冲区
    int m_write_idx;                        // 标识写缓冲区中已写入数据的位置

    METHOD m_method;      // 请求方法
    CHECK_STATE m_check_state;  // 主状态机当前所处的状态（解析状态）

    char* m_url;          // 客户端请求目标文件的文件名
    char m_real_file[FILENAME_LEN];  // 客户请求的目标文件的完整路径，其内容等于doc_root+m_url（doc_root是网站根目录）
    char* m_version;      // 协议版本，只支持HTTP1.1
    char* m_host;           // 主机名
    int m_content_length;   // HTTP请求的消息总长度(请求体长度，请求体在请求空行后面)
    bool m_linger;          // HTTP请求是否要保持连接
    char *m_doc_root;       // 网站资源的根目录

    char* m_file_address;      // 客户请求的目标文件被mmap到内存中的起始位置 (请求体映射的地址)
    struct stat m_file_stat;   // 目标文件的状态。判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息
    
    struct iovec m_iv[2];      // io向量机制iovec，定义采用writev来执行写操作所用的两个内存块
    int m_iv_count;            // 被写内存块的数量

    int bytes_to_send;              // 将要发送的数据的字节数
    int bytes_have_send;            // 已经发送的字节数

    int cgi;                       // 是否启用POST
    char *m_string;                // 存储请求消息体的数据

    map<string, string> m_users;   // 存储每一个连接的用户名和密码

    int m_TRIGMode;                // 是否为ET触发
    int m_close_log;

    char sql_user[100];            //
    char sql_passwd[100];          //
    char sql_name[100];            //


};



#endif