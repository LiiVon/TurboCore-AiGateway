#pragma once

#include "global.h"
#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#undef ERROR
typedef int socklen_t;

#endif
#include <string>

namespace TcFrame
{
	// IPv4地址包装类，存储IP+Port，提供系统调用需要的sockaddr转换
	class Address
	{
	public:
		// 默认构造：绑定0.0.0.0:0
		Address();
		// 构造：指定端口和IP，IP默认0.0.0.0
		Address(uint16_t port, const std::string& ip = "0.0.0.0");
		~Address() = default;

		// 获取系统调用需要的sockaddr指针，非const版本
		struct sockaddr* GetSockAddr();
		// const版本，支持const Address调用
		const struct sockaddr* GetSockAddr() const;

		// 获取sockaddr长度，给bind/accept等系统调用用
		socklen_t GetSockLen() const;

		// 获取IP、Port、格式化字符串
		std::string GetIp() const;
		uint16_t GetPort() const;
		std::string ToString() const;

	private:
		struct sockaddr_in m_addr;  // IPv4地址结构
	};
}
