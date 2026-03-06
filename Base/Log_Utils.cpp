#include "log_utils.h"

namespace TcFrame
{
	std::string Log_utils::LevelToString(LogLevel level)
	{
		switch (level)
		{
		case LogLevel::DEBUG: return "DEBUG";
		case LogLevel::INFO: return "INFO";
		case LogLevel::WARN: return "WARN";
		case LogLevel::ERROR: return "ERROR";
		case LogLevel::FATAL: return "FATAL";
		default: return "UNKNOWN";
		}
	}

	std::string Log_utils::GetCurrentDate()
	{
		auto now = std::chrono::system_clock::now();
		std::time_t time = std::chrono::system_clock::to_time_t(now);
		std::tm local_time;
		localtime_s(&local_time, &time);
		std::ostringstream oss;
		oss << std::put_time(&local_time, "%Y-%m-%d");
		return oss.str();
	}

	std::string Log_utils::GetCurrentTime()
	{
		auto now = std::chrono::system_clock::now();
		std::time_t time = std::chrono::system_clock::to_time_t(now);
		std::tm tm;
		localtime_s(&tm, &time); // Windows安全线程实现
		std::ostringstream oss;
		oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
		return oss.str();
	}
}
