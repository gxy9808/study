#include <mysql/mysql.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;


//****连接池代码实现****

connection_pool::connection_pool()
{
    this->m_CurConn = 0;
    this->m_FreeConn = 0;
}

//RAII机制销毁连接池
connection_pool::~connection_pool()
{
    DestroyPool();
}

connection_pool* connection_pool::GetInstance()
{
    static connection_pool connPool;
    return &connPool;
}

// 构造初始化
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, unsigned int MaxConn, int close_log)
{
    // 初始化数据库信息
    this->m_url = url;
    this->m_port = Port;
    this->m_User = User;
    this->m_PassWord = PassWord;
    this->m_DatabaseName = DBName;
    this->m_close_log = close_log;

    // 创建MaxConn条数据库连接
    for (int i = 0; i < MaxConn; i++)
    {
        MYSQL *con = NULL;
        con = mysql_init(con);

        if(con == NULL) {
            cout << "Error1:" << mysql_error(con);
            exit(1);
        }
        con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);  // c_str()函数返回一个指向正规C字符串的指针常量
        
        if(con == NULL) {
            cout << "Error2:" << mysql_error(con);
            exit(1);
        }

        // 更新连接池和空闲连接数量
        connList.push_back(con);
        ++m_FreeConn;
    }

    // 将信号量初始化为最大连接次数
    reserve = sem(m_FreeConn);  // 使用信号量实现多线程争夺连接的同步机制

    this->m_MaxConn = m_FreeConn;
}

// 当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL*connection_pool::GetConnection()
{
    MYSQL *con = NULL;

    if(connList.size()==0)
        return NULL;

    // 取出连接，信号量原子减1，为0则等待
    reserve.wait();

    lock.lock();

    // 获取连接池中的一个可用连接
    con = connList.front();
    connList.pop_front();

    --m_FreeConn;
    ++m_CurConn;

    lock.unlock();
    return con;
}


// 释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *con)
{
    if(con==NULL)
        return false;
    
    lock.lock();

    connList.push_back(con);
    ++m_FreeConn;
    --m_CurConn;

    lock.unlock();

    //释放连接原子加1
    reserve.post();
    return true;
}

// 销毁数据库连接池
void connection_pool::DestroyPool()
{
    lock.lock();
    if(connList.size() > 0)
    {
        // 通过迭代器遍历，关闭数据库连接
        list<MYSQL *>::iterator it;
        for (it = connList.begin(); it != connList.end(); ++it)
        {
            MYSQL *con = *it;
            mysql_close(con);
        }
        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear(); // 清空list
    }
    lock.unlock();
}

//当前空闲的连接数
int connection_pool::GetFreeConn()
{
    return this->m_FreeConn;
}

// RAII机制获取数据库连接并封装
connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool)
{
    // 不直接调用获取和释放连接的接口，将其封装起来，通过RAII机制进行获取和释放
    *SQL = connPool->GetConnection();

    conRAII = *SQL;
    poolRAII = connPool;
}

// 通过RAII机制进行数据库释放
connectionRAII::~connectionRAII()
{
    poolRAII->ReleaseConnection(conRAII);
}


// Unbuntu  mysql
// 登录数据库： mysql -u root -p
// use mywebdb;
// show tables;
// select * from user;
// 插入数据：INSERT INTO user(username, passwd) VALUES('name', 'passwd');
// 删除数据：delete from user where username = 'name';