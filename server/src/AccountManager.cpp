// AccountManager.cpp - 账号管理器实现

#include "../include/AccountManager.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <errno.h>
#endif

namespace Chat01
{
    // 构造函数
    AccountManager::AccountManager(const std::string& dataFilePath)
        : m_dataFilePath(dataFilePath), m_nextUserID(1), m_totalAccounts(0)
    {
        std::cout << "[AccountManager] 初始化，数据文件: " << m_dataFilePath << std::endl;
        
        // 尝试加载现有账号数据
        if (!loadAccounts())
        {
            std::cout << "[AccountManager] 无法加载账号数据，将使用空数据库" << std::endl;
            // 初始化空数据库
            m_nextUserID = 1;
            m_totalAccounts = 0;
        }
        else
        {
            std::cout << "[AccountManager] 成功加载 " << m_totalAccounts << " 个账号" << std::endl;
        }
    }

    // 析构函数
    AccountManager::~AccountManager()
    {
        // 保存账号数据
        if (!saveAccounts())
        {
            std::cerr << "[AccountManager] 警告：保存账号数据失败" << std::endl;
        }
        std::cout << "[AccountManager] 销毁" << std::endl;
    }

    // 处理用户登录
    std::string AccountManager::handleUserLogin(const std::string& username, bool& isNewUser)
    {
        // 去除用户名前后空格
        std::string trimmedUsername = username;
        trimmedUsername.erase(0, trimmedUsername.find_first_not_of(" \t\n\r\f\v"));
        trimmedUsername.erase(trimmedUsername.find_last_not_of(" \t\n\r\f\v") + 1);
        
        if (!isValidUsername(trimmedUsername))
        {
            std::cerr << "[AccountManager] 错误：无效的用户名: " << trimmedUsername << std::endl;
            return "";
        }

        AccountInfo accountInfo;
        isNewUser = !findAccount(trimmedUsername, accountInfo);

        if (isNewUser)
        {
            // 新用户，创建账号
            std::string userID;
            if (createAccount(trimmedUsername, userID))
            {
                std::cout << "[AccountManager] 新用户注册: " << trimmedUsername << " (ID: " << userID << ")" << std::endl;
                return userID;
            }
            else
            {
                std::cerr << "[AccountManager] 创建账号失败: " << trimmedUsername << std::endl;
                return "";
            }
        }
        else
        {
            // 老用户，更新登录信息
            if (updateAccountLogin(trimmedUsername))
            {
                std::cout << "[AccountManager] 老用户登录: " << trimmedUsername << " (ID: " << accountInfo.userID << ")" << std::endl;
                return accountInfo.userID;
            }
            else
            {
                std::cerr << "[AccountManager] 更新登录信息失败: " << trimmedUsername << std::endl;
                return accountInfo.userID; // 仍然返回用户ID，但记录错误
            }
        }
    }

    // 创建账号
    bool AccountManager::createAccount(const std::string& username, std::string& userID)
    {
        // 去除用户名前后空格
        std::string trimmedUsername = username;
        trimmedUsername.erase(0, trimmedUsername.find_first_not_of(" \t\n\r\f\v"));
        trimmedUsername.erase(trimmedUsername.find_last_not_of(" \t\n\r\f\v") + 1);
        
        if (!isValidUsername(trimmedUsername))
        {
            return false;
        }

        // 检查用户名是否已存在（用户名唯一性）
        if (m_accounts.find(trimmedUsername) != m_accounts.end())
        {
            std::cerr << "[AccountManager] 用户名已存在: " << trimmedUsername << std::endl;
            return false;
        }

        // 生成用户ID
        userID = generateUserID();
        
        // 创建账号信息
        AccountInfo newAccount;
        newAccount.userID = userID;
        newAccount.username = trimmedUsername;
        newAccount.createdAt = getCurrentTime();
        newAccount.lastLogin = getCurrentTime();
        newAccount.loginCount = 1;

        // 添加到账号列表（使用用户名作为键，确保唯一性）
        m_accounts[trimmedUsername] = newAccount;
        m_totalAccounts++;
        m_nextUserID++;

        // 保存数据
        if (!saveAccounts())
        {
            std::cerr << "[AccountManager] 警告：创建账号后保存数据失败" << std::endl;
            // 回滚更改
            m_accounts.erase(trimmedUsername);
            m_totalAccounts--;
            m_nextUserID--;
            return false;
        }

        return true;
    }

