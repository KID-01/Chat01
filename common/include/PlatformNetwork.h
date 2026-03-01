#ifndef PLATFORMNETWORK_H
#define PLATFORMNETWORK_H

#ifdef _WIN32
    // Windows平台
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    
    // 必须在包含其他头文件前处理 errno 冲突
    #include <cerrno> 
    
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    
    #pragma comment(lib, "ws2_32.lib")
    
    typedef SOCKET SocketType;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
    #define CLOSE_SOCKET closesocket
    #define GET_LAST_ERROR WSAGetLastError()
    
    // 强制使用 Winsock 错误码（避免被 <cerrno> 的 EWOULDBLOCK=11 等覆盖，导致非阻塞 connect 被误判为“拒绝”）
    #undef EWOULDBLOCK
    #undef EINPROGRESS
    #undef EAGAIN
    #define EWOULDBLOCK   WSAEWOULDBLOCK
    #define EINPROGRESS  WSAEINPROGRESS
    #define EAGAIN       WSAEWOULDBLOCK

#else
    // Unix/Linux平台
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/select.h>
    #include <netdb.h>
    #include <errno.h>
    
    typedef int SocketType;
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
    #define CLOSE_SOCKET close
    #define GET_LAST_ERROR errno
#endif

#include <string>

namespace PlatformNetwork {

    // 初始化网络库（仅Windows需要，Linux可为空实现）
    bool Initialize();
    
    // 清理网络库（仅Windows需要，Linux可为空实现）
    void Cleanup();
    
    // 创建socket
    SocketType CreateSocket();
    
    // 关闭socket
    int CloseSocket(SocketType socket);
    
    // 连接到服务器
    int ConnectToServer(SocketType socket, const std::string& address, int port);
    
    // 设置非阻塞模式
    int SetNonBlocking(SocketType socket, bool nonBlocking);
    
    // 使用跨平台的select
    int Select(SocketType maxSocket, fd_set* readSet, fd_set* writeSet, fd_set* exceptSet, int timeoutSec);
    
    // 发送数据
    int Send(SocketType socket, const char* buffer, int len);
    
    // 接收数据
    int Receive(SocketType socket, char* buffer, int len);
    
    // 检查socket是否有效
    bool IsValidSocket(SocketType socket);
    
    // 获取详细错误描述字符串
    std::string GetErrorMessage();
}

#endif // PLATFORMNETWORK_H