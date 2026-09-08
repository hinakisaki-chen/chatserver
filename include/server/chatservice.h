#pragma once

// service服务，不要与服务器搞混

#include "json.hpp"
#include "usermodel.h"
#include "offlinemessagemodel.h"
#include "friendmodel.h"
#include "groupmodel.h"
#include "redis.h"

#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
#include <mutex>
using namespace std;
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;

// 处理消息的事件回调方法类型
using MsgHandler = std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp)>;

// 聊天服务器业务类  单例模式
class ChatService
{
public:
    // 获取单例对象
    static ChatService *instance();
    // 处理登入业务
    void login(const TcpConnectionPtr &conn, json &js, Timestamp time); // 参数和MsgHandler相同，将用来作为被绑定的回调函数
    // 处理注册业务
    void reg(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 处理注销业务
    void loginout(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 处理用户一对一聊天
    void oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 添加好友业务
    void addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 创建群组业务
    void createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 加入群组业务
    void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 群组聊天业务
    void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 获取对应消息的处理器handler
    MsgHandler getHandler(int msgid);
    // 处理客户端异常退出
    void clientClosException(const TcpConnectionPtr &conn);
    // 服务器异常，业务重置方法
    void reset();

    // 从redis消息队列中获取订阅的消息
    void handleRedisSubscribeMessage(int userid, string msg);

private:
    ChatService(); // 设为私有，防止构造

    // 存储消息id和对应的业务处理方法
    unordered_map<int, MsgHandler> msgHandlerMap_;
    // 存储在线用户的通信连接
    unordered_map<int, TcpConnectionPtr> userConnMap_;
    // 定义互斥锁，保证userConnMap_的线程安全
    mutex connMutex_;
    // 数据操作类对象
    UserModel usermodel_;
    OfflineMsgModel offlinemsgmodel_;
    FriendModel friendmodel_;
    GroupModel groupmodel_;

    // redis操作对象
    Redis redis_;
};
