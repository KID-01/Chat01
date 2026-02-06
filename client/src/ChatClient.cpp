// ChatClient.cpp - 聊天客户端主程序

#include "../include/ClientNetworkManager.h"
#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include <csignal>

// 全局变量用于优雅退出
static volatile sig_atomic_t g_should_exit = 0;

// 信号处理函数
void signalHandler(int signal) 
{
    g_should_exit = 1;
    std::cout << "\n接收到退出信号，正在退出..." << std::endl;
}

int main() 
{
    std::cout << "=== Chat01 客户端 ===" << std::endl;
    
    // 注册信号处理器，用于优雅退出
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // 创建网络管理器
    auto networkManager = std::unique_ptr<ClientNetworkManager>(new ClientNetworkManager());
    
    // 设置回调函数
    networkManager->setOnConnected([]() {
        std::cout << "\n连接服务器成功！可以开始聊天了" << std::endl;
        std::cout << "输入消息并按回车发送，输入 'quit' 退出" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
    });
    
    networkManager->setOnDisconnected([]() {
        std::cout << "\n与服务器的连接已断开" << std::endl;
    });
    
    networkManager->setOnMessageReceived([](const std::string& sender, const std::string& content) {
        std::cout << "[来自 " << sender << "]: " << content << std::endl;
    });
    
    networkManager->setOnError([](const std::string& error) {
        std::cout << "\n错误: " << error << std::endl;
    });
    
    // 获取用户输入
    std::string serverAddress, username;
    
    std::cout << "请输入服务器地址 (默认: localhost): ";
    std::getline(std::cin, serverAddress);
    if (serverAddress.empty()) 
    {
        serverAddress = "localhost";
    }
    
    std::cout << "请输入用户名: ";
    std::getline(std::cin, username);
    if (username.empty()) 
    {
        username = "Anonymous";
    }

    // 连接到服务器
    std::cout << "正在连接到服务器..." << std::endl;
    if (!networkManager->connectToServer(serverAddress, 8080, username)) 
    {
        std::cerr << "无法连接到服务器，程序退出" << std::endl;
        return 1;
    }

    // 主循环 - 处理用户输入
    std::string input;
    while (!g_should_exit && networkManager->isConnected()) 
    {
        std::cout << "> ";
        
        // 使用非阻塞方式读取输入，避免阻塞在getline上
	if (!std::getline(std::cin, input)) 
	{
            // 输入流结束（如Ctrl+D）
            break;
        }

        if (input.empty())continue;

        if (input == "quit" || input == "exit" || input == "/quit" || input == "/exit") 
	{
            std::cout << "正在退出..." << std::endl;
            g_should_exit = 1;  // 立即设置退出标志
            break;
        }

	if (input == "/help") 
	{
            std::cout << "可用命令:" << std::endl;
            std::cout << "  quit/exit - 退出聊天" << std::endl;
            std::cout << "  /help - 显示此帮助" << std::endl;
            std::cout << "  /status - 显示连接状态" << std::endl;
            continue;
        }

	if (input == "/status") 
	{
            if (networkManager->isConnected()) 
	    {
                std::cout << "已连接到服务器" << std::endl;
            } 
	    else 
	    {
                std::cout << "未连接到服务器" << std::endl;
            }
            continue;
        }

        // 发送消息
	if (!networkManager->sendMessage(input)) 
	{
            std::cout << "发送消息失败!" << std::endl;
        }
    }

    // 通知退出
    g_should_exit = 1;
    
    // 断开连接
    networkManager->disconnect();
    
    std::cout << "\n再见喵(´-ω-`)ﾉ" << std::endl;

    return 0;
}