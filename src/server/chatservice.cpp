#include "chatservice.h"
#include "public.h"
#include "user.h"
#include "usermodel.h"
#include "offlinemessagemodel.h"

#include <muduo/base/Logging.h>
#include <vector>
#include <iostream>

using namespace std;
using namespace placeholders;

ChatService *ChatService::instance()
{
    static ChatService chatservice;
    return &chatservice;
}

// 注册消息和对应的handleг回调操作
ChatService::ChatService()
{
    msgHandlerMap_[LOGIN_MSG] = std::bind(&ChatService::login, this, _1, _2, _3);
    msgHandlerMap_[LOGINOUT_MSG] = std::bind(&ChatService::loginout, this, _1, _2, _3);
    msgHandlerMap_[REG_MSG] = std::bind(&ChatService::reg, this, _1, _2, _3);
    msgHandlerMap_[ONE_CHAT_MSG] = std::bind(&ChatService::oneChat, this, _1, _2, _3);
    msgHandlerMap_[ADD_FRIEND_MSG] = std::bind(&ChatService::addFriend, this, _1, _2, _3);

    msgHandlerMap_[CREATE_GROUP_MSG] = std::bind(&ChatService::createGroup, this, _1, _2, _3);
    msgHandlerMap_[ADD_GROUP_MSG] = std::bind(&ChatService::addGroup, this, _1, _2, _3);
    msgHandlerMap_[GROUP_CHAT_MSG] = std::bind(&ChatService::groupChat, this, _1, _2, _3);

    // 连接redis服务器
    if(redis_.connect())
    {
        // 设置上报消息的回调
        redis_.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

// 获取对应消息的处理器handler
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志，即msgid没有对应的回调
    auto it = msgHandlerMap_.find(msgid);
    if (it == msgHandlerMap_.end())
    {
        // 返回一个默认的处理器，空操作， 防止使用error使得网络模块处理服务模块抛出的异常
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp)
        {
            LOG_ERROR << "msgid:" << msgid << " can not find handler.";
        };
    }
    return msgHandlerMap_[msgid];
}

// 以下是如果事情发生了该做什么事情，但是什么时候发生不知道，要通过上面绑定的回调函数
// 处理登入业务
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"].get<int>(); // 不要忘了字符串强转成int
    string pwd = js["password"];

    User user = usermodel_.query(id);
    if (user.getId() == id && user.getPassword() == pwd)
    {
        if (user.getState() == "online")
        {
            // 用户已经登入，不允许重复登入
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 1;
            response["errmsg"] = "该账号已登入";
            conn->send(response.dump());
        }
        else
        {
            // 登入成功，记录用户连接信息
            {
                lock_guard<mutex> lock(connMutex_);
                userConnMap_.insert({id, conn}); // 这个记录所有线程用户连接，需要处理线程安全问题
                // 因为muduo中用户消息回调函数onMessage函数会被多个线程调用，相应的这个回调函数也会被多个线程调用
            }

            // id用户登录成功后，向redis订阅channel(id)
            // 这是本程序内双方约定的规则，id用户对自己id号的通道感兴趣，其他用户向该用户发消息时，发到redis对应该id的通道上
            redis_.subscribe(id);

            // 下面的不用互斥锁因为user和response都是该线程的局部变量，usermodel_操作的互斥由mysql保证，并且要保证多线程效率
            // 登入成功，更新用户状态信息
            user.setState("online"); // 数据层和业务层中间的ORM层更新（临时的user对象）
            usermodel_.updateState(user); // 数据层数据库更新

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0; // 约定0为无错误
            response["id"] = user.getId();
            response["name"] = user.getName();
            // 查询该用户是否有离线消息
            vector<string> vec = offlinemsgmodel_.query(id);
            if (!vec.empty())
            {
                response["offlinemsg"] = vec;
                // 读取该用户的离线消息后，把该用户的离线消息删除掉
                offlinemsgmodel_.remove(id);
            }
            // 查询该用户的好友信息并返回
            vector<User> userVec = friendmodel_.query(id);
            if (!userVec.empty())
            {
                vector<string> vec2;
                for (User &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }

            // 查询用户的群组信息
            vector<Group> groupVec = groupmodel_.queryGroups(id);
            if (!groupVec.empty())
            {
                vector<string> groupV;
                for (Group &group : groupVec)
                {
                    json groupjs;
                    groupjs["id"] = group.getId();
                    groupjs["groupname"] = group.getName();
                    groupjs["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for (GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    groupjs["users"] = userV;
                    groupV.push_back(groupjs.dump());
                }
                response["groups"] = groupV;
            }

            conn->send(response.dump());
        }
    }
    else
    {
        // 用户名不存在或密码错误， 登入失败
        if (user.getId() == id)
        {
            // 密码错误
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "密码错误";
            conn->send(response.dump());
        }
        else
        {
            // 用户不存在
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 3;
            response["errmsg"] = "用户不存在";
            conn->send(response.dump());
        }
    }
}
// 处理注册业务
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPassword(pwd);
    bool state = usermodel_.insert(user); // 主键自动生成，状态默认关，传入引用，传出前赋值id
    if (state)
    {
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0; // 约定0为无错误，1为有错误
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        // 注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
}

// 处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(connMutex_);
        auto it = userConnMap_.find(userid);
        if(it != userConnMap_.end())
        {
            userConnMap_.erase(it);
        }
    }

    // 用户注销，相当于下线，在redis中取消订阅通道
    redis_.unsubscribe(userid);

    // 更新用户状态信息
    User user(userid, "", "", "offline");
    usermodel_.updateState(user);// 这句话的mysql语句只需要userid和state
}

