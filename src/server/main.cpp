#include "chatserver.h"
#include "chatservice.h"

#include <iostream>
#include <signal.h>

using namespace std;

// 处理服务器ctrl+c结束后，重置user的状态信息,其他的服务器异常结束这个不会处理
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}

int main(int argc, char* argv[])
{
    std::string ip = "127.0.0.1";
    int port = 6000;

    // 如果传入了参数，覆盖默认值
    if (argc >= 2)
    {
        ip = argv[1];
    }
    if (argc >= 3)
    {
        port = atoi(argv[2]);
    }

    // 在终端服务器端按下 Ctrl + C 的那一刻，操作系统向当前进程发送 `SIGINT`，程序不会直接死掉，先走你写的 `resetHandler`
    /**
     * - 客户端断开连接 → **不会触发**
        - 程序崩溃段错误 segfault → 不是 SIGINT，不走这个函数
        - kill pid（不带参数）发送的是 SIGTERM，**不会触发 SIGINT**
        - kill -2 pid：手动发送 SIGINT，等价 Ctrl+C，**会触发**
     */
    signal(SIGINT, resetHandler);

    EventLoop loop;
    InetAddress addr(ip, port);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();
    return 0;
}