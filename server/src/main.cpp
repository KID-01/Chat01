// main.cpp - 聊天服务器主程序

#include "../include/ChatServer.h"
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <conio.h>  // 用于 Windows 下非阻塞检测输入
    #define STDIN_FILENO 0
#else
    #include <sys/select.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #include <errno.h>
#endif

// 全局变量，用于信号处理
std::atomic<bool> g_running{true};
ChatServer* g_chatServer = nullptr;

// 信号处理函数
void signalHandler(int signal)
{
    std::cout << "\n收到信号 " << signal << "，正在关闭服务器..." << std::endl;
    g_running = false;
    
    if (g_chatServer)
    {
        g_chatServer->stop();
    }
}

// 设置信号处理（【修复】跨平台兼容性）
void setupSignalHandlers()
{
#ifdef _WIN32
    // Windows 下使用简单的 signal 函数
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
#else
    // POSIX 环境使用更可靠的 sigaction
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;   
    
    sigaction(SIGINT, &sa, nullptr);   // Ctrl+C
    sigaction(SIGTERM, &sa, nullptr);  // 终止信号
    sigaction(SIGQUIT, &sa, nullptr);  // 退出信号
#endif
}

// 显示使用说明
void showUsage(const std::string& programName)
{
    std::cout << "使用方法: " << programName << " [选项]" << std::endl;
    std::cout << "选项:" << std::endl;
    std::cout << "  -p, --port <端口号>    设置服务器端口（默认: 8080）" << std::endl;
    std::cout << "  -n, --name <服务器名>  设置服务器名称（默认: 聊天服务器）" << std::endl;
    std::cout << "  -h, --help             显示此帮助信息" << std::endl;
}

// 解析命令行参数
bool parseArguments(int argc, char* argv[], int& port, std::string& serverName)
{
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            showUsage(argv[0]);
            return false;
        }
        else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) port = std::stoi(argv[++i]);
        }
        else if (arg == "-n" || arg == "--name") {
            if (i + 1 < argc) serverName = argv[++i];
        }
    }
    return true;
}

// 主函数
int main(int argc, char* argv[])
{
    int port = 8080;
    std::string serverName = "Utopia";
    
    if (!parseArguments(argc, argv, port, serverName)) return 1;
    
    std::cout << "=== Chat01 服务器 v0.1 ===" << std::endl;
    std::cout << "服务器名称: " << serverName << " | 监听端口: " << port << std::endl;
    std::cout << "==========================" << std::endl;
    
    ChatServer chatServer(serverName, port);
    g_chatServer = &chatServer;
    
    setupSignalHandlers();
    
    if (!chatServer.start()) {
        std::cout << "错误：服务器启动失败" << std::endl;
        return 1;
    }

    std::cout << "服务器启动成功！输入 'quit' 停止服务器。" << std::endl;
    
    std::string input;
    bool firstPrompt = true;
    
    while (g_running)
    {
        if (firstPrompt) {
            std::cout << "服务器> ";
            std::cout.flush();
            firstPrompt = false;
        }

        bool hasInput = false;

#ifdef _WIN32
        // 【关键修复】Windows select 不支持控制台句柄，使用 _kbhit 检测
        if (_kbhit()) {
            hasInput = true;
        } else {
            Sleep(100); // 避免 CPU 空转
        }
#else
        // POSIX 环境继续使用 select
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        int selectResult = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout);
        if (selectResult > 0) hasInput = true;
#endif

        if (hasInput)
        {
            if (!std::getline(std::cin, input)) break;
            
            if (input == "quit" || input == "exit") {
                g_running = false;
            }
            else if (input == "status" || input == "info") {
                std::cout << chatServer.getServerInfoFast() << std::endl;
            }
            else if (input == "users") {
                auto users = chatServer.getConnectedUsersFast();
                std::cout << "在线用户 (" << users.size() << "):" << std::endl;
                for (const auto& user : users) std::cout << "  - " << user << std::endl;
            }
            
            if (g_running) {
                std::cout << "服务器> ";
                std::cout.flush();
            }
        }
    }
    
    std::cout << "正在停止服务器..." << std::endl;
    chatServer.stop();
    return 0;
}