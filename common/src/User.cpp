#include"User.h"
#include<string>

// 构造函数
User::User(std::string id,const std::string& nk):ID(id),nickname(nk),isOnline(false){}

// 获取三个信息
std::string User::getID() const{return ID;}
std::string User::getNickname() const{return nickname;}
bool User::getOnline() const{return isOnline;}

// 登录？
void User::login(){isOnline=true;}

// 登出？
void User::logout(){isOnline=false;}

