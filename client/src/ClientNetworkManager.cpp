// ClientNetworkManager.cpp - 客户端网络管理器实现

#include "../include/ClientNetworkManager.h"
#include "../../common/include/User.h"
#include "../../common/include/Message.h"
#include "../../common/include/NetworkProtocol.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <chrono>

// 构造函数
ClientNetworkManager::ClientNetworkManager() 
    : m_socket(-1)
    , m_connected(false)
    , m_port(8080)
    , m_running(false)
{
    log("ClientNetworkManager Created");
}

// 析构函数
ClientNetworkManager::~ClientNetworkManager() 
{
    disconnect();
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
    
    // 初始化信息
    m_serverAddress = address;
    m_port = port;
    m_username = username;
    
    // 创建socket
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < 0) 
    {
        log("Failed to create socket");
        return false;
    }
    
    // 设置socket为非阻塞模式，以便能够及时退出
    int flags = fcntl(m_socket, F_GETFL, 0);
    if (flags == -1) 
    {
        log("Failed to get socket flags");
        close(m_socket);
        m_socket = -1;
        return false;
    }
    
    if (fcntl(m_socket, F_SETFL, flags | O_NONBLOCK) == -1) 
    {
        log("Failed to set socket to non-blocking mode");
        close(m_socket);
        m_socket = -1;
        return false;
    }
    
    // 设置服务器地址
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    
    // 转换IP地址（支持localhost）
    std::string resolvedAddress = address;
    if (address == "localhost") {
        resolvedAddress = "127.0.0.1";
    }
    
    if (inet_pton(AF_INET, resolvedAddress.c_str(), &serverAddr.sin_addr) <= 0) 
    {
        log("Invalid server address: " + address);
        close(m_socket);
        m_socket = -1;
        return false;
    }
    
    // 建立连接（非阻塞模式）
    if (::connect(m_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) 
    {
        if (errno != EINPROGRESS) 
        {
            log("Failed to connect to server: " + std::string(strerror(errno)));
            close(m_socket);
            m_socket = -1;
            return false;
        }
        
        // 连接正在进行中，使用select等待连接完成
        fd_set writefds;
        struct timeval timeout;
        
        FD_ZERO(&writefds);
        FD_SET(m_socket, &writefds);
        
        timeout.tv_sec = 5;  // 5秒超时
        timeout.tv_usec = 0;
        
        int selectResult = select(m_socket + 1, nullptr, &writefds, nullptr, &timeout);
        
        if (selectResult <= 0) 
        {
            log("Connection timeout or error: " + std::string(strerror(errno)));
            close(m_socket);
            m_socket = -1;
            return false;
        }
        
        // 检查连接是否成功
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(m_socket, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) 
        {
            log("Connection failed: " + std::string(strerror(error)));
            close(m_socket);
            m_socket = -1;
            return false;
        }
    }
    
    // 启动接收线程
    m_running = true;
    m_receiveThread = std::thread(&ClientNetworkManager::receiveLoop, this);// 第一个参数是成员函数指针，第二个参数是调用该成员函数的对象指针
    
    m_connected = true;
    
    // 触发连接成功回调函数，让用户知道连接成功
    if (m_onConnected)// 检查m_onConnected回调函数是否被设置（不是空指针）
    {
        m_onConnected();// 调用连接成功的回调函数
    }
    
    // 等待服务器完全准备好（100毫秒延迟，避免时序竞争）
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 连接成功后发送登录消息
    if (!sendLoginMessage()) 
    {
        log("Failed to send login message");
        disconnect();
        return false;
    }
    
    // 连接服务器成功（日志已隐藏）
    return true;
}

// 断开连接
void ClientNetworkManager::disconnect() 
{
    if (!m_connected) 
    {
        return;
    }
    
    m_running = false;
    
    // 关闭socket
    if (m_socket != -1) 
    {
        close(m_socket);
        m_socket = -1;
    }
    
    // 等待接收线程结束
    if (m_receiveThread.joinable())// 检查线程是否可以被加入（join）
    {
        m_receiveThread.join();// 主线程会在这里等待，直到接收线程执行完毕
    }
    
    m_connected = false;
    
    // 触发断开连接回调函数
    if (m_onDisconnected) 
    {
        m_onDisconnected();
    }
    
    log("Disconnected from server");
}

// 检查连接状态
bool ClientNetworkManager::isConnected() const 
{
	return m_connected;
}

