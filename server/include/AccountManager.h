// AccountManager.h - 账号管理器

#ifndef ACCOUNTMANAGER_H
#define ACCOUNTMANAGER_H

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace Chat01
{
    // 账号信息结构
    struct AccountInfo
    {
        std::string userID;
        std::string username;
        std::string createdAt;
        std::string lastLogin;
        int loginCount;
    };

    class AccountManager
    {
    public:
        // 构造函数和析构函数
        AccountManager(const std::string& dataFilePath);
        ~AccountManager();

        // 用户登录处理
        std::string handleUserLogin(const std::string& username, bool& isNewUser);

        // 账号管理
        bool createAccount(const std::string& username, std::string& userID);
        bool findAccount(const std::string& username, AccountInfo& accountInfo);
        bool updateAccountLogin(const std::string& username);

        // 数据持久化
        bool loadAccounts();
        bool saveAccounts();

        // 统计信息
        int getTotalAccounts() const;
        int getNextUserID() const;

    private:
        std::string m_dataFilePath;
        std::map<std::string, AccountInfo> m_accounts;  // username -> AccountInfo
        int m_nextUserID;
        int m_totalAccounts;

        // 内部工具方法
        std::string generateUserID();
        std::string getCurrentTime();
        bool isValidUsername(const std::string& username) const;
        bool ensureDirectoryExists(const std::string& filePath);
    };
}

#endif