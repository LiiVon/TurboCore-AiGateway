#include "registry_manager.h"

namespace AiSchedule
{
	namespace Registry
	{
		std::string RegistryManager::RegisterWorker(const RegistryNodeInfo& info)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			// 用地址+时间戳生成唯一id
			std::string node_id = info.node_addr + "_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

			m_nodes[node_id] = info;
			for (const auto& model : info.support_models)
			{
				m_model_to_nodes[model].push_back(node_id);
			}
			return node_id;
		}

		void RegistryManager::UpdateHeartbeat(const std::string& node_id, int32_t available_memory_mb, const std::vector<std::string>& online_models)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			auto it = m_nodes.find(node_id);
			if (m_nodes.count(node_id)) {
				m_nodes[node_id].last_heartbeat = std::chrono::steady_clock::now();
				m_nodes[node_id].available_memory_mb = available_memory_mb;
				m_nodes[node_id].support_models = online_models;
			}
		}

		void RegistryManager::RemoveWorker(const std::string& node_id)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (!m_nodes.count(node_id))
			{
				return;
			}

			for (const auto& model : m_nodes[node_id].support_models)
			{
				auto& vec = m_model_to_nodes[model];
				vec.erase(std::remove(vec.begin(), vec.end(), node_id), vec.end());
			}

			m_nodes.erase(node_id);
		}

		std::vector<RegistryNodeInfo> RegistryManager::GetOnlineNodesByModel(const std::string& model_name)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			std::vector<RegistryNodeInfo> result;
			if (!m_model_to_nodes.count(model_name))
			{
				return result;
			}

			for (const auto& node_id : m_model_to_nodes[model_name])
			{
				if (m_nodes.count(node_id))
				{
					// 过滤掉超时节点，不用等定时扫，这里直接过滤，保证返回都是活的
					auto duration = std::chrono::steady_clock::now() - m_nodes[node_id].last_heartbeat;

					if (std::chrono::duration_cast<std::chrono::seconds>(duration).count() < 30) {
						result.push_back(m_nodes[node_id]);
					}
				}
			}

			std::sort(result.begin(), result.end(), [](const RegistryNodeInfo& a, const RegistryNodeInfo& b) {
				return a.available_memory_mb > b.available_memory_mb; // 可用内存降序
				});

			return result;
		}

		void RegistryManager::ScanTimeoutNodes()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			std::vector<std::string> to_remove;
			for (const auto& [node_id, info] : m_nodes)
			{
				auto duration = std::chrono::steady_clock::now() - info.last_heartbeat;
				if (std::chrono::duration_cast<std::chrono::seconds>(duration).count() >= 30)
				{
					to_remove.push_back(node_id);
				}
			}

			for (const auto& node_id : to_remove)
			{
				RemoveWorker(node_id);
			}
		}
	}
}