#include "db.h"
#include <muduo/base/Logging.h>

// 数据库配置信息
static string server = "127.0.0.1";
static string user = "root";
static string password = "467219";
static string dbname = "mychat";
// 初始化数据库连接
MySQL::MySQL()
{
    // 1. 创建一个MySQL 连接句柄（MYSQL*），分配内存，初始化连接所需的内部结构；
    // 2. 只是初始化对象，并没有真正连接数据库（只是准备工作）；
    conn_ = mysql_init(nullptr);
}

// 释放数据库连接资源
MySQL::~MySQL()
{
    if (conn_ != nullptr)
    {
        mysql_close(conn_);
    }
}
// 连接数据库
bool MySQL::connect()
{
    MYSQL *p = mysql_real_connect(conn_, server.c_str(), user.c_str(),
                                  password.c_str(), dbname.c_str(), 3306, nullptr, 0);
    if (p != nullptr)
    {
        // C和C++代码默认的编码字符是ASCTT，如果不设置，从MVSOL上拉下来的中文显示"？"
        // 设置本次数据库连接的字符编码为 GBK。
        mysql_query(conn_, "set names gbk");
        LOG_INFO << "connect mysql success!";
    }
    else
    {
        LOG_INFO << "connect mysql fail!";
    }
    return p;
}

// 更新操作
bool MySQL::update(string sql)
{
    if (mysql_query(conn_, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "更新失败! ";
        return false;
    }
    return true;
}

// 査询操作
MYSQL_RES *MySQL::query(string sql)
{
    if (mysql_query(conn_, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "查询失败! ";
        return nullptr;
    }
    return mysql_use_result(conn_);
}

// 获取连接
MYSQL *MySQL::getConnection()
{
    return conn_;
}