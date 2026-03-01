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

// 引入跨平台网络层
#include "../../common/include/PlatformNetwork.h"


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
    bool sendToClient(SocketType clientSocket, const std::string& message);
    void broadcastMessage(const std::string& message);
    void broadcastMessageExclude(SocketType excludeSocket, const std::string& message);
    
    // 连接管理
    void disconnectClient(SocketType clientSocket);
    int getClientCount() const;
    std::vector<SocketType> getAllClientSockets() const;
    
    // 回调函数设置（使用 SocketType 与跨平台一致）
    void setOnClientConnected(std::function<void(SocketType, const std::string&)> callback);
    void setOnClientDisconnected(std::function<void(SocketType)> callback);
    void setOnMessageReceived(std::function<void(SocketType, const std::string&)> callback);
    
private:
    // 网络状态（使用 SocketType，避免 64 位 Windows 截断）
    SocketType m_serverSocket;
    std::atomic<bool> m_running;
    
    // 客户端管理（只管理socket连接）
    std::vector<SocketType> m_clientSockets;  // 所有连接的客户端socket
    mutable std::mutex m_clientsMutex;  // 保护客户端列表
    
    // 网络线程
    std::thread m_acceptThread;  // 接受连接线程
    std::map<SocketType, std::thread> m_clientThreads;  // socket -> 线程映射
    mutable std::mutex m_threadsMutex;  // 保护线程映射
    
    // 回调函数
    std::function<void(SocketType, const std::string&)> m_onClientConnected;
    std::function<void(SocketType)> m_onClientDisconnected;
    std::function<void(SocketType, const std::string&)> m_onMessageReceived;
    
    // 私有方法
    void acceptConnections();
    void handleClient(SocketType clientSocket);
    void cleanupClient(SocketType clientSocket);
    
    void addClientSocket(SocketType socket);
    void removeClientSocket(SocketType socket);
    bool isClientSocketValid(SocketType socket) const;
    
    // 日志
    void log(const std::string& message);// 记录程序运行期间发生的所有重要事件
};

#endif // SERVERNETWORKMANAGER_H