    // 查找账号
    bool AccountManager::findAccount(const std::string& username, AccountInfo& accountInfo)
    {
        auto it = m_accounts.find(username);
        if (it != m_accounts.end())
        {
            accountInfo = it->second;
            return true;
        }
        return false;
    }

    // 更新账号登录信息
    bool AccountManager::updateAccountLogin(const std::string& username)
    {
        auto it = m_accounts.find(username);
        if (it == m_accounts.end())
        {
            return false;
        }

        // 更新登录信息
        it->second.lastLogin = getCurrentTime();
        it->second.loginCount++;

        // 保存数据
        if (!saveAccounts())
        {
            std::cerr << "[AccountManager] 警告：更新登录信息后保存数据失败" << std::endl;
            return false;
        }

        return true;
    }

    // 加载账号数据
    bool AccountManager::loadAccounts()
    {
        // 确保目录存在
        if (!ensureDirectoryExists(m_dataFilePath)) {
            std::cerr << "[AccountManager] 无法确保目录存在: " << m_dataFilePath << std::endl;
            return false;
        }

        std::ifstream file(m_dataFilePath);
        if (!file.is_open())
        {
            std::cout << "[AccountManager] 数据文件不存在，将创建新文件: " << m_dataFilePath << std::endl;
            return false;
        }

        try
        {
            // 简单的JSON解析（实际项目中应该使用JSON库）
            std::string line;
            std::string jsonContent;
            while (std::getline(file, line))
            {
                jsonContent += line;
            }
            file.close();

            // 简化处理：这里只是演示，实际应该使用JSON解析库
            // 由于时间关系，我们使用一个简化的解析逻辑
            
            // 清空现有数据
            m_accounts.clear();
            
            // 查找accounts数组
            size_t accountsPos = jsonContent.find("\"accounts\"");
            if (accountsPos == std::string::npos)
            {
                std::cerr << "[AccountManager] 数据文件格式错误：找不到accounts字段" << std::endl;
                return false;
            }

            // 解析accounts数组中的账号信息
            size_t arrayStart = jsonContent.find('[', accountsPos);
            size_t arrayEnd = jsonContent.find(']', arrayStart);
            
            if (arrayStart == std::string::npos || arrayEnd == std::string::npos)
            {
                std::cerr << "[AccountManager] 数据文件格式错误：accounts数组格式不正确" << std::endl;
                return false;
            }
            
            std::string accountsArray = jsonContent.substr(arrayStart, arrayEnd - arrayStart + 1);
            
            // 解析每个账号对象
            size_t objStart = accountsArray.find('{');
            while (objStart != std::string::npos)
            {
                size_t objEnd = accountsArray.find('}', objStart);
                if (objEnd == std::string::npos) break;
                
                std::string accountObj = accountsArray.substr(objStart, objEnd - objStart + 1);
                
                // 解析账号字段
                AccountInfo account;
                
                // 解析userID
                size_t userIDPos = accountObj.find("\"userID\"");
                if (userIDPos != std::string::npos)
                {
                    size_t colonPos = accountObj.find(':', userIDPos);
                    size_t commaPos = accountObj.find(',', colonPos);
                    if (commaPos == std::string::npos) commaPos = accountObj.find('}', colonPos);
                    
                    std::string userIDStr = accountObj.substr(colonPos + 1, commaPos - colonPos - 1);
                    // 去除引号
                    userIDStr.erase(std::remove(userIDStr.begin(), userIDStr.end(), '"'), userIDStr.end());
                    account.userID = userIDStr;
                }
                
                // 解析username
                size_t usernamePos = accountObj.find("\"username\"");
                if (usernamePos != std::string::npos)
                {
                    size_t colonPos = accountObj.find(':', usernamePos);
                    size_t commaPos = accountObj.find(',', colonPos);
                    if (commaPos == std::string::npos) commaPos = accountObj.find('}', colonPos);
                    
                    std::string usernameStr = accountObj.substr(colonPos + 1, commaPos - colonPos - 1);
                    // 去除引号
                    usernameStr.erase(std::remove(usernameStr.begin(), usernameStr.end(), '"'), usernameStr.end());
                    // 去除前后空格
                    usernameStr.erase(0, usernameStr.find_first_not_of(" \t\n\r\f\v"));
                    usernameStr.erase(usernameStr.find_last_not_of(" \t\n\r\f\v") + 1);
                    account.username = usernameStr;
                }
                
                // 解析createdAt
                size_t createdAtPos = accountObj.find("\"createdAt\"");
                if (createdAtPos != std::string::npos)
                {
                    size_t colonPos = accountObj.find(':', createdAtPos);
                    size_t commaPos = accountObj.find(',', colonPos);
                    if (commaPos == std::string::npos) commaPos = accountObj.find('}', colonPos);
                    
                    std::string createdAtStr = accountObj.substr(colonPos + 1, commaPos - colonPos - 1);
                    // 去除引号
                    createdAtStr.erase(std::remove(createdAtStr.begin(), createdAtStr.end(), '"'), createdAtStr.end());
                    account.createdAt = createdAtStr;
                }
                
                // 解析lastLogin
                size_t lastLoginPos = accountObj.find("\"lastLogin\"");
                if (lastLoginPos != std::string::npos)
                {
                    size_t colonPos = accountObj.find(':', lastLoginPos);
                    size_t commaPos = accountObj.find(',', colonPos);
                    if (commaPos == std::string::npos) commaPos = accountObj.find('}', colonPos);
                    
                    std::string lastLoginStr = accountObj.substr(colonPos + 1, commaPos - colonPos - 1);
                    // 去除引号
                    lastLoginStr.erase(std::remove(lastLoginStr.begin(), lastLoginStr.end(), '"'), lastLoginStr.end());
                    account.lastLogin = lastLoginStr;
                }
                
                // 解析loginCount
                size_t loginCountPos = accountObj.find("\"loginCount\"");
                if (loginCountPos != std::string::npos)
                {
                    size_t colonPos = accountObj.find(':', loginCountPos);
                    size_t commaPos = accountObj.find(',', colonPos);
                    if (commaPos == std::string::npos) commaPos = accountObj.find('}', colonPos);
                    
                    std::string loginCountStr = accountObj.substr(colonPos + 1, commaPos - colonPos - 1);
                    try {
                        account.loginCount = std::stoi(loginCountStr);
                    } catch (const std::exception& e) {
                        std::cerr << "[AccountManager] 解析loginCount时发生错误: " << e.what() << std::endl;
                        account.loginCount = 0;
                    }
                }
                
                // 将账号添加到映射中
                if (!account.username.empty() && !account.userID.empty())
                {
                    m_accounts[account.username] = account;
                }
                
                // 查找下一个账号对象
                objStart = accountsArray.find('{', objEnd);
            }

            // 查找nextUserID
            size_t nextIDPos = jsonContent.find("\"nextUserID\"");
            if (nextIDPos != std::string::npos)
            {
                size_t colonPos = jsonContent.find(':', nextIDPos);
                size_t commaPos = jsonContent.find(',', colonPos);
                if (commaPos == std::string::npos) commaPos = jsonContent.find('}', colonPos);
                
                std::string nextIDStr = jsonContent.substr(colonPos + 1, commaPos - colonPos - 1);
                try {
                    m_nextUserID = std::stoi(nextIDStr);
                } catch (const std::exception& e) {
                    std::cerr << "[AccountManager] 解析nextUserID时发生错误: " << e.what() << std::endl;
                    m_nextUserID = m_accounts.size() + 1;
                }
            }

            // totalAccounts应该等于实际加载的账号数量
            m_totalAccounts = m_accounts.size();

            std::cout << "[AccountManager] 加载数据成功: 加载了 " << m_accounts.size() 
                      << " 个账号, nextUserID=" << m_nextUserID 
                      << ", totalAccounts=" << m_totalAccounts << std::endl;
            return true;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[AccountManager] 加载数据时发生错误: " << e.what() << std::endl;
            return false;
        }
    }

