// ClientNetworkManager.h - 客户端网络管理器

#ifndef CLIENTNETWORKMANAGER_H
#define CLIENTNETWORKMANAGER_H

#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

// 引入跨平台网络层
#include "../../common/include/PlatformNetwork.h"
// 引入网络协议
#include "../../common/include/NetworkProtocol.h"

class ClientNetworkManager {
public:
    // 构造函数和析构函数
    ClientNetworkManager();
    ~ClientNetworkManager();
    
    // 连接管理

    // 连接到服务器
    bool connectToServer(const std::string& address, int port, const std::string& username);
    // 断开连接
    void disconnect();
    // 确认连接状态
    bool isConnected() const;
    
    // 发送消息
    bool sendMessage(const std::string& message);
    
    // 发送登录消息
    bool sendLoginMessage();
    
    // 获取最后一次发送错误信息
    std::string getLastSendError();
    
    // 请求用户列表
    bool requestUserList();
    
    // 回调函数设置
    void setOnConnected(std::function<void()> callback);
    void setOnDisconnected(std::function<void()> callback);
    void setOnMessageReceived(std::function<void(const std::string&, const std::string&)> callback);
    void setOnError(std::function<void(const std::string&)> callback);

private:
    // 网络状态（使用 SocketType：Linux 为 int，Windows 为 SOCKET/UINT_PTR，避免 64 位截断导致 select/FD_SET 失效）
    SocketType m_socket;
    std::atomic<bool> m_connected;
    std::string m_serverAddress;
    int m_port;
    std::string m_username;
    
    // 网络线程
    std::thread m_receiveThread;
    std::atomic<bool> m_running;
    
    // 回调函数
    std::function<void()> m_onConnected;
    std::function<void()> m_onDisconnected;
    std::function<void(const std::string&, const std::string&)> m_onMessageReceived;
    std::function<void(const std::string&)> m_onError;
    
    // 最后一次发送错误信息
    std::string m_lastSendError;
    std::mutex m_errorMutex;
    
    // 私有方法
    void receiveLoop();  // 接收消息的循环
    void log(const std::string& message);  // 日志记录
    bool validateMessageFormat(const Chat01::NetworkMessage& message);  // 验证消息格式
};

#endif