#pragma once

#include "LoggerQueue.hpp"
#include <string>
#include <time.h>

using namespace std;

#define STR_SIZE 1024

/*
* 宏函数
* 1.先获取单例对象
* 2.组织参数
* 3.写入日志文件
*/
#define RPC_LOG_INFO(logmsgformat, ...)                       \
    do                                                        \
    {                                                         \
        RpcLogger &logger = RpcLogger::get_instance();        \
        char str[STR_SIZE] = {0};                             \
        snprintf(str, STR_SIZE, logmsgformat, ##__VA_ARGS__); \
        logger.log_info(str);                                 \
    } while (0);

#define RPC_LOG_ERROR(logmsgformat, ...)                      \
    do                                                        \
    {                                                         \
        RpcLogger &logger = RpcLogger::get_instance();        \
        char str[STR_SIZE] = {0};                             \
        snprintf(str, STR_SIZE, logmsgformat, ##__VA_ARGS__); \
        logger.log_error(str);                                \
    } while (0);

#define RPC_LOG_FATAL(logmsgformat, ...)                      \
    do                                                        \
    {                                                         \
        RpcLogger &logger = RpcLogger::get_instance();        \
        char str[STR_SIZE] = {0};                             \
        snprintf(str, STR_SIZE, logmsgformat, ##__VA_ARGS__); \
        logger.log_fatal(str);                                \
    } while (0);

// 日志等级
enum LogLevel
{
    INFO,  // 普通信息
    ERROR, // 错误信息
    FATAL, // 毁灭信息
};

//日志系统 （单例模式）
class RpcLogger
{
public:
    void log_info(string msg);
    void log_error(string msg);
    void log_fatal(string msg);

    static RpcLogger &get_instance()  // 获取日志的单例  （在类外实现的静态成员不需要加static）
    {
        static RpcLogger instance;
        return instance;
    }

private:
    //设置日志级别
    void set_level(LogLevel level);

    //写日志， 把日志信息写入到队列缓冲区中
    void log(string msg);

private:
    RpcLogger();
    RpcLogger(const RpcLogger &) = delete;
    RpcLogger(RpcLogger &&) = delete;
    RpcLogger &operator=(const RpcLogger &) = delete;

private:
    int log_level_;                 // 日志级别
    LoggerQueue<string> log_queue_; //日志缓冲队列
};