#ifndef MESSAGE_H
#define MESSAGE_H

#include<string>
#include<chrono>
#include<memory>

class User;

class Message{
private:
	std::shared_ptr<User> m_sender;// m_sender现在是一个智能指针，它指向一个User对象，并且允许多个地方共享这个对象的所有权
	std::string m_content;// 内容
	std::chrono::system_clock::time_point m_sendTime;// 发送时间
public:
	// 构造函数
    	Message(std::shared_ptr<User> sender,const std::string& content);
	
	// 获取发送内容
	std::string getContent() const;
    	
	// 获取发送人
	std::shared_ptr<User> getSender() const;
	
	// 获取发送时间
	// 时间的处理用的是系统时钟
	std::string getSendTime() const;
    
};


#endif
