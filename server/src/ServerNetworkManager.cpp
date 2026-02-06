// ServerNetworkManager.cpp - 服务器网络管理器实现

#include "../include/ServerNetworkManager.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <stdexcept>
#include <algorithm>  // 需要这个头文件来使用std::find
#include <future>      // 用于线程超时控制
#include <chrono>      // 用于时间控制

// 构造函数
ServerNetworkManager::ServerNetworkManager() 
    : m_serverSocket(-1)
    , m_running(false) 
{
    log("ServerNetworkManager 创建");
}

// 析构函数
ServerNetworkManager::~ServerNetworkManager() 
{
    stop();
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
    
    // 创建socket
    m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverSocket < 0) 
    {
        log("创建socket失败");
        return false;
    }
    
    // 设置SO_REUSEADDR（快速重启，告诉操作系统：即使这个端口还在TIME_WAIT状态，也让我重新使用它）
    int opt = 1;
    if (setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) 
    {
        log("设置SO_REUSEADDR失败");
        close(m_serverSocket);
        return false;
    }
    
    // 设置socket为非阻塞模式，以便能够及时退出
    int flags = fcntl(m_serverSocket, F_GETFL, 0);
    if (flags == -1) 
    {
        log("获取socket标志失败");
        close(m_serverSocket);
        return false;
    }
    
    if (fcntl(m_serverSocket, F_SETFL, flags | O_NONBLOCK) == -1) 
    {
        log("设置socket为非阻塞模式失败");
        close(m_serverSocket);
        return false;
    }
    
    // 绑定地址和端口
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;// 指定使用IPv4地址家族
    serverAddr.sin_port = htons(port);// 设置端口号，并转换为网络字节序
    
    if (address == "0.0.0.0") 
    {
        serverAddr.sin_addr.s_addr = INADDR_ANY;// 监听所有网卡
    } 
    else 
    {
        serverAddr.sin_addr.s_addr = inet_addr(address.c_str());// 监听特定IP
    }
    
    if (bind(m_serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) 
    {
        log("绑定地址失败: " + std::string(strerror(errno)));
        close(m_serverSocket);
        return false;
    }
    
    // 开始监听，最多5个等待连接
    if (listen(m_serverSocket, 5) < 0) 
    {  
        log("监听失败");
        close(m_serverSocket);
        return false;
    }
    
    // 启动接受连接线程
    m_running = true;
    m_acceptThread = std::thread(&ServerNetworkManager::acceptConnections, this);
    
    log("服务器启动成功，监听端口: " + std::to_string(port));
    return true;
}

// 停止服务器
void ServerNetworkManager::stop() 
{
    if (!m_running){return;}
    
    m_running = false;
    
    // 首先关闭服务器socket，这会中断accept()
    if (m_serverSocket != -1) 
    {
        // 使用shutdown()确保所有连接都被中断
        shutdown(m_serverSocket, SHUT_RDWR);
        close(m_serverSocket);
        m_serverSocket = -1;
        log("服务器socket已关闭");
    }
    
    // 强制关闭所有客户端连接
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        log("正在关闭 " + std::to_string(m_clientSockets.size()) + " 个客户端连接");
        
        for (int socket : m_clientSockets) 
	{
            if (socket != -1)
            {
                // 使用shutdown()确保客户端收到断开信号
                shutdown(socket, SHUT_RDWR);
                close(socket);
                log("强制关闭客户端socket: " + std::to_string(socket));
            }
        }
        
	m_clientSockets.clear();
    }
    
    // 等待接受连接线程结束（最多等待3秒）
    if (m_acceptThread.joinable()) 
    {
        log("等待接受连接线程结束...");
        auto future = std::async(std::launch::async, &std::thread::join, &m_acceptThread);
        if (future.wait_for(std::chrono::seconds(3)) == std::future_status::timeout) 
        {
            log("接受连接线程超时，强制分离");
            m_acceptThread.detach();
        }
        else
        {
            log("接受连接线程已结束");
        }
    }
    
    // 等待所有客户端线程结束（简化版本）
    log("等待 " + std::to_string(m_clientThreads.size()) + " 个客户端线程结束...");
    
    // 首先强制关闭所有客户端socket，确保线程能检测到退出条件
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (int socket : m_clientSockets) 
	{
            if (socket != -1)
            {
                shutdown(socket, SHUT_RDWR);
                close(socket);
            }
        }
        m_clientSockets.clear();
    }
    
    // 给线程一点时间自然退出
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 直接join线程，设置超时
    for (auto& pair : m_clientThreads)
    {
        if (pair.second.joinable())
        {
            // 直接join，最多等待500ms
            auto start = std::chrono::steady_clock::now();
            
            // 使用简单的超时机制
            while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500)) {
                // 尝试join，设置超时
                if (pair.second.joinable()) {
                    // 直接join，不进行复杂的轮询
                    pair.second.join();
                    log("客户端线程已结束: socket=" + std::to_string(pair.first));
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            // 如果超时后线程仍然joinable，强制分离
            if (pair.second.joinable()) {
                log("客户端线程超时，强制分离: socket=" + std::to_string(pair.first));
                pair.second.detach();
            }
        }
    }
    
    m_clientThreads.clear();
    
    log("服务器已完全停止");
}

