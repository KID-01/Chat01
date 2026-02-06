// ServerNetworkManager.h - 服务器专用网络管理器

#ifndef SERVERNETWORKMANAGER_H
#define SERVERNETWORKMANAGER_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>


class ServerNetworkManager {
public:
    // 构造函数和析构函数
    ServerNetworkManager();
    ~ServerNetworkManager();
    
    // 服务器控制
    bool start(int port, const std::string& address = "0.0.0.0");
    void stop();
    bool isRunning() const;
    
    // 消息发送
    // 这三个函数ChatServer也有，这边的是网络基础，负责消息的原始发送
    bool sendToClient(int clientSocket, const std::string& message);
    void broadcastMessage(const std::string& message);// 广播发送
    void broadcastMessageExclude(int excludeSocket, const std::string& message);// 组播，前面是被排除的客户端
    
    // 连接管理
    void disconnectClient(int clientSocket);// 断开连接
    int getClientCount() const;// 获取连接客户端数量
    std::vector<int> getAllClientSockets() const;// 获取所有客户端socket
    
    // 回调函数设置
    // 注意：回调函数参数现在包含 clientSocket，这样上层知道是哪个客户端的事件
    void setOnClientConnected(std::function<void(int, const std::string&)> callback);// 客户端连接通知
    void setOnClientDisconnected(std::function<void(int)> callback);// 客户端断开连接通知
    void setOnMessageReceived(std::function<void(int, const std::string&)> callback);// 消息接收通知
    
private:
    // 网络状态
    int m_serverSocket;
    std::atomic<bool> m_running;
    
    // 客户端管理（只管理socket连接）
    std::vector<int> m_clientSockets;  // 所有连接的客户端socket
    mutable std::mutex m_clientsMutex;  // 保护客户端列表
    
    // 网络线程
    std::thread m_acceptThread;  // 接受连接线程
    std::map<int, std::thread> m_clientThreads;  // socket -> 线程映射，便于管理
    mutable std::mutex m_threadsMutex;  // 保护线程映射
    
    // 回调函数
    std::function<void(int, const std::string&)> m_onClientConnected;// 用户登记通知
    std::function<void(int)> m_onClientDisconnected;// 用户离开通知
    std::function<void(int, const std::string&)> m_onMessageReceived;// 收到消息通知
    
    // 私有方法
    void acceptConnections();  // 接受连接线程函数
    void handleClient(int clientSocket);  // 处理单个客户端的线程函数
    void cleanupClient(int clientSocket);  // 清理客户端资源
    
    // 线程安全工具
    void addClientSocket(int socket);// 添加用户socket
    void removeClientSocket(int socket);// 删除用户socket
    bool isClientSocketValid(int socket) const;// 根据socket判断客户端是否还是有效连接
    
    // 日志
    void log(const std::string& message);// 记录程序运行期间发生的所有重要事件
};

#endif // SERVERNETWORKMANAGER_H