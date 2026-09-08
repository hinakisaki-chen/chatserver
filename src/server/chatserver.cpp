#include "chatserver.h"
#include "chatservice.h"

#include <functional>
#include <json.hpp>
#include <iostream>

using namespace std;
using namespace placeholders;
using json = nlohmann::json;

// 初始化聊天服务器对象
ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &nameArg)
    : server_(loop, listenAddr, nameArg), loop_(loop)
{
    // 注册链接回调
    server_.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));
    // 注册消息回调
    server_.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));
    // 设置线程数量
    server_.setThreadNum(4);
}

// 启动服务
void ChatServer::start()
{
    server_.start();
}
// 上报连接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    // 客户断开连接
    if (!conn->connected())
    {
        ChatService::instance()->clientClosException(conn);// 从这里区分一下和回调函数的使用区别
        conn->shutdown();
    }
}

// 上报读写事件相关信息的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn,
                           Buffer *buffer,
                           Timestamp time)
{ 
    string buf = buffer->retrieveAllAsString();
    // 数据的反序烈化
    json js = json::parse(buf);
    // 通过js["msgid”]获取=>回调函数，业务handler=>conn js time
    // 达到的目的:完全解耦网络模块的代码和业务模块的代码
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>()); // js["msgid"]是一个json实例对象,强制类型转换get方法获取的对象为int
    // msgHandler是chatservice.h中定义的function类型
    // 回调消息绑定好的业务处理器，热行业务处理
    msgHandler(conn, js, time);
}
