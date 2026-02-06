#include"Message.h"
#include<string>
#include<chrono>
#include<sstream>
#include<iomanip>
#include<memory>
#include"User.h"

// 构造函数
Message::Message(std::shared_ptr<User> sender,const std::string& content)
	:m_sender(sender),m_content(content),
	m_sendTime(std::chrono::system_clock::now()){}

// 获取发送内容
std::string Message::getContent() const{return m_content;}

// 获取发送人
std::shared_ptr<User> Message::getSender() const{return m_sender;}

// 获取发送时间，现在的东西没体现此功能
std::string Message::getSendTime() const
{
	std::time_t time=std::chrono::system_clock::to_time_t(m_sendTime);// 将time_point转换为C风格的time_t
	std::tm* local_time=std::localtime(&time);
	std::stringstream ss;
	ss<<std::put_time(local_time,"%Y-%m-%d  %H:%M");
	return ss.str();
}