// 发送消息
bool ClientNetworkManager::sendMessage(const std::string& content) 
{
	// 没有建立连接就无法发送消息
    if (!m_connected || m_socket == -1) 
    {
        log("Error: Not connected to server");
        if (m_onError) 
        {
            m_onError("Not connected to server");
        }
        return false;
    }
    
    // 创建网络消息并序列化
    Chat01::NetworkMessage netMessage(
        Chat01::MessageType::TEXT_MESSAGE,
        m_username,
        content
    );
    
    // 序列化消息
    std::string serializedMessage = Chat01::MessageSerializer::serialize(netMessage);
    
    // ::send()是全局作用域的send函数，POSIX标准的socket发送函数
    ssize_t sent = ::send(m_socket, serializedMessage.c_str(), serializedMessage.length(), 0);
    if (sent == -1) 
    {
        log("Failed to send message: " + std::string(strerror(errno)));
        if (m_onError) 
        {
            m_onError("Failed to send message: " + std::string(strerror(errno)));
        }
        return false;
    }
    
    log("Message sent successfully: " + content);
    return true;
}

// 发送登录消息
bool ClientNetworkManager::sendLoginMessage()
{
    if (!m_connected || m_socket == -1) 
    {
        log("Error: Not connected to server");
        return false;
    }
    
    // 创建登录消息
    Chat01::NetworkMessage loginMessage(
        Chat01::MessageType::LOGIN_REQUEST,
        m_username,
        m_username  // 使用用户名作为登录内容
    );
    
    // 序列化消息
    std::string serializedMessage = Chat01::MessageSerializer::serialize(loginMessage);
    
    // 发送登录消息
    ssize_t sent = ::send(m_socket, serializedMessage.c_str(), serializedMessage.length(), 0);
    if (sent == -1) 
    {
        log("Failed to send login message: " + std::string(strerror(errno)));
        return false;
    }
    
    // 登录消息发送成功（日志已隐藏）
    return true;
}

// 接收消息循环
void ClientNetworkManager::receiveLoop() 
{
    char buffer[4096];
    std::string recvBuf;

    while (m_running && m_connected) 
    {
        // 接收消息
        ssize_t received = recv(m_socket, buffer, sizeof(buffer), 0);

        // 处理接收结果
        if (received > 0) 
        {
            // 将数据追加到接收缓冲
            recvBuf.append(buffer, static_cast<size_t>(received));

            // 按行解析完整消息（每条消息以"\n"分隔）
            size_t pos = 0;
            while ((pos = recvBuf.find('\n')) != std::string::npos)
            {
                std::string line = recvBuf.substr(0, pos);
                if (!line.empty() && line.back() == '\r') line.pop_back();

                if (!line.empty())
                {
                    // 解析消息并触发回调
                    if (m_onMessageReceived) 
                    {
                        try {
                            Chat01::NetworkMessage netMessage = Chat01::MessageSerializer::deserialize(line);
                            switch (netMessage.type) {
                                case Chat01::MessageType::TEXT_MESSAGE:
                                    m_onMessageReceived(netMessage.sender, netMessage.content);
                                    break;
                                case Chat01::MessageType::USER_JOIN:
                                    m_onMessageReceived(netMessage.sender, netMessage.content);
                                    break;
                                case Chat01::MessageType::USER_LEAVE:
                                    m_onMessageReceived(netMessage.sender, netMessage.content);
                                    break;
                                case Chat01::MessageType::ERROR_MESSAGE:
                                    m_onMessageReceived("系统错误", netMessage.content);
                                    break;
                                default:
                                    m_onMessageReceived("系统", "收到未知类型的消息");
                                    break;
                            }
                        }
                        catch (const std::exception& e) {
                            log("消息反序列化失败: " + std::string(e.what()));
                            m_onMessageReceived("服务器", line);
                        }
                    }
                }

                // 移除已处理的行
                recvBuf.erase(0, pos + 1);
            }
        }
        else if (received == 0) 
        {
            // 服务器关闭连接
            log("Server closed the connection");
            m_connected = false;
            if (m_onDisconnected) 
            {
                m_onDisconnected();
            }
            break;
        }
	// 接收错误
        else
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK) 
            {
                log("Error receiving message: " + std::string(strerror(errno)));
                if (m_onError) 
                {
                    m_onError("Error receiving message: " + std::string(strerror(errno)));
                }
                break;
            }
        }
        
        // 短暂睡眠以避免高CPU使用率，同时让线程能够及时退出
        for (int i = 0; i < 10 && m_running; i++) 
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    log("接收线程已退出");
}

// 打日志
void ClientNetworkManager::log(const std::string& message) 
{
    std::cout << "[ClientNetworkManager] " << message << std::endl;
}

// 回调函数实现
void ClientNetworkManager::setOnConnected(std::function<void()> callback) 
{
    m_onConnected = callback;
}

void ClientNetworkManager::setOnDisconnected(std::function<void()> callback) 
{
    m_onDisconnected = callback;
}

void ClientNetworkManager::setOnMessageReceived(std::function<void(const std::string&, const std::string&)> callback) 
{
    m_onMessageReceived = callback;
}

void ClientNetworkManager::setOnError(std::function<void(const std::string&)> callback) 
{
    m_onError = callback;
}