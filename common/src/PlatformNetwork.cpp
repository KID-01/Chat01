#include "PlatformNetwork.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
static bool g_wsaInitialized = false;
#endif

namespace PlatformNetwork {

    bool Initialize() {
#ifdef _WIN32
        if (g_wsaInitialized) {
            return true;
        }
        
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cerr << "WSAStartup failed: " << result << std::endl;
            return false;
        }
        g_wsaInitialized = true;
        return true;
#else
        return true; // Unix不需要初始化
#endif
    }

    void Cleanup() {
#ifdef _WIN32
        if (g_wsaInitialized) {
            WSACleanup();
            g_wsaInitialized = false;
        }
#endif
    }

    SocketType CreateSocket() {
        SocketType sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET_VALUE) {
#ifdef _WIN32
            std::cerr << "Failed to create socket: " << GET_LAST_ERROR << std::endl;
#else
            std::cerr << "Failed to create socket: " << strerror(GET_LAST_ERROR) << std::endl;
#endif
        }
        return sock;
    }

    int CloseSocket(SocketType socket) {
        if (!IsValidSocket(socket)) {
            return 0; // Already closed or invalid
        }
        
        int result = CLOSE_SOCKET(socket);
        if (result == SOCKET_ERROR_VALUE) {
#ifdef _WIN32
            std::cerr << "Failed to close socket: " << GET_LAST_ERROR << std::endl;
#else
            std::cerr << "Failed to close socket: " << strerror(GET_LAST_ERROR) << std::endl;
#endif
        }
        return result;
    }

    int ConnectToServer(SocketType socket, const std::string& address, int port) {
        // 处理localhost特殊地址
        std::string targetAddress = address;
        if (address == "localhost") {
            targetAddress = "127.0.0.1";
        }

        // 检查是否为IP地址
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);

        int result = inet_pton(AF_INET, targetAddress.c_str(), &serverAddr.sin_addr);
        if (result > 0) {
            // 是IP地址，直接连接
            return ::connect(socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
        }

        // 是域名，需要解析
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        std::string portStr = std::to_string(port);
        int status = getaddrinfo(targetAddress.c_str(), portStr.c_str(), &hints, &res);
        if (status != 0) {
            std::cerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;
            return SOCKET_ERROR_VALUE;
        }

        // 尝试连接到解析出的第一个地址
        int connectResult = ::connect(socket, res->ai_addr, (int)res->ai_addrlen);
        freeaddrinfo(res);

        return connectResult;
    }

    int SetNonBlocking(SocketType socket, bool nonBlocking) {
#ifdef _WIN32
    u_long mode = nonBlocking ? 1 : 0;
    int result = ioctlsocket(socket, FIONBIO, &mode);
#else
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1) {
        return SOCKET_ERROR_VALUE;
    }
    
    flags = nonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    int result = fcntl(socket, F_SETFL, flags);
#endif
    
    return result;
}

    int Select(SocketType maxSocket, fd_set* readSet, fd_set* writeSet, fd_set* exceptSet, int timeoutSec) {
        struct timeval timeout;
        timeout.tv_sec = timeoutSec;
        timeout.tv_usec = 0;

        return select(maxSocket + 1, readSet, writeSet, exceptSet, &timeout);
    }

    int Send(SocketType socket, const char* buffer, int len) {
        return send(socket, buffer, len, 0);
    }

    int Receive(SocketType socket, char* buffer, int len) {
#ifdef _WIN32
        return recv(socket, buffer, len, 0);
#else
        return recv(socket, buffer, len, 0);
#endif
    }

    bool IsValidSocket(SocketType socket) {
        return socket != INVALID_SOCKET_VALUE;
    }

    std::string GetErrorMessage() {
#ifdef _WIN32
        int error = GET_LAST_ERROR;
        wchar_t* errorMessageW = nullptr;
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL, (DWORD)error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&errorMessageW, 0, NULL);
        
        if (!errorMessageW) return "Unknown error (Error code: " + std::to_string(error) + ")";
        
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, errorMessageW, -1, NULL, 0, NULL, NULL);
        if (utf8Len <= 0) { LocalFree(errorMessageW); return "Unknown error (Error code: " + std::to_string(error) + ")"; }
        std::string result(static_cast<size_t>(utf8Len), '\0');
        WideCharToMultiByte(CP_UTF8, 0, errorMessageW, -1, &result[0], utf8Len, NULL, NULL);
        LocalFree(errorMessageW);
        if (!result.empty() && result.back() == '\0') result.pop_back();
        return result + " (Error code: " + std::to_string(error) + ")";
#else
        int error = GET_LAST_ERROR;
        return std::string(strerror(error)) + " (Error code: " + std::to_string(error) + ")";
#endif
    }
}