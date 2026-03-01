// ClientNetworkManager.cpp - 客户端网络管理器实现

#include "../include/ClientNetworkManager.h"
#include "../../common/include/User.h"
#include "../../common/include/Message.h"
#include "../../common/include/NetworkProtocol.h"
#include "../../common/include/PlatformNetwork.h"
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <chrono>

using namespace Chat01;

// 解决 Windows 下 ssize_t 和 close 的兼容性问题
#ifdef _WIN32
typedef intptr_t ssize_t;
#define CLOSE_FUNC closesocket
#else
#define CLOSE_FUNC close
#endif

// 构造函数
ClientNetworkManager::ClientNetworkManager()
    : m_socket(INVALID_SOCKET_VALUE)
    , m_connected(false)
    , m_port(8080)
    , m_running(false)
{
    PlatformNetwork::Initialize();
    log("ClientNetworkManager Created");
}

// 析构函数
ClientNetworkManager::~ClientNetworkManager()
{
    m_onConnected = nullptr;
    m_onDisconnected = nullptr;
    m_onMessageReceived = nullptr;
    m_onError = nullptr;

    if (m_connected)
    {
        m_running = false;
        if (PlatformNetwork::IsValidSocket(m_socket))
        {
            PlatformNetwork::CloseSocket(m_socket);
            m_socket = INVALID_SOCKET_VALUE;
        }

        if (m_receiveThread.joinable())
        {
            m_receiveThread.join();
        }
        m_connected = false;
    }
    PlatformNetwork::Cleanup();
    log("ClientNetworkManager Destroyed");
}

// 连接到服务器
bool ClientNetworkManager::connectToServer(const std::string& address, int port, const std::string& username)
{
    if (m_connected)
    {
        log("Already connected to server");
        return false;
    }

    m_serverAddress = address;
    m_port = port;
    m_username = username;

    m_socket = PlatformNetwork::CreateSocket();
    if (!PlatformNetwork::IsValidSocket(m_socket))
    {
        std::string err = "创建套接字失败: " + PlatformNetwork::GetErrorMessage();
        log(err);
        if (m_onError) m_onError(err);
        return false;
    }

    // 设置为非阻塞以进行带超时的连接
    PlatformNetwork::SetNonBlocking(m_socket, true);

    int connectResult = PlatformNetwork::ConnectToServer(m_socket, address, port);
    if (connectResult < 0)
    {
        int connectError = GET_LAST_ERROR;
#ifdef _WIN32
        // Windows: WSAEWOULDBLOCK(10035)/WSAEINPROGRESS(10036) 表示“正在连接”，需用 select 等待
        const bool inProgress = (connectError == EINPROGRESS || connectError == EWOULDBLOCK
            || connectError == 10035 || connectError == 10036);
#else
        const bool inProgress = (connectError == EINPROGRESS || connectError == EWOULDBLOCK);
#endif
        if (!inProgress)
        {
            std::string err = "连接被拒绝或网络错误: " + PlatformNetwork::GetErrorMessage();
            log(err);
            if (m_onError) m_onError(err);
            PlatformNetwork::CloseSocket(m_socket);
            m_socket = INVALID_SOCKET_VALUE;
            return false;
        }

        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(m_socket, &writefds);

        int selectResult = PlatformNetwork::Select(m_socket, nullptr, &writefds, nullptr, 5);

        if (selectResult <= 0)
        {
            std::string err = "连接超时（5秒内未完成），请检查地址与端口及防火墙";
            log(err);
            if (m_onError) m_onError(err);
            PlatformNetwork::CloseSocket(m_socket);
            m_socket = INVALID_SOCKET_VALUE;
            return false;
        }

        // 【修复 1】跨平台检查 SO_ERROR；Windows 需要 (char*) 强转，且 optlen 为 int*
#ifdef _WIN32
        int error = 0;
        int len = sizeof(error);
        if (getsockopt(m_socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len) < 0)
#else
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(m_socket, SOL_SOCKET, SO_ERROR, (char*)&error, &len) < 0)
#endif
        {
            std::string err = "连接状态检查失败: " + PlatformNetwork::GetErrorMessage();
            log(err);
            if (m_onError) m_onError(err);
            PlatformNetwork::CloseSocket(m_socket);
            m_socket = INVALID_SOCKET_VALUE;
            return false;
        }

        if (error != 0)
        {
            std::string err = "连接失败(错误码 " + std::to_string(error) + ")";
            log(err);
            if (m_onError) m_onError(err);
            PlatformNetwork::CloseSocket(m_socket);
            m_socket = INVALID_SOCKET_VALUE;
            return false;
        }
    }

    m_connected = true;
    m_running = true;

    // 连接建立成功后恢复为阻塞模式
    PlatformNetwork::SetNonBlocking(m_socket, false);

    // 先启动接收线程，再发送登录消息，确保能接收服务器的欢迎消息和登录成功消息
    m_receiveThread = std::thread(&ClientNetworkManager::receiveLoop, this);
    
    // 给接收线程一点时间初始化
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    if (!sendLoginMessage())
    {
        std::string err = "登录包发送失败: " + getLastSendError();
        log(err);
        if (m_onError) m_onError(err);
        disconnect();
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (m_onConnected) m_onConnected();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));


    /*
    // 可选重试（首包已在上方发送）
    bool loginSuccess = false;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        if (attempt > 0 && !sendLoginMessage())
        {
            log("Login retry " + std::to_string(attempt) + " send failed");
            if (m_onError) m_onError("登录重发失败: " + PlatformNetwork::GetErrorMessage());
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(2))
        {
            if (!m_running || !m_connected) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        loginSuccess = true;
        break;
    }
    

    if (!loginSuccess)
    {
        if (m_onError) m_onError("登录超时或连接已断开");
        disconnect();
        return false;
    }
    */

    return true;
}

