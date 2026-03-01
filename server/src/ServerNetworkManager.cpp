// ServerNetworkManager.cpp - 服务器网络管理器实现

#include "../include/ServerNetworkManager.h"
#include "../../common/include/PlatformNetwork.h"
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <future>
#include <chrono>

// --- Windows 兼容性定义 ---
#ifdef _WIN32
    typedef intptr_t ssize_t;
    #define close closesocket
#else
    #include <fcntl.h>
    #include <unistd.h>
#endif

// 构造函数
ServerNetworkManager::ServerNetworkManager() 
    : m_serverSocket(INVALID_SOCKET_VALUE)
    , m_running(false) 
{
    PlatformNetwork::Initialize();
    log("ServerNetworkManager 初始化");
}

// 析构函数
ServerNetworkManager::~ServerNetworkManager() 
{
    stop();
    PlatformNetwork::Cleanup();
    log("ServerNetworkManager 已析构");
}

// 启动服务器
bool ServerNetworkManager::start(int port, const std::string& address) 
{
    if (m_running) 
    {
        log("服务器正在运行中");
        return false;
    }
    
    m_serverSocket = PlatformNetwork::CreateSocket();
    if (!PlatformNetwork::IsValidSocket(m_serverSocket)) 
    {
        log("创建 socket 失败");
        return false;
    }
    
    // 设置 SO_REUSEADDR
#ifdef _WIN32
    char opt = 1;
#else
    int opt = 1;
#endif
    if (setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) 
    {
        log("设置 socket 选项失败");
        PlatformNetwork::CloseSocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET_VALUE;
        return false;
    }
    
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(address.c_str());
    
    if (bind(m_serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) 
    {
        log("绑定 socket 失败: " + PlatformNetwork::GetErrorMessage());
        PlatformNetwork::CloseSocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET_VALUE;
        return false;
    }
    
    if (listen(m_serverSocket, SOMAXCONN) < 0) 
    {
        log("监听 socket 失败: " + PlatformNetwork::GetErrorMessage());
        PlatformNetwork::CloseSocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET_VALUE;
        return false;
    }
    
    log("服务器开始监听端口: " + std::to_string(port));
    
    m_running = true;
    m_acceptThread = std::thread(&ServerNetworkManager::acceptConnections, this);
    
    return true;
}

// 停止服务器
void ServerNetworkManager::stop() 
{
    if (!m_running) return;
    m_running = false;
    
    if (PlatformNetwork::IsValidSocket(m_serverSocket)) 
    {
#ifdef _WIN32
        shutdown(m_serverSocket, SD_BOTH);
#else
        shutdown(m_serverSocket, SHUT_RDWR);
#endif
        PlatformNetwork::CloseSocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET_VALUE;
    }
    
    // 清理所有客户端
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (int socket : m_clientSockets) 
        {
            if (PlatformNetwork::IsValidSocket(socket))
            {
                PlatformNetwork::CloseSocket(socket);
            }
        }
        m_clientSockets.clear();
    }
    
    if (m_acceptThread.joinable()) m_acceptThread.join();
    log("服务器已停止运行");
}

bool ServerNetworkManager::isRunning() const {return m_running;}

void ServerNetworkManager::addClientSocket(int socket) 
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    m_clientSockets.push_back(socket);
}

void ServerNetworkManager::removeClientSocket(int socket) 
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    auto it = std::find(m_clientSockets.begin(), m_clientSockets.end(), socket);
    if (it != m_clientSockets.end()) m_clientSockets.erase(it);
}

bool ServerNetworkManager::isClientSocketValid(int socket) const 
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    return std::find(m_clientSockets.begin(), m_clientSockets.end(), socket) != m_clientSockets.end();
}

void ServerNetworkManager::disconnectClient(int clientSocket) 
{
    if (isClientSocketValid(clientSocket)) 
    {
        PlatformNetwork::CloseSocket(clientSocket);
        removeClientSocket(clientSocket);
    }
}

