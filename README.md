# TurboCore-AiGateway
高性能C++服务器核心框架 + AI推理网关，从零实现的Reactor异步网络+分布式AI推理调度项目

## 项目演示：

## 项目背景
此项目为本人 学习C++服务器开发过程中的项目
基础模块完成后，觉得单调，于是添加了分布式AI推理模块

环境依赖
·
C++17 及以上
·
CMake 3.10 及以上 (或Visual Studio 2022+)
·
yaml    (config 需要使用)
·
Protobuf 3.15 及以上（强制去除Arean限制)
·
Windows下推荐使用vcpkg管理依赖

## 代码整体框架

TurboCore 是基于One Loop Per Thread Reactor 模式设计的C++高性能异步服务器框架，上层扩展了AI分布式推理网关能力。框架从下到上分为「基础工具层、核心网络层、业务网关层」三个层次，每个类职责单一，封装清晰，方便扩展。

---
### 一、基础工具类层

| 类名 | 功能特点 & 封装 |
| :--- | :--- |
| **Log** | 封装了多线程安全的分级日志系统，支持`DEBUG/INFO/WARN/ERROR`四个日志级别，可配置输出到控制台/本地文件，自动处理日志滚动，对外提供`LOG_DEBUG`/`LOG_INFO`等易用宏，隐藏底层线程同步和IO细节，业务代码一行就能打日志。 |
| **Config** | 封装配置文件读取功能，支持JSON/YAML格式配置，统一管理框架的端口、线程数、超时时间等参数，避免硬编码，对外提供`GetInt`/`GetString`/`GetBool`等接口，不用业务关心配置解析细节。 |
| **Threadpool** | 封装通用线程池，支持任务队列、线程复用、动态扩容缩容，兼容普通函数和lambda任务，给非网络IO的耗时任务（比如配置加载、日志持久化）提供线程池支持，避免阻塞主Reactor线程。 |

---
### 二、核心网络类层

| 类名 | 功能特点 & 封装 |
| :--- | :--- |
| **SocketUtils** | 封装了Linux/Windows平台下原生Socket API，统一了不同平台的差异（比如Linux的`epoll`和Windows的`IOCP`适配），对外提供`CreateTcpSocket`/`Bind`/`Listen`/`SetNonBlocking`等常用操作，上层不用再处理不同平台的原生API差异。 |
| **Address** | 封装IP+端口信息，内部存储原生地址结构，对外提供`GetIp`/`GetPort`/`ToString`接口，把地址格式化、信息提取的细节都封装起来，所有网络模块统一用Address传递地址，不用上层手动处理原生地址。 |
| **Buffer** | 针对TCP字节流特性封装的缓冲区，支持自动扩容、读写分离，提供`Append`/`Retrieve`/`ReadBytes`接口，天然支持拆包粘包处理：上层可以先读取长度字段，再根据长度读取对应内容，完美解决TCP流的半包问题。 |
| **Socket** | 持有文件描述符，绑定通信地址`Address`，封装`Connect`/`Accept`/`Send`/`Recv`这些基础通信操作，把文件描述符的生命周期管理封装在内，自动关闭失效描述符，避免资源泄漏。 |
| **TcpConnection** | 封装一条完整的TCP连接，持有`Socket`和`Buffer`（读缓冲区+写缓冲区），绑定连接回调、消息回调，处理连接建立、消息到达、连接关闭三个核心事件，把应用层和底层IO细节隔离开。 |
| **EventLoop** | 整个框架的核心Reactor，每个线程最多一个EventLoop，封装了多路复用（`epoll`/`IOCP`），对外提供`loop()`启动循环、`RunInLoop`跨线程投送任务、`RunAfter`延迟执行任务三个核心接口，保证所有IO操作都在Loop线程执行，避免不必要的锁竞争。 |
| **Acceptor** | 专门负责监听端口、接受新连接，封装了监听Socket的创建、绑定、监听流程，新连接建立后自动把连接分配给对应的EventLoop，把"接受新连接"这个逻辑从TcpServer里独立出来，职责更清晰。 |
| **TcpServer** | 整个网络层的入口类，持有主EventLoop、Acceptor、所有TcpConnection，管理连接生命周期，支持配置线程数（子Loop数量），对外暴露连接建立/消息到达的回调接口，业务层只需要设置回调，不用处理底层IO细节，直接就能启动一个TCP服务。 |
| **TcpClient** | 封装TCP客户端功能，对应TcpServer，提供连接服务器、发送消息、断开连接接口，支持断线重连，给需要主动连接服务的场景使用。 |

