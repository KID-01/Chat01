#ifndef USER_H
#define USER_H

#include<string>
#include<memory>


class User{
public:
	// 构造函数
	User(std::string Id,const std::string& Nickname);
    
	// 获取ID
	std::string getID() const;
    
	// 获取用户名
	std::string getNickname() const;
    
	// 上线？
	bool getOnline() const;
    
	// 登录？
	void login();
    
	// 登出？
	void logout();

private:
    std::string ID;
    std::string nickname;
    bool isOnline;
};

#endif

