#pragma once
#include "global.h"


#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#undef ERROR
using SocketType = SOCKET;
#define INVALID_SOCKET_VALUE INVALID_SOCKET

#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

using SocketType = int;
#define INVALID_SOCKET_VALUE (-1)

#endif


namespace TcFrame
{
    /*
    @brief 跨平台socket工具函数，封装所有常用socket基础操作
    所有方法都是静态，不需要创建实例，直接调用即可
    */
    class SocketUtils
    {
    public:
        // -----Windows专属：初始化Winsock，程序启动调用一次即可-----
        static bool InitWinsock();

        // -----字节序转换-----
        static uint16_t HostToNetShort(uint16_t host16);
        static uint16_t NetToHostShort(uint16_t net16);
        static uint32_t HostToNetLong(uint32_t host32);
        static uint32_t NetToHostLong(uint32_t net32);

        // -----IP地址转换-----
        // 将点分十进制IPv4字符串转为二进制，输出到out_bin
        static bool IpV4StrToBin(const std::string& ip_str, void* out_bin);
        // 将二进制IPv4转为点分十进制字符串
        static std::string IpV4BinToStr(const void* ip_bin);

        // ----通用socket选项设置-----
        static bool SetReuseAddr(SocketType fd, bool enable = true);  // 地址复用，重启服务快速绑端口
        static bool SetReusePort(SocketType fd, bool enable = true);  // 端口复用，多进程绑同端口
        static bool SetNonBlocking(SocketType fd, bool enable = true); // 设置非阻塞，适配Reactor

        // ----创建socket-----
        static SocketType CreateTcpSocket(); // 创建TCP socket，失败返回INVALID_SOCKET_VALUE

        // ----错误处理-----
        static int GetLastError(); // 获取当前错误码
        static std::string GetLastErrorStr(int err); // 错误码转字符串

        // ----工具函数-----
        static bool IsPortAvailable(uint16_t port); // 检测端口是否可用

        // ----关闭socket-----
        static void CloseSocket(SocketType fd); // 跨平台关闭socket

    private:
        // 工具类禁用实例化
        SocketUtils() = default;
        ~SocketUtils() = default;
    };
}