// 返回服务器运行状态判定
bool ServerNetworkManager::isRunning() const {return m_running;}

// 线程安全工具方法
// 添加客户端
void ServerNetworkManager::addClientSocket(int socket) 
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    m_clientSockets.push_back(socket);
    log("添加客户端: socket=" + std::to_string(socket));
}

// 移除客户端
void ServerNetworkManager::removeClientSocket(int socket) 
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    auto it = std::find(m_clientSockets.begin(), m_clientSockets.end(), socket);
    if (it != m_clientSockets.end()) 
    {
        m_clientSockets.erase(it);
    }
    log("移除客户端: socket=" + std::to_string(socket));
}

// 客户端是否是有效连接
bool ServerNetworkManager::isClientSocketValid(int socket) const 
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    return std::find(m_clientSockets.begin(), m_clientSockets.end(), socket) != m_clientSockets.end();
}

// 连接管理，断开连接
void ServerNetworkManager::disconnectClient(int clientSocket) 
{
    if (isClientSocketValid(clientSocket)) 
    {
        close(clientSocket);
        removeClientSocket(clientSocket);
    }
}

// 获取客户端连接数
int ServerNetworkManager::getClientCount() const 
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    return m_clientSockets.size();
}

// 获取所有连接的客户端
std::vector<int> ServerNetworkManager::getAllClientSockets() const 
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    return m_clientSockets;  // 返回副本
}

// 回调函数设置
void ServerNetworkManager::setOnClientConnected(std::function<void(int, const std::string&)> callback) 
{
    m_onClientConnected = callback;
}

void ServerNetworkManager::setOnClientDisconnected(std::function<void(int)> callback) 
{
    m_onClientDisconnected = callback;
}

void ServerNetworkManager::setOnMessageReceived(std::function<void(int, const std::string&)> callback) 
{
    m_onMessageReceived = callback;
}

