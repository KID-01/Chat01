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
    std::map<int, ClientInfo> m_connectedClients;  // socket -> (用户名, IP地址, 用户ID, 登录状态)（可以根据socket查看是哪位用户，不用ID搜索）
    
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
    void addUser(int socket, const std::string& username, const std::string& ipAddress);// 添加用户（包含IP地址）
    void removeUser(int socket);// 踢出用户
    std::string getUsernameBySocket(int socket) const;// 用socket找到用户
    int getSocketByUsername(const std::string& username) const;// 获取用户的socket
    std::string getClientInfoBySocket(int socket) const;// 获取客户端完整信息
    
    // 消息处理（业务逻辑），这三个函数ServerNetworkManager也有，这边的是业务逻辑封装，对网络方法的封装和增强
    void broadcastMessage(const std::string& sender, const std::string& content);
    void broadcastMessageExclude(int excludeSocket, const std::string& sender, const std::string& content);// 组播，前面是被排除的客户端
    void sendToClient(int clientSocket, const std::string& message);
   
    // 获取服务器信息
    std::string getServerInfo() const;
    
    // 获取服务器信息（快速，避免阻塞）
    std::string getServerInfoFast() const;

    // 回调函数（供网络管理器调用）
    void onClientConnected(int clientSocket, const std::string& clientAddress);
    void onClientDisconnected(int clientSocket);
    void onMessageReceived(int clientSocket, const std::string& message);
    
private:
    // 内部工具方法
    void logMessage(const std::string& message);
    
    // 业务逻辑处理（在回调函数中实现）
    void handleLoginMessage(int clientSocket, const std::string& username);
    
    // ID管理
    std::string allocateUserID();
    bool canCreateNewAccount();
};

#endif