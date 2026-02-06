// NetworkProtocol.h - 网络协议定义

#ifndef NETWORKPROTOCOL_H
#define NETWORKPROTOCOL_H

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <chrono>

namespace Chat01
{
	// 消息类型枚举
	enum class MessageType
	{
		TEXT_MESSAGE=1,		// 普通文本消息
		LOGIN_REQUEST,		// 登录请求
		LOGOUT_RESPONSE,	// 登录响应
		USER_JOIN,		// 用户加入通知
		USER_LEAVE,		// 用户离开通知
		ERROR_MESSAGE		// 错误消息
	};

	// 基础消息结构
	struct NetworkMessage
	{
		MessageType type;// 消息类型
		std::string sender;// 发送者
		std::string content;// 消息内容
		long timestamp;// 时间戳

		// 构造函数，方便创建消息
		NetworkMessage(MessageType t,const std::string& s,const std::string& c)
			:type(t),sender(s),content(c)
		{
			timestamp=std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()
			).count();
		}
	};

	// 消息序列化和反序列化工具
	// 这个函数在C++16的第四个问答有讲解
	class MessageSerializer
	{
	public:
		// 将消息对象序列化为字符串
		static std::string serialize(const NetworkMessage& msg)
		{
			// 简单格式：类型|发送者|时间戳|内容
			// 注意：内容中不能包含"|"字符（暂时）
			// 为了减少TCP粘包/半包问题，在每条序列化消息末尾添加换行符作为分隔符
			return std::to_string(static_cast<int>(msg.type))+"|"+
				msg.sender+"|"+
				std::to_string(msg.timestamp)+"|"+
				msg.content + "\n";
		}

		// 将字符串反序列化为消息对象
		static NetworkMessage deserialize(const std::string& data)
		{
			std::vector<std::string> parts;
			std::string part;
			std::istringstream stream(data);

			// 用"|"分割字符串
			while(std::getline(stream,part,'|')){parts.push_back(part);}

			if(parts.size()<4)
			{
				// 格式错误，返回错误消息
				return NetworkMessage(MessageType::ERROR_MESSAGE,"SYSTEM","消息格式错误");
			}

			// 解析各部分
			MessageType type=static_cast<MessageType>(std::stoi(parts[0]));
			std::string sender=parts[1];
			long timestamp=std::stol(parts[2]);

			// 内容可能是多段的（如果有|被转义的话）
			std::string content;
			for(size_t i=3;i<parts.size();i++)
			{
				if(i>3)content+="|";// 恢复被分割的"|"
				content+=parts[i];
			}

			NetworkMessage msg(type,sender,content);
			msg.timestamp=timestamp;
			return msg;
		}

		// 检查是否是有效的消息格式
		static bool isValidFormat(const std::string& data)
		{
			// 至少需要3个|分隔符
			int count=std::count(data.begin(),data.end(),'|');
			return count>=3;
		}
	};
}

#endif