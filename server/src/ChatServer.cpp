// ChatServer.cpp - 聊天服务器业务逻辑实现

#include "../include/ChatServer.h"
#include "../include/ServerNetworkManager.h"
#include "../include/AccountManager.h"
#include "../../common/include/NetworkProtocol.h"
#include "../../common/include/Message.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <thread>

// 构造函数
ChatServer::ChatServer(const std::string& serverName, int port)
    : m_serverName(serverName)
    , m_port(port)
    , m_running(false)
    , m_totalAccounts(0)      // 从0开始，第一个账号ID为1
    , m_onlineUsers(0)
{
    m_networkManager = std::make_unique<ServerNetworkManager>();
    m_accountManager = std::make_unique<Chat01::AccountManager>("accounts/accounts.json");
    
    logMessage("ChatServer 创建: " + serverName + ", 端口: " + std::to_string(port) + 
               ", 最大在线用户数: " + std::to_string(MAX_USERS));
}

// 析构函数
ChatServer::~ChatServer()
{
    stop();
    logMessage("ChatServer 销毁");
}

// 启动服务器
bool ChatServer::start()
{
    if (m_running)
    {
        logMessage("服务器已经在运行");
        return false;
    }
    
    // 设置回调函数（SocketType 与 ServerNetworkManager 一致）
    m_networkManager->setOnClientConnected(
        [this](SocketType socket, const std::string& address) {
            this->onClientConnected(socket, address);
        }
    );
    
    m_networkManager->setOnClientDisconnected(
        [this](SocketType socket) {
            this->onClientDisconnected(socket);
        }
    );
    
    m_networkManager->setOnMessageReceived(
        [this](SocketType socket, const std::string& message) {
            this->onMessageReceived(socket, message);
        }
    );
    
    // 启动网络管理器
    if (!m_networkManager->start(m_port))
    {
        logMessage("启动网络管理器失败");
        return false;
    }
    
    m_running = true;
    logMessage("聊天服务器启动成功");
    logMessage("服务器信息: " + getServerInfo());
    
    return true;
}

// 停止服务器
void ChatServer::stop()
{
    if (!m_running)
    {
        return;
    }
    
    m_running = false;
    
    if (m_networkManager)
    {
        m_networkManager->stop();
    }
    
    logMessage("聊天服务器已停止");
}

// 检查服务器运行状态
bool ChatServer::isRunning() const
{
    return m_running;
}

// 获取连接客户端数量
int ChatServer::getConnectedCount() const
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    return m_connectedClients.size();
}

// 获取连接用户列表（快速，避免阻塞）
std::vector<std::string> ChatServer::getConnectedUsersFast() const
{
    // 尝试获取锁，如果无法立即获取则等待一小段时间
    std::unique_lock<std::mutex> lock(m_clientsMutex, std::try_to_lock);
    
    if (!lock.owns_lock()) {
        // 无法立即获取锁，等待10ms后重试
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        // 【修复】必须检查 try_lock 的返回值
        if (!lock.try_lock()) {
            return {}; // 依然拿不到锁，安全退出
        }
        
        if (!lock.owns_lock()) {
            // 仍然无法获取锁，返回空列表
            return {};
        }
    }
    
    std::vector<std::string> users;
    
    for (const auto& pair : m_connectedClients)
    {
        users.push_back(pair.second.username);
    }
    
    return users;
}

// 获取所有连接的用户
std::vector<std::string> ChatServer::getConnectedUsers() const
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    std::vector<std::string> users;
    
    for (const auto& pair : m_connectedClients)
    {
        users.push_back(pair.second.username);
    }
    
    return users;
}

// 添加用户
void ChatServer::addUser(SocketType socket, const std::string& username, const std::string& ipAddress)
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    ClientInfo clientInfo;
    clientInfo.username = username;
    clientInfo.ipAddress = ipAddress;
    clientInfo.userID = "";  // 初始为空
    clientInfo.isLoggedIn = false;  // 初始为未登录状态
    
    m_connectedClients[socket] = clientInfo;
    
    logMessage("添加用户: " + username + " (socket=" + std::to_string(socket) + ", IP=" + ipAddress + ")");
}

// 移除用户
void ChatServer::removeUser(SocketType socket)
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    auto it = m_connectedClients.find(socket);
    if (it != m_connectedClients.end())
    {
        std::string username = it->second.username;
        m_connectedClients.erase(it);
        logMessage("移除用户: " + username + " (socket=" + std::to_string(socket) + ")");
    }
}

// 根据socket获取用户名
std::string ChatServer::getUsernameBySocket(SocketType socket) const
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    auto it = m_connectedClients.find(socket);
    if (it != m_connectedClients.end())
    {
        return it->second.username;
    }
    
    return "";
}

