#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "group.h"
#include "user.h"
#include "public.h"

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;
// 显示当前登录成功用户的基本信息
void showCurrentUserData();

// 接收线程，收消息和发消息不是一个线程，不发消息时，线程阻塞，保证也能接受消息
void readTaskHandler(int clientfd);
// 获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime();
// 控制主菜单页面程序
bool isMainMenuRunning = false;

// 主聊天页面程序
void mainMenu(int clientfd);
void help(int clientfd, string str);
void chat(int clientfd, string str);
void addfriend(int clientfd, string str);
void creategroup(int clientfd, string str);
void addgroup(int clientfd, string str);
void groupchat(int clientfd, string str);
void loginout(int clientfd, string str);
// 接收线程
void readTaskHandler(int clientfd);


// 聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char **argv)
{
    // 判断命令参数的个数，因为启动的时候需要在命令行手动输入服务器端的IP地址和端口号
    if(argc < 3)
    {
        cerr << "command invalid! example: ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == clientfd)
    {
        cerr << "socket create error" << endl;
        exit(-1);
    }

    // 填写client需要连接的server信息ip+port
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_port = htons(port);

    // client和server进行连接
    if ( -1 == connect(clientfd, (sockaddr*)&server, sizeof(sockaddr_in)))
    {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    // main线程用于接收用户输入， 负责发送数据
    for( ; ; )
    {
        // 显示首页面菜单 登录 注册 退出
        cout << "=====================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "=====================" << endl;
        cout << "choice:";
        int choice = 0;
        cin >> choice;
        cin.get(); // 读掉缓冲区残留的回车

        switch(choice)
        {
            case 1: // login业务
            {
                int id = 0;
                char pwd[50] = {0};
                cout << "userid:";
                cin >> id;
                cin.get(); // 读掉缓冲区残留的回车
                cout << "userpassword:";
                cin.getline(pwd, 50);

                json js;
                js["msgid"] = LOGIN_MSG;
                js["id"] = id;
                js["password"] = pwd;
                string request = js.dump();

                int len =send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if(len == -1)
                {
                    cerr << "send login msg error:" << request <<endl;
                }
                else
                {
                    char buffer[1024] = {0};
                    len = recv(clientfd, buffer, 1024, 0);
                    if(len == -1)
                    {
                        cerr << "recv login response error" << endl;
                    }
                    else 
                    {
                        json responsejs = json::parse(buffer);
                        if(responsejs["errno"].get<int>() != 0) // 登陆失败
                        {
                            cerr << responsejs["errmsg"] << endl;
                        }
                        else    // 登陆成功
                        {
                            // 记录当前用户id和name
                            g_currentUser.setId(responsejs["id"].get<int>());
                            g_currentUser.setName(responsejs["name"]);

                            // 记录当前用户的好友列表信息
                            if(responsejs.contains("friends"))// 看返回json是否包含friends字段，是否有好友
                            {
                                // 初始化，防止程序一直运行时登出再登入login多次执行重复记录多次
                                g_currentUserFriendList.clear();

                                vector<string> vec = responsejs["friends"];
                                for(string &str : vec)
                                {
                                    json js =json::parse(str);
                                    User user;
                                    user.setId(js["id"].get<int>());
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    g_currentUserFriendList.push_back(user);
                                }
                            }

                            // 记录当前用户的群组列表信息
                            if(responsejs.contains("groups"))
                            {
                                // 初始化，防止程序一直运行时登出再登入login多次执行重复记录多次
                                g_currentUserGroupList.clear();

                                vector<string> vec1 = responsejs["groups"];
                                for(string &str : vec1)
                                {
                                    json js =json::parse(str);
                                    Group group;
                                    group.setId(js["id"].get<int>());
                                    group.setName(js["groupname"]);
                                    group.setDesc(js["groupdesc"]);
                                    // 获取该用户所有组信息

                                    // 获取每个组中的组员信息（除了自己，在sql语句上设计）
                                    vector<string> vec2 = js["users"];
                                    for(string &userstr : vec2)
                                    {
                                        GroupUser user;
                                        json gujs = json::parse(userstr);
                                        user.setId(gujs["id"].get<int>());
                                        user.setName(gujs["name"]);
                                        user.setState(gujs["state"]);
                                        user.setRole(gujs["role"]);
                                        group.getUsers().push_back(user);
                                    }

                                    g_currentUserGroupList.push_back(group);
                                }
                            }
                            // 显示登录用户的基本信息
                            showCurrentUserData();

                            // 显示当前用户离线消息 个人聊天信息或群组消息
                            if(responsejs.contains("offlinemsg"))
                            {
                                vector<string> vec = responsejs["offlinemsg"];
                                for(string &str : vec)
                                {
                                    json js =json::parse(str);
                                    // time + [id] + name + "said:" + xxx
                                    if ( ONE_CHAT_MSG == js["msgid"].get<int>())
                                    {
                                        cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                                            << "said: " << js["msg"].get<string>() << endl;
                                    }
                                    else
                                    {
                                        cout << "群消息[" << js["groupid"] << "]" << js["time"].get<string>() << " [" << js["id"] << "]" 
                                            << js["name"].get<string>() << "said: " << js["msg"].get<string>() << endl;
                                    }
                                }
                            }

                            //登录成功，启动接收线程负责接收数据，该线程只启动一次
                            static int threadnumber = 0;
                            if(threadnumber == 0)
                            {
                                std::thread readTask(readTaskHandler, clientfd);    // pthread_create
                                readTask.detach();  //pthread_detach
                                threadnumber++;
                            }
                            
                            //进入聊天主菜单页面
                            isMainMenuRunning = true;
                            mainMenu(clientfd);
                        } 
                    }
                }
                break;
            }
            case 2: // register业务
            {
                char name[50] = {0};
                char pwd[50] = {0};
                cout << "username:";
                cin.getline(name, 50); // 如果用cin>> 或scanf 这样方式输入内容不能有空格，否则会结束
                cout<< "userpassword:";
                cin.getline(pwd, 50);

                json js;
                js["msgid"] = REG_MSG;
                js["name"] = name;
                js["password"] = pwd;
                string request = js.dump();

                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if(len == -1)
                {
                    cerr << "send reg msg error:" << request << endl;
                }
                else
                {
                    char buffer[1024] = {0};
                    len = recv(clientfd, buffer, 1024, 0);
                    if(len == -1)
                    {
                        cerr << "recv reg response error" << endl;
                    }
                    else
                    {
                        json responsejs = json::parse(buffer);
                        if(responsejs["errno"].get<int>()!=0) // 注册失败
                        {
                            cerr<<name<<"us already exist, register error!" <<endl;
                        }
                        else // 注册成功
                        {
                            cout<<name<<"register success, userid is " << responsejs["id"]
                                << ", do not forget it" << endl;
                        }
                    }
                }
                break;
            }
            case 3: // quit业务
            {
                close(clientfd);
                exit(0);
            }
                
            default:
            {
                cerr << "invalid input!" <<endl;
                break;
            }
        }
    }
}

// 接收线程
void readTaskHandler(int clientfd)
{
    for(;;)
    {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0);
        if( len == -1 || len == 0)
        {
            close(clientfd);
            exit(-1);
        }

        // 接收ChatServer转发的数据，反序列化生成json数据对象
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if ( ONE_CHAT_MSG == msgtype)
        {
            cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                << "said: " << js["msg"].get<string>() << endl;
            continue;
        }
        else if(GROUP_CHAT_MSG == msgtype)
        {
            cout << "群消息[" << js["groupid"] << "]" << js["time"].get<string>() << " [" << js["id"] << "]" 
                << js["name"].get<string>() << "said: " << js["msg"].get<string>() << endl;
            continue;
        }
    }
}

// 注册系统支持的客户端命令处理
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令,格式help"},
    {"chat", "一对一聊天,格式chat:friendid:message"},
    {"addfriend", "添加好友,格式addfriend:friendid"},
    {"creategroup", "创建群组,格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组,格式addgroup:groupid"},
    {"groupchat", "群聊,格式groupchat:groupid:message"},
    {"loginout", "注销,格式loginout"}
};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}
};

