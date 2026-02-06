// ClientNetworkManager.h - 客户端网络管理器

#ifndef CLIENTNETWORKMANAGER_H
#define CLIENTNETWORKMANAGER_H

#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

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
    
    // 回调函数设置
    void setOnConnected(std::function<void()> callback);
    void setOnDisconnected(std::function<void()> callback);
    void setOnMessageReceived(std::function<void(const std::string&, const std::string&)> callback);
    void setOnError(std::function<void(const std::string&)> callback);

private:
    // 网络状态
    int m_socket;
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
    
    // 私有方法
    void receiveLoop();  // 接收消息的循环
    void log(const std::string& message);  // 日志记录
};

#endif