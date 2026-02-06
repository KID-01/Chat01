// main.cpp - 聊天服务器主程序

#include "../include/ChatServer.h"
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <sys/select.h>
#include <unistd.h>
#include <cstring>

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

// 设置信号处理（使用sigaction确保可靠性）
void setupSignalHandlers()
{
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  // 被信号中断的系统调用自动重启
    
    sigaction(SIGINT, &sa, nullptr);   // Ctrl+C
    sigaction(SIGTERM, &sa, nullptr);  // 终止信号
    sigaction(SIGQUIT, &sa, nullptr);  // 退出信号
}

// 显示使用说明
void showUsage(const std::string& programName)
{
    std::cout << "使用方法: " << programName << " [选项]" << std::endl;
    std::cout << "选项:" << std::endl;
    std::cout << "  -p, --port <端口号>    设置服务器端口（默认: 8080）" << std::endl;
    std::cout << "  -n, --name <服务器名>  设置服务器名称（默认: 聊天服务器）" << std::endl;
    std::cout << "  -h, --help             显示此帮助信息" << std::endl;
    std::cout << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  " << programName << " -p 8888 -n \"我的聊天室\"" << std::endl;
    std::cout << "  " << programName << " --port 9090" << std::endl;
}

// 解析命令行参数
bool parseArguments(int argc, char* argv[], int& port, std::string& serverName)
{
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help")
        {
            showUsage(argv[0]);
            return false;
        }
        else if (arg == "-p" || arg == "--port")
        {
            if (i + 1 < argc)
            {
                try
                {
                    port = std::stoi(argv[++i]);
                    if (port < 1 || port > 65535)
                    {
                        std::cout << "错误：端口号必须在1-65535之间" << std::endl;
                        return false;
                    }
                }
                catch (const std::exception& e)
                {
                    std::cout << "错误：无效的端口号: " << argv[i] << std::endl;
                    return false;
                }
            }
            else
            {
                std::cout << "错误：需要指定端口号" << std::endl;
                return false;
            }
        }
        else if (arg == "-n" || arg == "--name")
        {
            if (i + 1 < argc)
            {
                serverName = argv[++i];
            }
            else
            {
                std::cout << "错误：需要指定服务器名称" << std::endl;
                return false;
            }
        }
        else
        {
            std::cout << "错误：未知参数: " << arg << std::endl;
            showUsage(argv[0]);
            return false;
        }
    }
    
    return true;
}

// 主函数
int main(int argc, char* argv[])
{
    // 默认配置
    int port = 8080;
    std::string serverName = "Utopia";
    
    // 解析命令行参数
    if (!parseArguments(argc, argv, port, serverName))
    {
        return 1;
    }
    
    std::cout << "=== Chat01 服务器 v0.1 ===" << std::endl;
    std::cout << "服务器名称: " << serverName << std::endl;
    std::cout << "监听端口: " << port << std::endl;
    std::cout << "最大在线用户数: 100" << std::endl;
    std::cout << "==========================" << std::endl;
    
    // 创建聊天服务器
    ChatServer chatServer(serverName, port);
    g_chatServer = &chatServer;
    
    // 设置信号处理
    setupSignalHandlers();
    
    // 启动服务器
    if (!chatServer.start())
    {
        std::cout << "错误：服务器启动失败" << std::endl;
        return 1;
    }
    
    std::cout << "服务器启动成功！" << std::endl;
    std::cout << "输入 'quit' 或 'exit' 停止服务器" << std::endl;
    std::cout << "按 Ctrl+C 也可以停止服务器" << std::endl;
    std::cout << "注意：服务器日志将输出到控制台，可能会与输入提示符交错" << std::endl;
    std::cout << std::endl;
    
    // 主循环：处理控制台输入（改进的非阻塞方式）
    std::string input;
    bool firstPrompt = true;
    
    while (g_running)
    {
        if (firstPrompt)
        {
            std::cout << "服务器> ";
            firstPrompt = false;
        }
        
        // 使用非阻塞方式读取输入
        fd_set readfds;
        struct timeval timeout;
        
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);  // 监控标准输入
        
        timeout.tv_sec = 0;   // 不等待
        timeout.tv_usec = 100000;  // 100ms超时，定期检查退出条件
        
        int selectResult = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (selectResult > 0 && FD_ISSET(STDIN_FILENO, &readfds))
        {
            // 有输入数据可读
            std::cout << "\r[输入检测] 检测到输入数据" << std::endl;
            
            if (!std::getline(std::cin, input)) 
            {
                // 输入流结束（如Ctrl+D）
                std::cout << "输入流结束，退出程序" << std::endl;
                g_running = false;
                break;
            }
            
            std::cout << "[输入处理] 收到命令: " << input << std::endl;
            
            try {
                if (input == "quit" || input == "exit")
                {
                    g_running = false;  // 设置全局退出标志
                    break;
                }
                else if (input == "status" || input == "info")
                {
                    std::cout << chatServer.getServerInfoFast() << std::endl;
                    std::cout << "服务器> ";
                }
                else if (input == "users")
                {
                    auto users = chatServer.getConnectedUsersFast();
                    if (users.empty()) {
                        std::cout << "暂时无法获取用户列表（系统繁忙）" << std::endl;
                    } else {
                        std::cout << "在线用户 (" << users.size() << " 人):" << std::endl;
                        for (const auto& user : users)
                        {
                            std::cout << "  - " << user << std::endl;
                        }
                    }
                    std::cout << "服务器> ";
                }
                else if (input.empty())
                {
                    // 空输入，显示新的提示符
                    std::cout << "服务器> ";
                }
                else
                {
                    std::cout << "未知命令。可用命令: quit, exit, status, info, users" << std::endl;
                    std::cout << "服务器> ";
                }
                
                // 刷新输出缓冲区，确保提示符立即显示
                std::cout.flush();
            }
            catch (const std::exception& e) {
                std::cout << "命令处理错误: " << e.what() << std::endl;
                std::cout << "服务器> ";
                std::cout.flush();
            }
        }
        else if (selectResult == 0)
        {
            // 超时，没有输入，继续循环
            // 这里可以添加其他需要定期执行的任务
        }
        else
        {
            // select错误（包括EINTR信号中断）
            if (errno == EINTR)
            {
                // 信号中断，检查是否需要退出
                if (!g_running)
                {
                    break;
                }
            }
            else
            {
                std::cout << "输入监控错误: " << strerror(errno) << std::endl;
                std::cout << "服务器> ";
            }
        }
    }
    
    // 停止服务器
    std::cout << "正在停止服务器..." << std::endl;
    chatServer.stop();
    g_chatServer = nullptr;
    
    std::cout << "服务器已停止。再见！" << std::endl;
    return 0;
}