// 接受连接线程函数
void ServerNetworkManager::acceptConnections() 
{
    log("开始接受客户端连接");
    
    while (m_running) 
    {
        // 使用select()监控服务器socket，避免accept()阻塞
        fd_set readfds;
        struct timeval timeout;
        
        FD_ZERO(&readfds);
        FD_SET(m_serverSocket, &readfds);
        
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;  // 100ms超时，定期检查退出条件
        
        int selectResult = select(m_serverSocket + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (selectResult > 0 && FD_ISSET(m_serverSocket, &readfds))
        {
            // 有新的连接请求
            struct sockaddr_in clientAddr;
            socklen_t clientAddrLen = sizeof(clientAddr);
            
            int clientSocket = accept(m_serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
            
            if (clientSocket < 0) 
            {
                if (errno != EAGAIN && errno != EWOULDBLOCK) 
                {
                    log("接受连接失败: " + std::string(strerror(errno)));
                }
                continue;
            }
            
            // 获取客户端IP地址
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
            std::string clientAddress = std::string(clientIP) + ":" + std::to_string(ntohs(clientAddr.sin_port));
            
            log("客户端连接: socket=" + std::to_string(clientSocket) + ", IP=" + clientAddress);
            
            // 添加客户端到管理列表
            addClientSocket(clientSocket);
            
            

            // 调用客户端连接回调
            if (m_onClientConnected) 
            {
                m_onClientConnected(clientSocket, clientAddress);
            }
            
            
            
            
            // 设置客户端socket为非阻塞模式
            int flags = fcntl(clientSocket, F_GETFL, 0);
            if (flags != -1) 
            {
                fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);
            }

            // 为每个客户端创建处理线程（加锁保护）
            {
                std::lock_guard<std::mutex> lock(m_threadsMutex);
                
                // 检查socket是否已经在线程映射中（避免重复创建）
                auto it = m_clientThreads.find(clientSocket);
                if (it != m_clientThreads.end())
                {
                    // 如果线程还在运行，先等待它结束
                    if (it->second.joinable())
                    {
                        log("等待现有线程结束: socket=" + std::to_string(clientSocket));
                        it->second.join();
                    }
                    // 移除旧的线程记录
                    m_clientThreads.erase(it);
                }
                
                // 创建新的处理线程
                m_clientThreads[clientSocket] = std::thread(&ServerNetworkManager::handleClient, this, clientSocket);
            }

            
        }
        else if (selectResult == 0)
        {
            // 超时，没有新连接，继续检查退出条件
            continue;
        }
        else
        {
            // select错误
            if (errno != EINTR)  // 忽略信号中断
            {
                log("select错误: " + std::string(strerror(errno)));
                break;
            }
        }
    }
    
    log("停止接受客户端连接");
}

// 处理单个客户端的线程函数
void ServerNetworkManager::handleClient(int clientSocket) 
{
    log("开始处理客户端: socket=" + std::to_string(clientSocket));

    char buffer[1024];
    std::string recvBuf; // 累积接收数据，处理粘包/半包（按行分割）

    while (m_running && isClientSocketValid(clientSocket))
    {
        // 使用select()监控socket状态，避免recv()阻塞
        fd_set readfds;
        struct timeval timeout;
        
        FD_ZERO(&readfds);
        FD_SET(clientSocket, &readfds);
        
        timeout.tv_sec = 0;
        timeout.tv_usec = 50000;  // 50ms超时，更频繁检查退出条件
        
        int selectResult = select(clientSocket + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (selectResult > 0 && FD_ISSET(clientSocket, &readfds))
        {
            // 有数据可读
            ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

            if (bytesReceived > 0)
            {
                // 追加到缓冲区
                recvBuf.append(buffer, static_cast<size_t>(bytesReceived));

                // 逐行提取完整消息（以"\n"分隔）
                size_t pos = 0;
                while ((pos = recvBuf.find('\n')) != std::string::npos)
                {
                    std::string line = recvBuf.substr(0, pos);
                    if (!line.empty() && line.back() == '\r') line.pop_back();

                    if (!line.empty())
                    {
                        log("收到消息[socket=" + std::to_string(clientSocket) + "]: " + line);
                        if (m_onMessageReceived)
                        {
                            m_onMessageReceived(clientSocket, line);
                        }
                    }

                    // 移除已处理部分
                    recvBuf.erase(0, pos + 1);
                }
            }
            else if (bytesReceived == 0)
            {
                // 客户端正常断开
                log("客户端断开连接: socket=" + std::to_string(clientSocket));
                break;
            }
            else
            {
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    log("接收消息错误: " + std::string(strerror(errno)));
                    break;
                }
            }
        }
        else if (selectResult == 0)
        {
            // 超时，没有数据可读，继续检查退出条件
            continue;
        }
        else
        {
            // select错误（包括EINTR信号中断）
            if (errno == EINTR) {
                // 信号中断，检查是否需要退出
                if (!m_running) {
                    log("收到退出信号，结束客户端处理线程");
                    break;
                }
            } else {
                log("select错误: " + std::string(strerror(errno)));
                break;
            }
        }
        
        // 额外检查：如果服务器正在停止，立即退出
        if (!m_running) {
            log("服务器停止信号检测，结束客户端处理线程");
            break;
        }
    }
    
    // 客户端断开连接后的清理
    cleanupClient(clientSocket);
    
    log("结束处理客户端: socket=" + std::to_string(clientSocket));
}

// 清理客户端资源
void ServerNetworkManager::cleanupClient(int clientSocket) 
{
    // 关闭客户端socket
    close(clientSocket);
    
    // 从管理列表中移除
    removeClientSocket(clientSocket);
    
    // 从线程映射中移除对应的线程记录
    // 避免socket号重用时的冲突
    {
        std::lock_guard<std::mutex> lock(m_threadsMutex);
        auto threadIt = m_clientThreads.find(clientSocket);
        if (threadIt != m_clientThreads.end())
        {
            // 检查线程是否还在运行，如果已经结束就移除
            if (!threadIt->second.joinable())
            {
                m_clientThreads.erase(threadIt);
                log("从线程映射中移除已结束的线程: socket=" + std::to_string(clientSocket));
            }
            // 如果线程还在运行，保持记录，等待线程自然结束
        }
    }
    
    // 调用客户端断开连接回调
    if (m_onClientDisconnected) 
    {
        m_onClientDisconnected(clientSocket);
    }
    
    log("清理客户端资源: socket=" + std::to_string(clientSocket));
}

// 发送消息给特定客户端
bool ServerNetworkManager::sendToClient(int clientSocket, const std::string& message) 
{
	// 客户端有效性检查
    if (!isClientSocketValid(clientSocket)) 
    {
        log("发送失败：无效的客户端socket: " + std::to_string(clientSocket));
        return false;
    }
    
    ssize_t bytesSent = send(clientSocket, message.c_str(), message.length(), 0);
    
    if (bytesSent < 0) 
    {
        log("发送消息失败[socket=" + std::to_string(clientSocket) + "]: " + std::string(strerror(errno)));
        return false;
    }

    // 检查实际发送的字节数是否等于消息长度
    if (bytesSent != static_cast<ssize_t>(message.length())) 
    {
        log("发送消息不完整[socket=" + std::to_string(clientSocket) + "]: " + 
            std::to_string(bytesSent) + "/" + std::to_string(message.length()) + " bytes");
        return false;
    }
    
    log("发送消息成功[socket=" + std::to_string(clientSocket) + "]: " + message);
    return true;
}

// 广播消息给所有客户端
void ServerNetworkManager::broadcastMessage(const std::string& message) 
{
    // 先复制客户端列表，避免在锁内调用sendToClient（防止双重锁死锁）
    std::vector<int> clients;
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        clients = m_clientSockets;  // 复制一份
    }
    
    int successCount = 0;
    int totalCount = clients.size();
    
    log("开始广播消息给 " + std::to_string(totalCount) + " 个客户端");
    
    for (int clientSocket : clients) 
    {
        if (sendToClient(clientSocket, message)) 
        {
            successCount++;
        }
    }
    
    log("广播完成: " + std::to_string(successCount) + "/" + std::to_string(totalCount) + " 个客户端成功");
}

// 组播消息，排除指定客户端
void ServerNetworkManager::broadcastMessageExclude(int excludeSocket, const std::string& message) 
{
    // 先复制客户端列表，避免在锁内调用sendToClient（防止双重锁死锁）
    std::vector<int> clients;
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        clients = m_clientSockets;  // 复制一份
    }
    
    int successCount = 0;
    int totalCount = 0;
    
    // 计算实际要发送的客户端数量
    for (int clientSocket : clients) 
    {
        if (clientSocket != excludeSocket) 
        {
            totalCount++;
        }
    }
    
    log("开始组播消息（排除socket=" + std::to_string(excludeSocket) + "），发送给 " + 
        std::to_string(totalCount) + " 个客户端");
    
    for (int clientSocket : clients) 
    {
        if (clientSocket != excludeSocket) 
        {
            if (sendToClient(clientSocket, message)) 
            {
                successCount++;
            }
        }
    }
    
    log("组播完成: " + std::to_string(successCount) + "/" + std::to_string(totalCount) + " 个客户端成功");
}

// 日志
void ServerNetworkManager::log(const std::string& message) {std::cout << "[ServerNetworkManager] " << message << std::endl;}