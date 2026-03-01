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
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);

        // 处理localhost特殊地址
        std::string resolvedAddress = address;
        if (address == "localhost") {
            resolvedAddress = "127.0.0.1";
        }

        int result = inet_pton(AF_INET, resolvedAddress.c_str(), &serverAddr.sin_addr);
        if (result <= 0) {
            std::cerr << "Invalid server address: " << address << std::endl;
            return SOCKET_ERROR_VALUE;
        }

        return ::connect(socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
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
        char* errorMessage;
        FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (char*)&errorMessage, 0, NULL);
        
        std::string result(errorMessage);
        LocalFree(errorMessage);
        return result;
#else
        return std::string(strerror(GET_LAST_ERROR));
#endif
    }
}