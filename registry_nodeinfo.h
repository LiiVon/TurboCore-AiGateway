#pragma once
#include "global.h"
// 存储单个Worker节点信息

namespace AiSchedule
{
	namespace Registry
	{
		struct RegistryNodeInfo
		{
			std::string node_id;
			std::string node_addr;
			std::vector<std::string> support_models;
			int32_t total_memory_mb;
			int32_t available_memory_mb;
			int32_t weight;
			std::chrono::steady_clock::time_point last_heartbeat; // 最后心跳时间，超时判断用
		};
	}
}