// 根据用户名获取socket，未找到返回 INVALID_SOCKET_VALUE
SocketType ChatServer::getSocketByUsername(const std::string& username) const
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    for (const auto& pair : m_connectedClients)
    {
        if (pair.second.username == username)
        {
            return pair.first;
        }
    }
    
    return INVALID_SOCKET_VALUE;
}

// 获取客户端完整信息
std::string ChatServer::getClientInfoBySocket(SocketType socket) const
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    auto it = m_connectedClients.find(socket);
    if (it != m_connectedClients.end())
    {
        return it->second.username + " (" + it->second.ipAddress + ")";
    }
    
    return "未知客户端";
}

// 内部日志方法
void ChatServer::logMessage(const std::string& message)
{
    std::cout << "[ChatServer] " << message << std::endl;
}

// 分配用户ID（永久性，递增不回收）
std::string ChatServer::allocateUserID()
{
    m_totalAccounts++;  // 总账号数增加
    
    // 根据总账号数决定ID位数
    if (m_totalAccounts <= 999)
    {
        // 3位数ID：001-999
        char idStr[4];
        snprintf(idStr, sizeof(idStr), "%03d", m_totalAccounts);
        return std::string(idStr);
    }
    else if (m_totalAccounts <= 9999)
    {
        // 4位数ID：1000-9999
        char idStr[5];
        snprintf(idStr, sizeof(idStr), "%04d", m_totalAccounts);
        return std::string(idStr);
    }
    else
    {
        // 更多位数（理论上不会达到）
        return std::to_string(m_totalAccounts);
    }
}

// 检查是否可以创建新账号（在线用户数限制）
bool ChatServer::canCreateNewAccount()
{
    return m_onlineUsers < MAX_USERS;
}

// 广播消息给所有客户端
void ChatServer::broadcastMessage(const std::string& sender, const std::string& content)
{
    // 使用NetworkProtocol格式化消息
    Chat01::NetworkMessage message(Chat01::MessageType::TEXT_MESSAGE, sender, content);
    
    std::string formattedMsg = Chat01::MessageSerializer::serialize(message);
    
    // 调用网络管理器广播
    if (m_networkManager)
    {
        m_networkManager->broadcastMessage(formattedMsg);
    }
    
    // 记录到消息历史
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        m_messageHistory.push_back(formattedMsg);
    }
    
    logMessage("广播消息[来自 " + sender + "]: " + content);
}

// 组播消息，排除指定客户端
void ChatServer::broadcastMessageExclude(SocketType excludeSocket, const std::string& sender, const std::string& content)
{
    // 使用NetworkProtocol格式化消息
    Chat01::NetworkMessage message(Chat01::MessageType::TEXT_MESSAGE, sender, content);
    
    std::string formattedMsg = Chat01::MessageSerializer::serialize(message);
    
    // 调用网络管理器排除广播
    if (m_networkManager)
    {
        m_networkManager->broadcastMessageExclude(excludeSocket, formattedMsg);
    }
    
    // 记录到消息历史
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        m_messageHistory.push_back(formattedMsg);
    }
    
    logMessage("组播消息[来自 " + sender + ", 排除socket=" + std::to_string(excludeSocket) + "]: " + content);
}

// 发送消息给特定客户端
void ChatServer::sendToClient(SocketType clientSocket, const std::string& message)
{
    // 直接发送原始消息（用于系统消息等）
    if (m_networkManager)
    {
        if (m_networkManager->sendToClient(clientSocket, message))
        {
            logMessage("发送消息到客户端[socket=" + std::to_string(clientSocket) + "]: " + message);
        }
        else
        {
            logMessage("发送消息失败[socket=" + std::to_string(clientSocket) + "]: " + message);
        }
    }
}

// 获取服务器信息
std::string ChatServer::getServerInfoFast() const
{
    // 使用无锁方式获取在线用户数
    int userCount = getOnlineUsersCount();
    return m_serverName + " - 在线用户: " + std::to_string(userCount) + 
           " - 端口: " + std::to_string(m_port);
}

std::string ChatServer::getServerInfo() const
{
    return m_serverName + " - 在线用户: " + std::to_string(getConnectedCount()) + 
           " - 端口: " + std::to_string(m_port);
}

