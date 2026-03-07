#include "jobinfo.h"
#include "jobmanager.h"
#include "logger.h"

namespace AiSchedule
{
	namespace DistTask
	{
		using namespace TcFrame;

		void JobManager::AddOrUpdateJob(const JobInfo& job_info)
		{
			std::lock_guard<std::mutex> lock(m_mtx);
			bool is_update = m_jobs.count(job_info.job_id) > 0;

			m_jobs[job_info.job_id] = job_info;
			if (is_update)
			{
				LOG_INFO("Job updated: job_id=[%s], name=[%s]",
					job_info.job_id.c_str(), job_info.job_name.c_str());
			}
			else
			{
				LOG_INFO("Job added: job_id=[%s], name=[%s]",
					job_info.job_id.c_str(), job_info.job_name.c_str());
			}
		}

		void JobManager::RemoveJob(const std::string& job_id)
		{
			std::lock_guard<std::mutex> lock(m_mtx);
			if (m_jobs.erase(job_id) > 0)
			{
				LOG_INFO("Job removed: job_id=[%s]", job_id.c_str());
			}

			m_jobs.erase(job_id);
			if (m_job_results.count(job_id) > 0)
			{
				m_job_results.erase(job_id);
			}
			LOG_INFO("Job removed: job_id=[%s]", job_id.c_str());
		}

		JobInfo JobManager::GetJob(const std::string& job_id)
		{
			std::lock_guard<std::mutex> lock(m_mtx);
			if (m_jobs.count(job_id) > 0)
			{
				LOG_DEBUG("Job not found: job_id=[%s]", job_id.c_str());
				return m_jobs[job_id];
			}
			return m_jobs.at(job_id);
		}

		std::vector<JobInfo> JobManager::GetAllEnableJobs()
		{
			std::lock_guard<std::mutex> lock(m_mtx);
			std::vector<JobInfo> result;
			result.reserve(m_jobs.size());

			for (auto& pair : m_jobs)
			{
				if (pair.second.status == JobStatus::Enabled)
				{
					result.push_back(pair.second);
				}
			}
			return result;
		}

		void JobManager::SaveJobResult(const JobResult& job_result)
		{
			std::lock_guard<std::mutex> lock(m_mtx);
			m_job_results[job_result.job_id].push_back(job_result);

			LOG_INFO("Job result added: job_id=[%s], node_id=[%s], success=[%d]",
				job_result.job_id.c_str(), job_result.node_id.c_str(), job_result.success);
		}

		std::vector<JobResult> JobManager::GetJobResults(const std::string& job_id, int max_count)
		{
			std::lock_guard<std::mutex> lock(m_mtx);

			// 边界1：找不到这个job的任何结果，直接返回空
			if (m_job_results.find(job_id) == m_job_results.end())
			{
				LOG_DEBUG("No job results found: job_id=[%s]", job_id.c_str());
				return {};
			}

			auto& all_results = m_job_results.at(job_id);

			// 边界2：结果总数 <= max_count，直接返回全部，不用裁剪
			if (all_results.size() <= (size_t)max_count || max_count <= 0)
			{
				// max_count <= 0 代表用户要全部结果，直接返回全部
				return all_results;
			}

			// 核心逻辑：只返回最后max_count条 → 因为我们一直往vector追加，最后面的就是最新的结果，符合用户预期
			return std::vector<JobResult>(
				all_results.end() - max_count,  // 从倒数第max_count个开始
				all_results.end()               // 到最后一个结束
			);
		}
	}
}