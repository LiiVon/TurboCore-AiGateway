#include "config.h"
#include "logger.h"
#include <yaml-cpp/yaml.h>
#include "global.h"

namespace TcFrame
{
	Config& Config::Instance()
	{
		static Config instance;
		return instance;
	}

	bool Config::LoadFromFile(const std::string& file_path)
	{
		std::lock_guard<std::recursive_mutex> lock(m_mtx);
		m_config_file_path = file_path;
		try
		{
			YAML::Node root = YAML::LoadFile(file_path);
			if (!root)
			{
				LOG_ERROR("Failed to load config file: " + file_path);
				return false;
			}
			ParseYamlNode("", root);
			LOG_INFO("Config loaded success: " + file_path);
			return true;
		}
		catch (const YAML::Exception& e)
		{
			LOG_ERROR("YAML parsing error in file " + file_path + ": " + std::string(e.what()));
			return false;
		}
	}

	bool Config::Reload()
	{
		LOG_INFO("Reloading config file: " + m_config_file_path);
		std::lock_guard<std::recursive_mutex> lock(m_mtx);

		// 先保存旧配置，加载失败就回滚，保证服务可用
		auto old_configs = m_configs;
		m_configs.clear();

		bool ret = LoadFromFile(m_config_file_path);
		if (!ret)
		{
			// 加载失败，恢复旧配置，不影响现有运行
			m_configs = old_configs;
			LOG_ERROR("Reload config failed, rollback to old config");
			return false;
		}

		// 加载成功，遍历所有注册了回调的key，对比变更，变更了才通知
		for (auto& [key, callbacks] : m_callbacks)
		{
			if (IsConfigChanged(key, old_configs))
			{
				for (auto& cb : callbacks)
				{
					cb(key);
				}
			}
		}

		LOG_INFO("Reload config success: " + m_config_file_path);
		return true;
	}

	void Config::OnChange(const std::string& key, const ConfigChangeCallback& callback)
	{
		std::lock_guard<std::recursive_mutex> lock(m_mtx);
		m_callbacks[key].push_back(callback);
	}

	bool Config::IsConfigChanged(const std::string& key, const std::unordered_map<std::string, std::any>& old_configs) const
	{
		// key存在性变化：原来有现在没有，或者原来没有现在有，都是变更
		bool old_has = old_configs.count(key) > 0;
		bool new_has = m_configs.count(key) > 0;
		if (old_has != new_has)
		{
			return true;
		}
		if (!old_has && !new_has)
		{
			return false;
		}

		// 空值判断
		const std::any& old_val = old_configs.at(key);
		const std::any& new_val = m_configs.at(key);
		if (old_val.has_value() != new_val.has_value())
		{
			return true;
		}

		// 类型判断
		if (old_val.type() != new_val.type())
		{
			return true;
		}

		// 同类型对比值
		if (old_val.type() == typeid(int))
		{
			int old_v = std::any_cast<int>(old_val);
			int new_v = std::any_cast<int>(new_val);
			return old_v != new_v;
		}
		else if (old_val.type() == typeid(double))
		{
			double old_v = std::any_cast<double>(old_val);
			double new_v = std::any_cast<double>(new_val);
			// 浮点数比较，处理精度问题，这里用简单的差小于1e-9判断
			return std::abs(old_v - new_v) > 1e-9;
		}
		else if (old_val.type() == typeid(bool))
		{
			bool old_v = std::any_cast<bool>(old_val);
			bool new_v = std::any_cast<bool>(new_val);
			return old_v != new_v;
		}
		else if (old_val.type() == typeid(std::string))
		{
			std::string old_v = std::any_cast<std::string>(old_val);
			std::string new_v = std::any_cast<std::string>(new_val);
			return old_v != new_v;
		}

		// 未知类型，默认认为变更
		return true;
	}

	void Config::ParseYamlNode(const std::string& prefix, const YAML::Node& node)
	{
		if (node.IsScalar())  // 单个值：处理int/double/bool/string
		{
			std::string key = prefix.empty() ? "root" : prefix;
			std::string scalar_str = node.as<std::string>();

			// 先试int类型
			try
			{
				int int_val = std::stoi(scalar_str);
				m_configs[key] = int_val;
				return;
			}
			catch (...) {}

			// 再试double类型（修复之前不支持浮点数问题）
			try
			{
				double double_val = std::stod(scalar_str);
				m_configs[key] = double_val;
				return;
			}
			catch (...) {}

			// 再试bool类型
			if (scalar_str == "true" || scalar_str == "True" || scalar_str == "1") {
				m_configs[key] = true;
				return;
			}
			if (scalar_str == "false" || scalar_str == "False" || scalar_str == "0") {
				m_configs[key] = false;
				return;
			}

			// 都不对，存成字符串
			m_configs[key] = scalar_str;
			return;
		}
		else if (node.IsMap())  // 字典：递归展开每个子节点
		{
			for (auto it = node.begin(); it != node.end(); ++it)
			{
				std::string child_key = prefix.empty()
					? it->first.as<std::string>()
					: prefix + "." + it->first.as<std::string>();
				ParseYamlNode(child_key, it->second);
			}
		}
		else if (node.IsSequence())  // 数组：展开成key.0、key.1...格式（修复之前不支持数组问题）
		{
			for (size_t i = 0; i < node.size(); ++i)
			{
				std::string child_key = prefix + "." + std::to_string(i);
				ParseYamlNode(child_key, node[i]);
			}
		}
		else
		{
			// 未知节点类型，打日志提醒
			LOG_WARN("Unsupported YAML node type for key: " + prefix + ", ignored");
		}
	}
}