// 客户端连接回调
void ChatServer::onClientConnected(SocketType clientSocket, const std::string& clientAddress)
{
    logMessage("客户端连接: socket=" + std::to_string(clientSocket) + ", IP=" + clientAddress);
    
    // 新用户连接时，暂时标记为"游客"状态
    // 用户需要通过登录消息来创建账号
    std::string tempUsername = "游客" + std::to_string(clientSocket);
    
    // 添加临时用户到管理列表
    addUser(clientSocket, tempUsername, clientAddress);
    
    // 不再发送欢迎消息，等待用户登录后再发送
    
    logMessage("新客户端连接，等待创建账号: " + tempUsername);
}

// 客户端断开连接回调
void ChatServer::onClientDisconnected(SocketType clientSocket)
{
    std::string username = getUsernameBySocket(clientSocket);
    
    if (!username.empty())
    {
        // 检查是否是游客（未登录用户）
        if (username.find("游客") == 0)
        {
            // 游客离开：不广播通知
            logMessage("游客离开: " + username);
        }
        else
        {
            // 已登录用户离开：广播下线通知
            std::string offlineMsg = username + " 下线了";
            Chat01::NetworkMessage offlineMessage(
                Chat01::MessageType::USER_LEAVE,
                "系统",
                offlineMsg
            );
            std::string serializedOffline = Chat01::MessageSerializer::serialize(offlineMessage);
            if (m_networkManager)
            {
                m_networkManager->broadcastMessage(serializedOffline);
            }
            logMessage("用户下线: " + username);
            
            // 减少在线用户数（ID永久保留）
            m_onlineUsers--;
        }
        
        // 从管理列表中移除
        removeUser(clientSocket);
    }
    else
    {
        logMessage("未知客户端断开连接: socket=" + std::to_string(clientSocket));
    }
}

// 消息接收回调
void ChatServer::onMessageReceived(SocketType clientSocket, const std::string& rawMessage)
{
    std::string username = getUsernameBySocket(clientSocket);
    
    if (username.empty())
    {
        logMessage("收到未知客户端的消息: socket=" + std::to_string(clientSocket));
        return;
    }
    
    logMessage("收到消息[来自 " + username + "]: " + rawMessage);
    
    // 使用NetworkProtocol解析消息
    try {
        Chat01::NetworkMessage message = Chat01::MessageSerializer::deserialize(rawMessage);
        
        // 根据消息类型处理
        switch (message.type)
        {
            case Chat01::MessageType::TEXT_MESSAGE:
                // 文本消息：广播给所有用户（排除发送者自己）
                // 只有已登录用户才能发送文本消息
                if (username.find("游客") != 0)
                {
                    // 重新序列化消息，确保格式正确
                    Chat01::NetworkMessage broadcastMessage(
                        Chat01::MessageType::TEXT_MESSAGE,
                        username,
                        message.content
                    );
                    std::string serializedBroadcast = Chat01::MessageSerializer::serialize(broadcastMessage);
                    
                    // 使用网络管理器进行排除广播
                    if (m_networkManager)
                    {
                        m_networkManager->broadcastMessageExclude(clientSocket, serializedBroadcast);
                    }
                }
                else
                {
                    Chat01::NetworkMessage errorMessage(
                        Chat01::MessageType::ERROR_MESSAGE,
                        "系统",
                        "错误：请先登录才能发送消息"
                    );
                    std::string serializedError = Chat01::MessageSerializer::serialize(errorMessage);
                    sendToClient(clientSocket, serializedError);
                }
                break;
                
            case Chat01::MessageType::LOGIN_REQUEST:
                // 登录消息：处理用户登录/注册
                handleLoginMessage(clientSocket, message.content);
                break;
                
            case Chat01::MessageType::USER_LIST:
                // 用户列表请求：发送当前在线用户列表
                {
                    std::vector<std::string> users = getConnectedUsersFast();
                    std::string userListStr;
                    for (size_t i = 0; i < users.size(); ++i) {
                        if (i > 0) userListStr += ",";  // 用逗号分隔用户名
                        userListStr += users[i];
                    }
                    
                    Chat01::NetworkMessage userListMessage(
                        Chat01::MessageType::USER_LIST,
                        "系统",
                        userListStr
                    );
                    std::string serializedList = Chat01::MessageSerializer::serialize(userListMessage);
                    sendToClient(clientSocket, serializedList);
                }
                break;
                
            default:
                logMessage("未知消息类型: " + std::to_string(static_cast<int>(message.type)));
                break;
        }
    }
    catch (const std::exception& e) {
        logMessage("消息解析失败: " + rawMessage + " - " + e.what());
        
        // 如果解析失败，发送错误消息
        Chat01::NetworkMessage errorMessage(
            Chat01::MessageType::ERROR_MESSAGE,
            "系统",
            "错误：消息格式不正确"
        );
        std::string serializedError = Chat01::MessageSerializer::serialize(errorMessage);
        sendToClient(clientSocket, serializedError);
    }
}

