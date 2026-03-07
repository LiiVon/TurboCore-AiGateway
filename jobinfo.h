#pragma once
#include "global.h"

//负责存任务的基本信息，先做内存版，后续再加数据库持久化就行：
namespace AiSchedule
{
	namespace DistTask
	{
		enum class JobStatus
		{
			Disable = 0, // 禁用
			Enabled = 1, // 启用
		};

		struct JobInfo
		{
			std::string job_id; // 任务ID，唯一标识
			std::string job_name; // 任务名称
			std::string cron_expr; // cron表达式，定义任务调度时间
			std::string handler_name; // worker端处理函数名称
			std::string job_params; // 任务参数，JSON字符串格式
			JobStatus status = JobStatus::Enabled; // 任务状态

			int32_t timeout_seconds = 500; // 任务超时时间，单位秒
			int32_t max_retry_times = 3; // 任务最大重试次数
			int64_t create_time;
			int64_t update_time;
		};

		struct JobResult
		{
			std::string result_id;
			std::string job_id;
			std::string node_id; // 执行该任务的worker节点ID
			bool success;
			std::string output; // 任务执行结果输出，JSON字符串格式
			int32_t exit_code; // 任务执行的退出码
			int64_t cost_ms;
			int64_t execute_time;		// 执行时间戳
		};
	}
}