// 接受客户端连接
void ServerNetworkManager::acceptConnections() 
{
    while (m_running) 
    {
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(m_serverSocket, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        
        // 使用 select 处理超时，防止 Windows 下主线程关闭时阻塞
        int selectResult = select((int)m_serverSocket + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (selectResult > 0 && FD_ISSET(m_serverSocket, &readfds))
        {
            struct sockaddr_in clientAddr;
            socklen_t clientAddrLen = sizeof(clientAddr);
            int clientSocket = (int)accept(m_serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
            
            if (clientSocket < 0) continue;
            
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
            std::string clientAddress = std::string(clientIP) + ":" + std::to_string(ntohs(clientAddr.sin_port));
            
            log("新客户端连接: socket=" + std::to_string(clientSocket));
            addClientSocket(clientSocket);
            
            if (m_onClientConnected) m_onClientConnected(clientSocket, clientAddress);
            
            // 设置非阻塞模式，通过 fcntl 或系统调用
            PlatformNetwork::SetNonBlocking(clientSocket, true);

            {
                std::lock_guard<std::mutex> lock(m_threadsMutex);
                m_clientThreads[clientSocket] = std::thread(&ServerNetworkManager::handleClient, this, clientSocket);
            }
        }
    }
}

// 处理特定客户端的数据
void ServerNetworkManager::handleClient(int clientSocket) 
{
    char buffer[1024];
    std::string recvBuf;

    while (m_running && isClientSocketValid(clientSocket))
    {
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(clientSocket, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 50000;
        
        int selectResult = select(clientSocket + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (selectResult > 0 && FD_ISSET(clientSocket, &readfds))
        {
            // 使用跨平台封装 PlatformNetwork::Receive
            int bytesReceived = PlatformNetwork::Receive(clientSocket, buffer, sizeof(buffer));

            if (bytesReceived > 0)
            {
                recvBuf.append(buffer, static_cast<size_t>(bytesReceived));
                size_t pos = 0;
                while ((pos = recvBuf.find('\n')) != std::string::npos)
                {
                    std::string line = recvBuf.substr(0, pos);
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (!line.empty() && m_onMessageReceived) m_onMessageReceived(clientSocket, line);
                    recvBuf.erase(0, pos + 1);
                }
            }
            else if (bytesReceived == 0) break; // 客户端主动断开
            else
            {
                int err = GET_LAST_ERROR;
                if (err != EAGAIN && err != EWOULDBLOCK) break;
            }
        }
        if (!m_running) break;
    }
    
    cleanupClient(clientSocket);
}

void ServerNetworkManager::cleanupClient(int clientSocket) 
{
    PlatformNetwork::CloseSocket(clientSocket);
    removeClientSocket(clientSocket);
    if (m_onClientDisconnected) m_onClientDisconnected(clientSocket);
}

// 发送消息到指定客户端
bool ServerNetworkManager::sendToClient(int clientSocket, const std::string& message) 
{
    if (!isClientSocketValid(clientSocket)) return false;
    
    // 使用跨平台封装 PlatformNetwork::Send
    int bytesSent = PlatformNetwork::Send(clientSocket, message.c_str(), (int)message.length());
    
    if (bytesSent < 0) 
    {
        log("发送消息失败: " + PlatformNetwork::GetErrorMessage());
        return false;
    }
    return true;
}

// 广播消息给所有连接的客户端
void ServerNetworkManager::broadcastMessage(const std::string& message) 
{
    std::vector<int> clients = getAllClientSockets();
    for (int clientSocket : clients) sendToClient(clientSocket, message);
}

void ServerNetworkManager::broadcastMessageExclude(int excludeSocket, const std::string& message) 
{
    std::vector<int> clients = getAllClientSockets();
    for (int clientSocket : clients) 
    {
        if (clientSocket != excludeSocket) sendToClient(clientSocket, message);
    }
}

void ServerNetworkManager::log(const std::string& message) { std::cout << "[ServerNetworkManager] " << message << std::endl; }

int ServerNetworkManager::getClientCount() const { std::lock_guard<std::mutex> lock(m_clientsMutex); return (int)m_clientSockets.size(); }
std::vector<int> ServerNetworkManager::getAllClientSockets() const { std::lock_guard<std::mutex> lock(m_clientsMutex); return m_clientSockets; }
void ServerNetworkManager::setOnClientConnected(std::function<void(int, const std::string&)> cb) { m_onClientConnected = cb; }
void ServerNetworkManager::setOnClientDisconnected(std::function<void(int)> cb) { m_onClientDisconnected = cb; }
void ServerNetworkManager::setOnMessageReceived(std::function<void(int, const std::string&)> cb) { m_onMessageReceived = cb; }