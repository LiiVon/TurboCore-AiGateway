#include "registry_server.h"
#include "json.hpp"
#include "protocol.h"
#include "logger.h"

namespace AiSchedule
{
	namespace Registry
	{
		using namespace TcFrame;

		RegistryServer::RegistryServer(EventLoop* loop, const Address& listen_addr)
			: m_loop(loop)
			, m_tcp_server(loop, listen_addr, 4)
		{
			// 绑定连接回调：处理连接建立/断开
			m_tcp_server.SetConnectionCallback(
				[this](const std::shared_ptr<TcpConnection>& conn)
				{
					OnConnection(conn);
				});

			// 绑定消息回调：处理收到的消息
			m_tcp_server.SetMessageCallback(
				[this](const std::shared_ptr<TcpConnection>& conn, Buffer* buf)
				{
					OnMessage(conn, buf);
				});

			LOG_INFO("RegistryServer construct success, listen on %s:%d",
				listen_addr.GetIp().c_str(), listen_addr.GetPort());
		}

		RegistryServer::~RegistryServer()
		{
			LOG_INFO("RegistryServer destructing...");
		}

		void RegistryServer::Start()
		{
			m_tcp_server.Start();
			LOG_INFO("RegistryServer started.");
		}

		RegistryManager* RegistryServer::GetManager()
		{
			return &m_registry_manager;
		}

		void RegistryServer::OnConnection(const TcpConnectionPtr& conn)
		{
			if (conn->IsConnected())
			{
				LOG_INFO("New connection: " + conn->GetName());
				m_parser.Reset();
			}
			else
			{
				LOG_INFO("Connection disconnected: " + conn->GetName());
			}
		}

		void RegistryServer::OnMessage(const std::shared_ptr<TcpConnection>& conn, Buffer* buf)
		{
			m_parser.Feed(buf->Peek(), buf->ReadableBytes());
			buf->Retrieve(buf->ReadableBytes()); // 消息已经被parser消费了，清空缓冲区

			std::vector<AiSchedulePacket> packets;

			size_t parsed = m_parser.ParseAll(packets);
			{
				if (parsed == 0)
				{
					return;
				}
			}
			for (auto& packet : packets)
			{
				HandlePacket(conn, packet);
			}
		}

		void RegistryServer::HandlePacket(const std::shared_ptr<TcpConnection>& conn, AiSchedulePacket& packet)
		{
			switch (packet.base_header.type)
			{
			case PacketType::REGISTER_WORKER:
				HandleRegister(conn, packet);
				break;
			case PacketType::HEARTBEAT:
				HandleHeartbeat(conn, packet);
				break;
			case PacketType::TASK_QUERY:
				HandleQuery(conn, packet);
				break;
			case PacketType::WORKER_REMOVE:
				HandleRemove(conn, packet);
				break;
			default:
				LOG_WARN("RegistryServer receive unhandled packet type: %d, ignore", (int)packet.base_header.type);
				break;
			}
		}

		void RegistryServer::HandleRegister(const std::shared_ptr<TcpConnection>& conn, AiSchedulePacket& packet)
		{
			// 反序列化注册请求
			WorkerRegisterReq req;
			try
			{
				req = WorkerRegisterReq::from_json(packet.body);
			}
			catch (const std::exception& e)
			{
				LOG_ERROR("HandleRegister parse request failed: %s", e.what());
				// 发送失败响应
				WorkerRegisterResp resp;
				resp.success = false;
				resp.message = std::string("parse failed: ") + e.what();
				SendResponse(conn, packet.base_header.seq, PacketType::REGISTER_RESP, resp.to_json());
				return;
			}

			// 处理注册逻辑
			RegistryNodeInfo node_info;
			node_info.node_addr = req.node_addr;
			node_info.support_models = req.support_models;
			node_info.total_memory_mb = req.total_memory_mb;
			node_info.available_memory_mb = req.total_memory_mb; // 刚注册时可用内存等于总内存
			node_info.weight = req.weight;
			node_info.last_heartbeat = std::chrono::steady_clock::now();

			std::string node_id = m_registry_manager.RegisterWorker(node_info);

			// 发送成功响应
			WorkerRegisterResp resp;
			resp.success = true;
			resp.node_id = node_id;
			resp.message = "register success";
			SendResponse(conn, packet.base_header.seq, PacketType::REGISTER_RESP, resp.to_json());
			LOG_INFO("Worker register success, node_id: %s, models: %d", node_id.c_str(), (int)info.support_models.size());
		}

		void RegistryServer::HandleHeartbeat(const std::shared_ptr<TcpConnection>& conn, AiSchedulePacket& packet)
		{
			Heartbeat hb;
			try
			{
				hb = Heartbeat::from_json(packet.body);
			}
			catch (const std::exception& e)
			{
				LOG_ERROR("HandleHeartbeat parse request failed: %s", e.what());
				return;
			}

			m_registry_manager.UpdateHeartbeat(hb.node_id, hb.available_memory_mb, hb.online_models);
			LOG_DEBUG("Receive heartbeat from node: %s, available memory: %d MB", hb.node_id.c_str(), hb.available_memory_mb);
		}

		void RegistryServer::HandleQuery(const std::shared_ptr<TcpConnection>& conn, AiSchedulePacket& packet)
		{
			// 处理调度中心查询：查询支持指定模型的在线Worker列表
			TaskQueryReq req;
			try
			{
				req = TaskQueryReq::from_json(packet.body);
			}
			catch (const std::exception& e)
			{
				LOG_ERROR("HandleQuery parse request failed: %s", e.what());
				return;
			}

			//  从管理器获取过滤排序后的在线节点
			std::vector<RegistryNodeInfo> nodes = m_registry_manager.GetOnlineNodesByModel(req.task_id);

			// 序列化节点列表为json返回
			nlohmann::json resp_body = nlohmann::json::array();
			for (const auto& node : nodes)
			{
				nlohmann::json node_json;
				node_json["node_id"] = node.node_id;
				node_json["node_addr"] = node.node_addr;
				node_json["available_memory_mb"] = node.available_memory_mb;
				node_json["weight"] = node.weight;
				resp_body.push_back(node_json);
			}

			SendResponse(conn, packet.base_header.seq, PacketType::TASK_QUERY, resp_body);
			LOG_INFO("Handle query for model %s, return %d online nodes", req.task_id.c_str(), (int)nodes.size());
		}

		void RegistryServer::HandleRemove(const std::shared_ptr<TcpConnection>& conn, AiSchedulePacket& packet)
		{
			// 处理Worker下线请求
			WorkerRemoveReq req;
			try
			{
				req = WorkerRemoveReq::from_json(packet.body);
			}
			catch (const std::exception& e)
			{
				LOG_ERROR("HandleRemove parse failed: %s", e.what());
				return;
			}

			m_registry_manager.RemoveWorker(req.node_id);
			LOG_INFO("Worker remove success, node_id: %s", req.node_id.c_str());
		}

		void RegistryServer::SendResponse(const std::shared_ptr<TcpConnection>& conn,
			uint64_t req_seq, PacketType resp_type, nlohmann::json body)
		{
			// 构造响应包
			AiSchedulePacket resp_packet;
			resp_packet.base_header.seq = req_seq; // 响应包的序列号和请求包一致，方便匹配
			resp_packet.base_header.type = resp_type;
			resp_packet.base_header.sender_id = "registry_server"; // 可以填服务器标识
			resp_packet.body = body;

			// 序列化为字节流
			std::vector<char> out_bytes;
			resp_packet.serialize(out_bytes);
			conn->Send(out_bytes.data(), out_bytes.size());
		}
	}
}