void mainMenu(int clientfd)
{
    help(clientfd, "");

    char buffer[1024] = {0};
    while(isMainMenuRunning)
    {
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command; // 命令
        int idx = commandbuf.find(":"); // 看commandMap中是否有这个命令，找到第一个冒号的位置
        if(-1 == idx)
        {
            command = commandbuf;
        }
        else
        {
            command = commandbuf.substr(0, idx); // 没有冒号：整行全部当作命令，没有参数, 有冒号：从开头截取到冒号前，作为命令名
        }

        auto it = commandHandlerMap.find(command);
        if(it == commandHandlerMap.end())
        {
            cerr << "invalid input command!" << endl;
            continue;
        }


        // 调用命令绑定的事件处理器，mainMenu->commandHandlerMap->commandHandler
        it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx - 1));
    }
}

void help(int clientfd, string str)
{
    cout << "show command list:" << endl;
    for(auto &p : commandMap)
    {
        cout << p.first << " : " << p.second << endl;
    }
    cout << endl;
}

// 显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    cout << "================login user==================" << endl;
    cout << "current login user => id:" << g_currentUser.getId() << " name:" << g_currentUser.getName() << endl;
    cout << "----------------friend list----------------" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "----------------group list------------------" << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group &group : g_currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState()
                     << " " << user.getRole() << endl;
            }
        }
    }
    cout << "============================================" << endl;
}

std::string getCurrentTime()
{
    time_t now = time(nullptr);
    tm* t = localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(t, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// friendid
void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(-1 == len)
    {
        cerr << "send addfriend msg error -> " << buffer << endl;
    }
}

// friendid:message
void chat(int clientfd, string str)
{
    int idx = str.find(":");    // friendid:message
    if(-1 == idx)
    {
        cerr << "chat command inivalid!" << endl;
        return;
    }

    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx - 1);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["to"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    // cerr << "client send raw = " << js << __FUNCTION__ << __LINE__ << endl;

    // +1 的目的：把最后的 '\0' 也一起发送出去,`strlen()` 统计有效字符，不包含末尾 '\0'
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send chat msg error -> " << buffer << endl;
    }
}

// groupname:groupdesc
void creategroup(int clientfd, string str)
{
    int idx = str.find(":");
    if(-1 == idx)
    {
        cerr << "creategroup command inivalid!" << endl;
        return;
    }

    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx - 1);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();

    // cerr << "client send raw = " << js << __FUNCTION__ << __LINE__ << endl;

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send creategroup msg error -> " << buffer << endl;
    }
}

// groupid
void addgroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(-1 == len)
    {
        cerr << "send addgroup msg error -> " << buffer << endl;
    }
}

// groupid:message
void groupchat(int clientfd, string str)
{
    int idx = str.find(":");    // friendid:message
    if(-1 == idx)
    {
        cerr << "groupchat command inivalid!" << endl;
        return;
    }

    int groupid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx - 1);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    // cerr << "client send raw = " << js << __FUNCTION__ << __LINE__ << endl;

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send chat msg error -> " << buffer << endl;
    }
}

void loginout(int clientfd, string str)
{
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(-1 == len)
    {
        cerr << "send addgroup msg error -> " << buffer << endl;
    }
    else
    {
        isMainMenuRunning = false;
    }
}