// 处理客户端异常退出
void ChatService::clientClosException(const TcpConnectionPtr &conn)
{
    // 关键利用map查找该conn对应的id，再通过id查询数据库修改状态
    User user;
    {
        lock_guard<mutex> lock(connMutex_); // 操作map表注意线程安全问题
        for (auto it = userConnMap_.begin(); it != userConnMap_.end(); ++it)
        {
            if (it->second == conn)
            {
                user.setId(it->first);
                userConnMap_.erase(it);
                break;
            }
        }
    }

    // 更新用户状态信息
    if (user.getId() != -1)
    {
        // 用户注销，相当于下线，在redis中取消订阅通道
        redis_.unsubscribe(user.getId());

        user.setState("offline");
        usermodel_.updateState(user);
    }
}

// 服务器异常，业务重置方法
void ChatService::reset()
{
    usermodel_.resetState();
}

// 处理用户一对一聊天
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // cerr << "client send raw = " << js << __FUNCTION__ << __LINE__ << endl;

    int toid = js["to"].get<int>();

    {
        lock_guard<mutex> lock(connMutex_);
        auto it = userConnMap_.find(toid);
        if (it != userConnMap_.end())
        {
            // toid在线 转发消息    服务器主动将消息通过对应conn推送给toid用户
            it->second->send(js.dump());
            return;
        }
    }

    // 查询toid是否在线，若在线且map中找不到，则说明这两个用户不在同一个服务器上,向redis对方id号通道中发布消息
    User user = usermodel_.query(toid);
    if(user.getState() == "online")
    {
        redis_.publish(toid, js.dump());
        return;
    }
    
    // toid不在线，存储离线消息
    offlinemsgmodel_.insert(toid, js.dump());
}

// 添加好友业务     msgid   id  friendid
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 存储好友信息
    friendmodel_.insert(userid, friendid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建的群组消息
    Group group(-1, name, desc); // 数据库还没创建，id号还没生成
    if(groupmodel_.createGroup(group))  // 引用传参，传出时有id号
    {
        // 存储群组创建人信息
        groupmodel_.addGroup(userid, group.getId(), "creator");
    }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    groupmodel_.addGroup(userid, groupid, "normal");
}
// 群组聊天业务 给这个groupid的里面除了userid的所有人发消息
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec =  groupmodel_.queryGroupUsers(userid, groupid);
    // 保证map的线程安全问题
    lock_guard<mutex> lock(connMutex_);
    for(int id : useridVec)
    {
        auto it =userConnMap_.find(id); // 获取服务器和这个userid的连接
        if(it != userConnMap_.end())
        {
            // 转发群消息
            it->second->send(js.dump());
        }
        else{
            // 查询id是否在线，若在线且map中找不到，则说明这两个用户不在同一个服务器上,向redis对方id号通道中发布消息
            User user = usermodel_.query(id);
            if(user.getState() == "online")
            {
                redis_.publish(id, js.dump());
                return;
            }

            // 该用户不在线，存储离线群消息
            offlinemsgmodel_.insert(id, js.dump());
        }

    }
}

// 从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    lock_guard<mutex> lock(connMutex_);
    auto it = userConnMap_.find(userid);
    if(it != userConnMap_.end())
    {
        it->second->send(msg);
        return;
    }

    // 存储该用户的离线消息
    // 也可能a用户向另一个服务器b用户发消息b在线，但在a向redis发送和b接收之间b下线了
    offlinemsgmodel_.insert(userid, msg);
}
