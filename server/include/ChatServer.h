// ChatServer.h - 独立服务器程序头文件

#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

#include "../../common/include/PlatformNetwork.h"

class ServerNetworkManager;
namespace Chat01 { class AccountManager; }


class ChatServer {
private:
    // 服务器配置
    std::string m_serverName;
    int m_port;
    bool m_running;
    
    // 网络管理器
    std::unique_ptr<ServerNetworkManager> m_networkManager;
    
    // 账号管理器
    std::unique_ptr<Chat01::AccountManager> m_accountManager;
    
    // 用户管理
    struct ClientInfo {
        std::string username;
        std::string ipAddress;
        std::string userID;  // 服务器分配的ID
        bool isLoggedIn;     // 是否已登录
    };
    std::map<SocketType, ClientInfo> m_connectedClients;  // socket -> 用户信息（使用 SocketType 避免 64 位截断）
    
    // ID管理
    static const int MAX_USERS = 100;  // 最大在线用户数
    int m_totalAccounts;               // 总账号数量（决定下一个ID）
    std::atomic<int> m_onlineUsers;    // 当前在线用户数（原子操作，避免锁竞争）
    
    // 消息历史（内存缓存，后续会移到数据库）
    std::vector<std::string> m_messageHistory;
    
    // 线程安全（mutable允许在const函数中修改）
    mutable std::mutex m_clientsMutex;
    mutable std::mutex m_messagesMutex;

public:
    // 构造函数和析构函数
    ChatServer(const std::string& serverName = "聊天服务器", int port = 8080);
    ~ChatServer();
    
    // 服务器控制
    bool start();
    void stop();
    bool isRunning() const;

    // 用户管理
    int getConnectedCount() const;// 获取连接数
    
    // 获取在线用户数（无锁，快速）
    int getOnlineUsersCount() const { return m_onlineUsers.load(); }
    
    // 获取连接用户列表（快速，避免阻塞）
    std::vector<std::string> getConnectedUsersFast() const;
    std::vector<std::string> getConnectedUsers() const;// 所有连接的用户
    void addUser(SocketType socket, const std::string& username, const std::string& ipAddress);
    void removeUser(SocketType socket);
    std::string getUsernameBySocket(SocketType socket) const;
    SocketType getSocketByUsername(const std::string& username) const;  // 未找到返回 INVALID_SOCKET_VALUE
    std::string getClientInfoBySocket(SocketType socket) const;
    
    void broadcastMessage(const std::string& sender, const std::string& content);
    void broadcastMessageExclude(SocketType excludeSocket, const std::string& sender, const std::string& content);
    void sendToClient(SocketType clientSocket, const std::string& message);
   
    // 获取服务器信息
    std::string getServerInfo() const;
    
    // 获取服务器信息（快速，避免阻塞）
    std::string getServerInfoFast() const;

    void onClientConnected(SocketType clientSocket, const std::string& clientAddress);
    void onClientDisconnected(SocketType clientSocket);
    void onMessageReceived(SocketType clientSocket, const std::string& message);
    
private:
    void logMessage(const std::string& message);
    void handleLoginMessage(SocketType clientSocket, const std::string& username);
    
    // ID管理
    std::string allocateUserID();
    bool canCreateNewAccount();
};

#endif