// 断开连接
void ClientNetworkManager::disconnect()
{
    if (!m_connected) return;

    m_running = false;
    if (PlatformNetwork::IsValidSocket(m_socket))
    {
        // 【修复 2】使用统一的 CloseSocket
        PlatformNetwork::CloseSocket(m_socket);
        m_socket = INVALID_SOCKET_VALUE;
    }

    if (m_receiveThread.joinable()) m_receiveThread.join();

    m_connected = false;
    if (m_onDisconnected) m_onDisconnected();
    log("Disconnected from server");
}

bool ClientNetworkManager::isConnected() const { return m_connected; }

// 发送消息
bool ClientNetworkManager::sendMessage(const std::string& content)
{
    if (!m_connected || !PlatformNetwork::IsValidSocket(m_socket)) return false;

    Chat01::NetworkMessage netMessage(Chat01::MessageType::TEXT_MESSAGE, m_username, content);
    
    // 验证消息格式
    if (!validateMessageFormat(netMessage)) {
        log("Message format validation failed before sending");
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_lastSendError = "消息格式验证失败";
        return false;
    }
    
    std::string serializedMessage = Chat01::MessageSerializer::serialize(netMessage);

    // 使用包装好的 PlatformNetwork::Send
    int sent = PlatformNetwork::Send(m_socket, serializedMessage.c_str(), static_cast<int>(serializedMessage.length()));
    
    if (sent == SOCKET_ERROR_VALUE) {
        // 立即捕获错误信息，避免被其他线程覆盖
        std::string errorMsg = PlatformNetwork::GetErrorMessage();
        log("Failed to send message: " + errorMsg);
        // 将错误信息存储到成员变量
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_lastSendError = errorMsg;
        return false;
    }
    else if (sent == 0)
    {
        // 发送了0字节，表示连接已关闭
        log("Failed to send message: connection closed");
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_lastSendError = "连接已关闭";
        return false;
    }
    
    log("Sent message: " + content);
    return true;
}

// 发送登录消息
bool ClientNetworkManager::sendLoginMessage()
{
    if (!m_connected || !PlatformNetwork::IsValidSocket(m_socket)) return false;

    Chat01::NetworkMessage loginMessage(Chat01::MessageType::LOGIN_REQUEST, m_username, m_username);
    
    // 验证消息格式
    if (!validateMessageFormat(loginMessage)) {
        log("Login message format validation failed before sending");
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_lastSendError = "消息格式验证失败";
        return false;
    }
    
    std::string serialized = Chat01::MessageSerializer::serialize(loginMessage);

    int sent = PlatformNetwork::Send(m_socket, serialized.c_str(), static_cast<int>(serialized.length()));

    log("[DEBUG] sendLoginMessage() sent " + std::to_string(sent) + " bytes, raw: " + serialized);

    if (sent == SOCKET_ERROR_VALUE)
    {
        // 立即捕获错误信息，避免被其他线程覆盖
        std::string errorMsg = PlatformNetwork::GetErrorMessage();
        log("[DEBUG] sendLoginMessage() failed: " + errorMsg);
        // 将错误信息存储到成员变量，供调用方获取
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_lastSendError = errorMsg;
    }
    else if (sent == 0)
    {
        // 发送了0字节，表示连接已关闭
        log("[DEBUG] sendLoginMessage() sent 0 bytes, connection closed");
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_lastSendError = "连接已关闭";
    }

    return sent != SOCKET_ERROR_VALUE && sent > 0;
}

