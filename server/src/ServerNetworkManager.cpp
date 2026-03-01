// ServerNetworkManager.cpp - 服务器网络管理器实现

#include "../include/ServerNetworkManager.h"
#include "../../common/include/PlatformNetwork.h"
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <future>
#include <chrono>

// --- Windows 兼容层修复 ---
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
    log("ServerNetworkManager 创建");
}

// 析构函数
ServerNetworkManager::~ServerNetworkManager() 
{
    stop();
    PlatformNetwork::Cleanup();
    log("ServerNetworkManager 销毁");
}

// 开始运行
bool ServerNetworkManager::start(int port, const std::string& address) 
{
    if (m_running) 
    {
        log("服务器已经在运行");
        return false;
    }
    
    m_serverSocket = PlatformNetwork::CreateSocket();
    if (!PlatformNetwork::IsValidSocket(m_serverSocket)) 
    {
        log("创建socket失败");
        return false;
    }
    
    // 设置SO_REUSEADDR（Windows 要求 optlen 为 int）
#ifdef _WIN32
    char opt = 1;
    int optlen = sizeof(opt);
#else
    int opt = 1;
    socklen_t optlen = sizeof(opt);
#endif
    if (setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), optlen) < 0) 
    {
        log("设置socket选项失败");
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
        log("绑定socket失败: " + PlatformNetwork::GetErrorMessage());
        PlatformNetwork::CloseSocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET_VALUE;
        return false;
    }
    
    if (listen(m_serverSocket, SOMAXCONN) < 0) 
    {
        log("监听socket失败: " + PlatformNetwork::GetErrorMessage());
        PlatformNetwork::CloseSocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET_VALUE;
        return false;
    }
    
    log("服务器开始监听端口 " + std::to_string(port));
    
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
    
    // 停止所有客户端
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (SocketType socket : m_clientSockets) 
        {
            if (PlatformNetwork::IsValidSocket(socket))
            {
                PlatformNetwork::CloseSocket(socket);
            }
        }
        m_clientSockets.clear();
    }
    
    if (m_acceptThread.joinable()) m_acceptThread.join();
    log("服务器已完全停止");
}

bool ServerNetworkManager::isRunning() const {return m_running;}

void ServerNetworkManager::addClientSocket(SocketType socket)
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    m_clientSockets.push_back(socket);
}

void ServerNetworkManager::removeClientSocket(SocketType socket)
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    auto it = std::find(m_clientSockets.begin(), m_clientSockets.end(), socket);
    if (it != m_clientSockets.end()) m_clientSockets.erase(it);
}

bool ServerNetworkManager::isClientSocketValid(SocketType socket) const
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    return std::find(m_clientSockets.begin(), m_clientSockets.end(), socket) != m_clientSockets.end();
}

void ServerNetworkManager::disconnectClient(SocketType clientSocket)
{
    if (isClientSocketValid(clientSocket)) 
    {
        PlatformNetwork::CloseSocket(clientSocket);
        removeClientSocket(clientSocket);
    }
}

// 接受连接线程函数
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
        
        // 【修复】select 的第一个参数在 Windows 上其实被忽略，但为了规范传值
        int selectResult = select((int)m_serverSocket + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (selectResult > 0 && FD_ISSET(m_serverSocket, &readfds))
        {
            struct sockaddr_in clientAddr;
#ifdef _WIN32
            int clientAddrLen = sizeof(clientAddr);
#else
            socklen_t clientAddrLen = sizeof(clientAddr);
#endif
            SocketType clientSocket = accept(m_serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
            
            if (!PlatformNetwork::IsValidSocket(clientSocket)) continue;
            
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
            std::string clientAddress = std::string(clientIP) + ":" + std::to_string(ntohs(clientAddr.sin_port));
            
            log("客户端连接: socket=" + std::to_string(clientSocket));
            addClientSocket(clientSocket);
            
            if (m_onClientConnected) m_onClientConnected(clientSocket, clientAddress);
            
            // 【修复】使用跨平台非阻塞设置替换 fcntl
            PlatformNetwork::SetNonBlocking(clientSocket, true);

            {
                std::lock_guard<std::mutex> lock(m_threadsMutex);
                m_clientThreads[clientSocket] = std::thread(&ServerNetworkManager::handleClient, this, clientSocket);
            }
        }
    }
}

// 处理单个客户端的线程函数
void ServerNetworkManager::handleClient(SocketType clientSocket) 
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
            // 【修复】统一使用 PlatformNetwork::Receive
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
            else if (bytesReceived == 0) break;
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

void ServerNetworkManager::cleanupClient(SocketType clientSocket) 
{
    PlatformNetwork::CloseSocket(clientSocket);
    removeClientSocket(clientSocket);
    if (m_onClientDisconnected) m_onClientDisconnected(clientSocket);
}

// 发送消息给特定客户端
bool ServerNetworkManager::sendToClient(SocketType clientSocket, const std::string& message) 
{
    if (!isClientSocketValid(clientSocket)) return false;
    
    // 【修复】统一使用 PlatformNetwork::Send
    int bytesSent = PlatformNetwork::Send(clientSocket, message.c_str(), (int)message.length());
    
    if (bytesSent < 0) 
    {
        // 立即捕获错误信息，避免被其他线程覆盖
        std::string errorMsg = PlatformNetwork::GetErrorMessage();
        log("发送消息失败: " + errorMsg);
        return false;
    }
    return true;
}

void ServerNetworkManager::broadcastMessage(const std::string& message)
{
    std::vector<SocketType> clients = getAllClientSockets();
    for (SocketType clientSocket : clients) sendToClient(clientSocket, message);
}

void ServerNetworkManager::broadcastMessageExclude(SocketType excludeSocket, const std::string& message)
{
    std::vector<SocketType> clients = getAllClientSockets();
    for (SocketType clientSocket : clients)
    {
        if (clientSocket != excludeSocket) sendToClient(clientSocket, message);
    }
}

void ServerNetworkManager::log(const std::string& message) { std::cout << "[ServerNetworkManager] " << message << std::endl; }

int ServerNetworkManager::getClientCount() const { std::lock_guard<std::mutex> lock(m_clientsMutex); return (int)m_clientSockets.size(); }
std::vector<SocketType> ServerNetworkManager::getAllClientSockets() const { std::lock_guard<std::mutex> lock(m_clientsMutex); return m_clientSockets; }
void ServerNetworkManager::setOnClientConnected(std::function<void(SocketType, const std::string&)> cb) { m_onClientConnected = cb; }
void ServerNetworkManager::setOnClientDisconnected(std::function<void(SocketType)> cb) { m_onClientDisconnected = cb; }
void ServerNetworkManager::setOnMessageReceived(std::function<void(SocketType, const std::string&)> cb) { m_onMessageReceived = cb; }