#pragma once

#include "jobinfo.h"
#include "global.h"

namespace AiSchedule
{
	namespace DistTask
	{
		class JobManager
		{
		public:
			void AddOrUpdateJob(const JobInfo& job_info);
			void RemoveJob(const std::string& job_id);
			JobInfo GetJob(const std::string& job_id);
			std::vector<JobInfo> GetAllEnableJobs();

			// 保存任务执行结果
			void SaveJobResult(const JobResult& job_result);

			// 获取任务执行结果
			std::vector<JobResult> GetJobResults(const std::string& job_id, int max_count = 100);
		private:
			std::mutex m_mtx;
			std::unordered_map<std::string, JobInfo> m_jobs; // job_id -> JobInfo
			std::unordered_map<std::string, std::vector<JobResult>> m_job_results; // job_id -> list of JobResult
		};
	}
}