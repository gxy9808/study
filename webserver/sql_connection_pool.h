#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_


#include <stdio.h>
#include <list>
#include <mysql/mysql.h>  // sudo apt-get install libmysqlclient-dev
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "locker.h"

using namespace std;


// 使用局部静态变量懒汉模式创建连接池
class connection_pool
{
public:
    MYSQL *GetConnection();               // 获取数据库连接
    bool ReleaseConnection(MYSQL *conn);  // 释放连接
    int GetFreeConn();                    // 获取当前空闲连接数
    void DestroyPool();                   // 销毁所有连接

    //局部静态变量单例模式
    static connection_pool * GetInstance(); 

    // 构造初始化
    void init(string url, string User, string PassWord, string DBName, int Port, unsigned int MaxConn, int close_log);


private:
    connection_pool();
    ~connection_pool();

    int m_MaxConn;    // 最大连接数
    int m_CurConn;    // 当前已使用的连接数
    int m_FreeConn;   // 当前空闲的连接数
    locker lock;
    list<MYSQL *> connList;  // 连接池
    sem reserve;

public:
    string m_url;           //主机地址
    string m_port;          //数据库端口号
    string m_User;          //登陆数据库用户名
    string m_PassWord;      //登陆数据库密码
    string m_DatabaseName;  //使用数据库名
    int m_close_log;        // 日志开关
};


// RAII机制释放数据库连接(将数据库连接的获取与释放通过RAII机制封装，避免手动释放)
class connectionRAII{
public:
    //双指针对MYSQL *con修改(数据库连接本身是指针类型，所以参数需要通过双指针才能对其进行修改)
    connectionRAII(MYSQL **con, connection_pool *connPool);
    ~connectionRAII();

private:
    MYSQL *conRAII;
    connection_pool *poolRAII;
};


#endif