// 处理登录消息
void ChatServer::handleLoginMessage(SocketType clientSocket, const std::string& username)
{
    if (username.empty())
    {
        Chat01::NetworkMessage errorMessage(
            Chat01::MessageType::ERROR_MESSAGE,
            "系统",
            "错误：用户名不能为空"
        );
        std::string serializedError = Chat01::MessageSerializer::serialize(errorMessage);
        sendToClient(clientSocket, serializedError);
        return;
    }
    
    std::string oldUsername = getUsernameBySocket(clientSocket);
    
    // 检查是否已经登录
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    auto it = m_connectedClients.find(clientSocket);
    if (it == m_connectedClients.end())
    {
        Chat01::NetworkMessage errorMessage(
            Chat01::MessageType::ERROR_MESSAGE,
            "系统",
            "错误：客户端未连接"
        );
        std::string serializedError = Chat01::MessageSerializer::serialize(errorMessage);
        sendToClient(clientSocket, serializedError);
        return;
    }
    
    if (it->second.isLoggedIn)
    {
        Chat01::NetworkMessage errorMessage(
            Chat01::MessageType::ERROR_MESSAGE,
            "系统",
            "错误：已经登录，无需重复登录"
        );
        std::string serializedError = Chat01::MessageSerializer::serialize(errorMessage);
        sendToClient(clientSocket, serializedError);
        return;
    }
    
    // 检查是否可以创建新账号（在线用户数限制）
    if (!canCreateNewAccount())
    {
        Chat01::NetworkMessage errorMessage(
            Chat01::MessageType::ERROR_MESSAGE,
            "系统",
            "错误：服务器在线用户数已达上限（" + std::to_string(MAX_USERS) + "），无法创建新账号"
        );
        std::string serializedError = Chat01::MessageSerializer::serialize(errorMessage);
        sendToClient(clientSocket, serializedError);
        return;
    }
    
    // 使用AccountManager处理登录，区分新老用户
    bool isNewUser = false;
    std::string userID;
    
    if (m_accountManager)
    {
        userID = m_accountManager->handleUserLogin(username, isNewUser);
    }
    else
    {
        // 如果AccountManager不可用，回退到原来的逻辑
        userID = allocateUserID();
        isNewUser = true;
    }
    
    // 更新用户信息
    it->second.username = username;
    it->second.userID = userID;
    it->second.isLoggedIn = true;
    
    // 增加在线用户数
    m_onlineUsers++;
    
    // 根据新老用户发送不同的消息
    if (isNewUser)
    {
        // 新用户：发送欢迎消息 + 登录成功 + 加入通知
        Chat01::NetworkMessage welcomeMessage(
            Chat01::MessageType::TEXT_MESSAGE,
            "系统",
            "欢迎来到 " + m_serverName + "！"
        );
        std::string serializedWelcome = Chat01::MessageSerializer::serialize(welcomeMessage);
        sendToClient(clientSocket, serializedWelcome);
        
        Chat01::NetworkMessage successMessage(
            Chat01::MessageType::TEXT_MESSAGE,
            "系统",
            "登录成功！"
        );
        std::string serializedSuccess = Chat01::MessageSerializer::serialize(successMessage);
        sendToClient(clientSocket, serializedSuccess);
        
        // 广播新用户加入通知
        Chat01::NetworkMessage onlineMessage(
            Chat01::MessageType::USER_JOIN,
            "系统",
            username + "加入了聊天室！"
        );
        std::string serializedOnline = Chat01::MessageSerializer::serialize(onlineMessage);
        if (m_networkManager)
        {
            m_networkManager->broadcastMessage(serializedOnline);
        }
    }
    else
    {
        // 老用户：只发送登录成功 + 上线通知
        Chat01::NetworkMessage successMessage(
            Chat01::MessageType::TEXT_MESSAGE,
            "系统",
            "登录成功！"
        );
        std::string serializedSuccess = Chat01::MessageSerializer::serialize(successMessage);
        sendToClient(clientSocket, serializedSuccess);
        
        // 广播老用户上线通知
        Chat01::NetworkMessage onlineMessage(
            Chat01::MessageType::USER_JOIN,
            "系统",
            username + "上线了！"
        );
        std::string serializedOnline = Chat01::MessageSerializer::serialize(onlineMessage);
        if (m_networkManager)
        {
            m_networkManager->broadcastMessage(serializedOnline);
        }
    }
    
    logMessage("用户登录: " + username + " (ID: " + userID + ", " + (isNewUser ? "新用户" : "老用户") + ")");
}