// 获取最后一次发送错误信息
std::string ClientNetworkManager::getLastSendError()
{
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_lastSendError;
}

// 请求用户列表
bool ClientNetworkManager::requestUserList()
{
    if (!m_connected || !PlatformNetwork::IsValidSocket(m_socket)) return false;

    Chat01::NetworkMessage userListRequest(Chat01::MessageType::USER_LIST, m_username, "REQUEST_USER_LIST");
    
    // 验证消息格式
    if (!validateMessageFormat(userListRequest)) {
        log("User list request format validation failed before sending");
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_lastSendError = "消息格式验证失败";
        return false;
    }
    
    std::string serialized = Chat01::MessageSerializer::serialize(userListRequest);

    int sent = PlatformNetwork::Send(m_socket, serialized.c_str(), static_cast<int>(serialized.length()));
    
    if (sent == SOCKET_ERROR_VALUE) {
        // 立即捕获错误信息，避免被其他线程覆盖
        std::string errorMsg = PlatformNetwork::GetErrorMessage();
        log("Failed to send user list request: " + errorMsg);
        // 将错误信息存储到成员变量
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_lastSendError = errorMsg;
        return false;
    }
    else if (sent == 0)
    {
        // 发送了0字节，表示连接已关闭
        log("Failed to send user list request: connection closed");
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_lastSendError = "连接已关闭";
        return false;
    }
    
    return true;
}

// 验证消息格式
bool ClientNetworkManager::validateMessageFormat(const NetworkMessage& message)
{
    // 验证消息类型
    int msgType = static_cast<int>(message.type);
    if (msgType < 1 || msgType > 7) { // 7 is the maximum MessageType value
        log("Invalid message type: " + std::to_string(msgType));
        return false;
    }
    
    // 验证发送者
    if (message.sender.empty()) {
        log("Empty sender in message");
        return false;
    }
    
    // 验证时间戳 - timestamp现在是long long类型，可以正确表示毫秒时间
    if (message.timestamp <= 0) {
        log("Invalid timestamp: " + std::to_string(message.timestamp));
        // 时间戳验证失败不返回false，只记录日志
    }
    
    // 验证消息内容（可选，根据业务需求）
    // 这里可以添加更多的消息内容验证逻辑
    
    return true;
}

// 接收消息循环
void ClientNetworkManager::receiveLoop()
{
    char buffer[4096];
    std::string recvBuf;

    while (m_running && m_connected)
    {
        int received = PlatformNetwork::Receive(m_socket, buffer, sizeof(buffer));

        if (received > 0)
        {
            recvBuf.append(buffer, static_cast<size_t>(received));
            size_t pos = 0;
            while ((pos = recvBuf.find('\n')) != std::string::npos)
            {
                std::string line = recvBuf.substr(0, pos);
                if (!line.empty() && line.back() == '\r') line.pop_back();

                if (!line.empty() && m_onMessageReceived)
                {
                    try {
                        Chat01::NetworkMessage netMessage = Chat01::MessageSerializer::deserialize(line);
                        // 简化分发逻辑
                        if (netMessage.type == Chat01::MessageType::USER_LIST)
                            m_onMessageReceived("USER_LIST", netMessage.content);
                        else
                            m_onMessageReceived(netMessage.sender, netMessage.content);
                    }
                    catch (...) {
                        log("Message deserialization failed");
                    }
                }
                recvBuf.erase(0, pos + 1);
            }
        }
        else if (received == 0)
        {
            // 对于阻塞模式，received == 0 明确代表对端关闭
            m_connected = false;
            if (m_onDisconnected) m_onDisconnected();
            break;
        }
        else
        {
            int error = GET_LAST_ERROR;
            // 如果是阻塞模式且被系统中断，则继续；否则判定为错误
#ifdef _WIN32
            if (error == WSAEINTR) continue;
#else
            if (error == EINTR) continue;
#endif
            log("Receive error: " + std::to_string(error));
            m_connected = false;
            if (m_onDisconnected) m_onDisconnected();
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    log("Receive thread exited");
}

void ClientNetworkManager::log(const std::string& message) {
    std::cout << "[ClientNetworkManager] " << message << std::endl;
}

// 回调设置函数保持不变
void ClientNetworkManager::setOnConnected(std::function<void()> callback) { m_onConnected = callback; }
void ClientNetworkManager::setOnDisconnected(std::function<void()> callback) { m_onDisconnected = callback; }
void ClientNetworkManager::setOnMessageReceived(std::function<void(const std::string&, const std::string&)> callback) { m_onMessageReceived = callback; }
void ClientNetworkManager::setOnError(std::function<void(const std::string&)> callback) { m_onError = callback; }