    // 确保目录存在
    bool AccountManager::ensureDirectoryExists(const std::string& filePath)
    {
        // 查找最后一个目录分隔符
        size_t lastSep = filePath.find_last_of("/\\");
        if (lastSep == std::string::npos) {
            // 没有目录分隔符，不需要创建目录
            return true;
        }
        
        // 提取目录路径
        std::string directory = filePath.substr(0, lastSep);
        
        // 在Windows平台上创建目录
#ifdef _WIN32
        if (!CreateDirectoryA(directory.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
            std::cerr << "[AccountManager] 无法创建目录: " << directory << ", 错误代码: " << GetLastError() << std::endl;
            return false;
        }
#else
        // 在POSIX平台上创建目录
        struct stat st = {0};
        if (stat(directory.c_str(), &st) == -1) {
            if (mkdir(directory.c_str(), 0755) != 0) {
                std::cerr << "[AccountManager] 无法创建目录: " << directory << ", 错误: " << strerror(errno) << std::endl;
                return false;
            }
        }
#endif
        
        return true;
    }

    // 保存账号数据
    bool AccountManager::saveAccounts()
    {
        // 确保目录存在
        if (!ensureDirectoryExists(m_dataFilePath)) {
            std::cerr << "[AccountManager] 无法确保目录存在: " << m_dataFilePath << std::endl;
            return false;
        }

        std::ofstream file(m_dataFilePath);
        if (!file.is_open())
        {
            std::cerr << "[AccountManager] 无法打开数据文件: " << m_dataFilePath << std::endl;
            return false;
        }

        try
        {
            file << "{\n";
            file << "    \"accounts\": [\n";
            
            bool first = true;
            for (const auto& pair : m_accounts)
            {
                if (!first) file << ",\n";
                first = false;
                
                const AccountInfo& account = pair.second;
                file << "        {\n";
                file << "            \"userID\": \"" << account.userID << "\",\n";
                file << "            \"username\": \"" << account.username << "\",\n";
                file << "            \"createdAt\": \"" << account.createdAt << "\",\n";
                file << "            \"lastLogin\": \"" << account.lastLogin << "\",\n";
                file << "            \"loginCount\": " << account.loginCount << "\n";
                file << "        }";
            }
            
            file << "\n    ],\n";
            file << "    \"nextUserID\": " << m_nextUserID << ",\n";
            file << "    \"totalAccounts\": " << m_totalAccounts << ",\n";
            file << "    \"lastUpdate\": \"" << getCurrentTime() << "\"\n";
            file << "}";
            
            file.close();
            return true;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[AccountManager] 保存数据时发生错误: " << e.what() << std::endl;
            return false;
        }
    }

    // 获取总账号数
    int AccountManager::getTotalAccounts() const
    {
        return m_totalAccounts;
    }

    // 获取下一个用户ID
    int AccountManager::getNextUserID() const
    {
        return m_nextUserID;
    }

    // 生成用户ID
    std::string AccountManager::generateUserID()
    {
        if (m_nextUserID <= 999)
        {
            char idStr[4];
            snprintf(idStr, sizeof(idStr), "%03d", m_nextUserID);
            return std::string(idStr);
        }
        else if (m_nextUserID <= 9999)
        {
            char idStr[5];
            snprintf(idStr, sizeof(idStr), "%04d", m_nextUserID);
            return std::string(idStr);
        }
        else
        {
            return std::to_string(m_nextUserID);
        }
    }

    // 获取当前时间
    std::string AccountManager::getCurrentTime()
    {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    // 验证用户名有效性
    bool AccountManager::isValidUsername(const std::string& username) const
    {
        if (username.empty())
        {
            return false;
        }
        
        // 检查是否只包含字母、数字、下划线和中文
        for (char c : username)
        {
            if (!std::isalnum(c) && c != '_' && !(c & 0x80)) // 简单的中文检测
            {
                return false;
            }
        }
        
        return true;
    }
}