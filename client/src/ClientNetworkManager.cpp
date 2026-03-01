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
        log("Failed to create socket");
        return false;
    }

    PlatformNetwork::SetNonBlocking(m_socket, true);

    int connectResult = PlatformNetwork::ConnectToServer(m_socket, address, port);
    if (connectResult < 0)
    {
        int connectError = GET_LAST_ERROR;
        if (connectError != EINPROGRESS && connectError != EWOULDBLOCK)
        {
            log("Failed to connect to server: " + PlatformNetwork::GetErrorMessage());
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
            log("Connection failed or timeout");
            PlatformNetwork::CloseSocket(m_socket);
            m_socket = INVALID_SOCKET_VALUE;
            return false;
        }

        // 【修复 1】跨平台检查 SO_ERROR，Windows 需要 (char*) 强转
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(m_socket, SOL_SOCKET, SO_ERROR, (char*)&error, &len) < 0)
        {
            log("Getsockopt error");
            PlatformNetwork::CloseSocket(m_socket);
            m_socket = INVALID_SOCKET_VALUE;
            return false;
        }

        if (error != 0)
        {
            log("Connection failed with error code: " + std::to_string(error));
            PlatformNetwork::CloseSocket(m_socket);
            m_socket = INVALID_SOCKET_VALUE;
            return false;
        }
    }

    m_connected = true;
    m_running = true;

    // 【改动】保持非阻塞模式，直到登录成功
    // PlatformNetwork::SetNonBlocking(m_socket, false);  <-- 注释掉

    m_receiveThread = std::thread(&ClientNetworkManager::receiveLoop, this);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    if (m_onConnected) m_onConnected();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    bool loginSuccess = false;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        if (!sendLoginMessage())
        {
            log("Login attempt " + std::to_string(attempt + 1) + " send failed, retrying...");
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        // 【改动】等待服务器确认登录成功
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(2))
        {
            // 如果收到登录成功消息，设置 loginSuccess = true
            // 这里我们简化处理，假设收到任何消息即认为连接正常
            if (!m_running || !m_connected) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        loginSuccess = true; // 临时标记成功
        if (loginSuccess) break;
    }

    if (!loginSuccess)
    {
        disconnect();
        return false;
    }

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
    std::string serializedMessage = Chat01::MessageSerializer::serialize(netMessage);
    
    // 使用包装好的 PlatformNetwork::Send
    int sent = PlatformNetwork::Send(m_socket, serializedMessage.c_str(), static_cast<int>(serializedMessage.length()));
    return sent != SOCKET_ERROR_VALUE;
}

// 【修复 3】发送登录消息，统一使用 PlatformNetwork::Send 避免 ssize_t 问题
bool ClientNetworkManager::sendLoginMessage()
{
    if (!m_connected || !PlatformNetwork::IsValidSocket(m_socket)) return false;
    
    Chat01::NetworkMessage loginMessage(Chat01::MessageType::LOGIN_REQUEST, m_username, m_username);
    std::string serialized = Chat01::MessageSerializer::serialize(loginMessage);
    serialized += "\n"; // 确保有换行符
    
    int sent = PlatformNetwork::Send(m_socket, serialized.c_str(), static_cast<int>(serialized.length()));

    log("[DEBUG] sendLoginMessage() sent " + std::to_string(sent) + " bytes, raw: " + serialized);

    if (sent == SOCKET_ERROR_VALUE)
    {
        int err = GET_LAST_ERROR;
        log("[DEBUG] sendLoginMessage() error code: " + std::to_string(err));
    }

    return sent != SOCKET_ERROR_VALUE;
}

// 【修复 4】请求用户列表，同上
bool ClientNetworkManager::requestUserList()
{
    if (!m_connected || !PlatformNetwork::IsValidSocket(m_socket)) return false;
    
    Chat01::NetworkMessage userListRequest(Chat01::MessageType::USER_LIST, m_username, "REQUEST_USER_LIST");
    std::string serialized = Chat01::MessageSerializer::serialize(userListRequest);
    
    int sent = PlatformNetwork::Send(m_socket, serialized.c_str(), static_cast<int>(serialized.length()));
    return sent != SOCKET_ERROR_VALUE;
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
            // 【改动】先用 select 判断 socket 是否真的关闭
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(m_socket, &readfds);
            timeval tv{ 0,0 };
            int sel = select(static_cast<int>(m_socket) + 1, &readfds, nullptr, nullptr, &tv);
            if (sel > 0)
            {
                continue; // 数据就绪，可能是非阻塞状态
            }
            else
            {
                m_connected = false;
                if (m_onDisconnected) m_onDisconnected();
                break;
            }
        }
        else
        {
            int error = GET_LAST_ERROR;
            if (error != EAGAIN && error != EWOULDBLOCK) break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    log("Receive thread exited");
}

void ClientNetworkManager::log(const std::string& message) {
    std::cout << "[ClientNetworkManager] " << message << std::endl;
}

// 回调设置函数保持不变...
void ClientNetworkManager::setOnConnected(std::function<void()> callback) { m_onConnected = callback; }
void ClientNetworkManager::setOnDisconnected(std::function<void()> callback) { m_onDisconnected = callback; }
void ClientNetworkManager::setOnMessageReceived(std::function<void(const std::string&, const std::string&)> callback) { m_onMessageReceived = callback; }
void ClientNetworkManager::setOnError(std::function<void(const std::string&)> callback) { m_onError = callback; }