---
### 三、AI分布式推理网关层

| 类名 | 功能特点 & 封装 |
| :--- | :--- |
| **DistConnection** | 封装Master和Worker之间的分布式连接，基于`TcpConnection`扩展，绑定Protobuf消息回调，自动处理消息编解码，把协议处理和连接分开，上层不用关心拆包解码细节。 |
| **ProtocolCodec** | 封装Protobuf消息的编解码和粘包处理，对外提供`BuildXXXRequest`/`BuildXXXResponse`方法，把Protobuf消息打包成带长度头的二进制帧，解码时自动按长度拆包，保证完整消息才会回调上层。 |
| **StoredModelInfo** | 纯C++自定义结构体，存储Worker加载的模型信息，绕开Protobuf强制Arena拷贝的问题，只存我们需要的模型名、版本、大小，比直接存Protobuf对象更轻量。 |
| **WorkerInfo** | Master存储的Worker节点信息，持有`DistConnection`、节点基础信息、已加载模型列表、负载信息，自带`HasModel`方法匹配模型，支持按模型名+版本过滤，给负载均衡提供匹配能力。 |
| **DistServer** | AI推理集群的Master调度节点，负责Worker注册管理、心跳保活、超时清理，核心提供`SelectWorkerForModel`接口，基于最小 pending 任务数做负载均衡，选最优Worker处理推理请求，所有操作加互斥锁，支持多线程并发查询，适合网关多线程接入场景。 |
| **InferenceGateway** | 整个AI推理网关的对外入口，封装HTTP/gRPC接口，接收业务方推理请求，调用`DistServer`选Worker，转发请求给对应Worker，拿到结果返回给业务方，统一处理超时、重试，业务方不用感知底层集群细节。 |

---
## 二、整体运行流程

我们从新连接进来，到推理请求处理完成，串一遍整个框架的流程：
1.  **启动阶段**：`TcpServer`创建主`EventLoop`，`Acceptor`绑定监听`Address`，调用`SocketUtils`创建监听Socket，开始在主EventLoop里监听接受连接事件；`DistServer`启动后，每10秒自动执行一次`CleanTimeoutWorkers`，清理心跳超时的死Worker节点。
2.  **Worker注册阶段**：Worker启动后作为`TcpClient`连接`DistServer`，发送注册请求；`Acceptor`接受新连接，创建`DistConnection`绑定到子EventLoop，`ProtocolCodec`解码注册请求，`DistServer`校验协议版本，提取模型信息存入`WorkerInfo`，返回注册成功，完成Worker上线。
3.  **推理请求阶段**：业务方给`InferenceGateway`发推理请求，`InferenceGateway`调用`DistServer::SelectWorkerForModel`，根据模型名+版本，遍历所有存活Worker，选pending任务最少的节点，然后把推理请求转发给对应Worker。
4.  **IO处理阶段**：Worker返回推理结果，`TcpConnection`读到数据存入`Buffer`，`ProtocolCodec`从`Buffer`里拆出完整Protobuf消息，回调`DistServer`的`OnInferenceResponse`，`DistServer`再透传给`InferenceGateway`，`InferenceGateway`把结果返回给业务方。
5.  **心跳保活**：Worker定期给`DistServer`发心跳，带上当前pending任务数和空闲内存，`DistServer`更新`WorkerInfo`的负载信息和心跳时间；超过心跳超时的Worker会被自动清理，下线通知上层回调。
