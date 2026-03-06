#pragma once
#include "global.h"
#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "socket_utils.h"
#undef ERROR
typedef int socklen_t;
#endif
namespace TcFrame
{
	class Address
	{
	public:
		Address();
		~Address() = default;
		Address(uint16_t port, const std::string& ip = "0.0.0.0");

		/*
		@brief：系统调用需要的是struct sockaddr*，所以转换一下
		*/
		struct sockaddr* GetSockAddr();

		socklen_t GetSockLen() const;
		std::string GetIp() const;
		uint16_t GetPort() const;
		std::string ToString() const;

	private:
		struct sockaddr_in  m_addr;  // IPv4专属地址结构
	};
}
