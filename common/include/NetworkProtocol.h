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
        TEXT_MESSAGE=1,     // 普通文本消息
        LOGIN_REQUEST,      // 登录请求
        LOGOUT_RESPONSE,    // 登出响应
        USER_JOIN,          // 用户加入通知
        USER_LEAVE,         // 用户离开通知
        USER_LIST,          // 用户列表更新
        ERROR_MESSAGE       // 错误消息
    };

    // 网络消息结构体
    struct NetworkMessage
    {
        MessageType type;   // 消息类型
        std::string sender; // 发送者
        std::string content;// 消息内容
        long long timestamp;     // 时间戳

        // 构造函数，自动生成时间戳
        NetworkMessage(MessageType t,const std::string& s,const std::string& c)
            :type(t),sender(s),content(c)
        {
            timestamp=std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
        }
    };

    // 消息序列化与反序列化工具类
    // 这个函数在C++16的第四个问答有讲解
    class MessageSerializer
	{
	public:
		// 将消息对象序列化为字符串
		static std::string serialize(const NetworkMessage& msg)
		{
			// 简单格式：类型|发送者|时间戳|内容
			// 对内容中的"|"进行转义处理，使用"||"表示一个"|"
			std::string escapedContent = msg.content;
			size_t pos = 0;
			while ((pos = escapedContent.find('|', pos)) != std::string::npos) {
				escapedContent.replace(pos, 1, "||");
				pos += 2;
			}

			// 为了减少TCP粘包/半包问题，在每条序列化消息末尾添加换行符作为分隔符
			// 使用\n作为统一的换行符，确保跨平台一致性
			int typeValue;
			switch (msg.type) {
			case MessageType::TEXT_MESSAGE: typeValue = 1; break;
			case MessageType::LOGIN_REQUEST: typeValue = 2; break;
			case MessageType::LOGOUT_RESPONSE: typeValue = 3; break;
			case MessageType::USER_JOIN: typeValue = 4; break;
			case MessageType::USER_LEAVE: typeValue = 5; break;
			case MessageType::USER_LIST: typeValue = 6; break;
			case MessageType::ERROR_MESSAGE: typeValue = 7; break;
			default: typeValue = 7; break;
			}

			return std::to_string(typeValue) + "|" +
				msg.sender + "|" +
				std::to_string(msg.timestamp) + "|" +
				escapedContent + "\n";
		}

		// 将字符串反序列化为消息对象
		static NetworkMessage deserialize(const std::string& data)
		{
			// 移除可能的Windows风格换行符\r\n
			std::string trimmedData = data;
			if (!trimmedData.empty() && trimmedData.back() == '\n') {
				trimmedData.pop_back();
			}
			if (!trimmedData.empty() && trimmedData.back() == '\r') {
				trimmedData.pop_back();
			}

			// 检查消息长度
			if (trimmedData.empty()) {
				return NetworkMessage(MessageType::ERROR_MESSAGE, "SYSTEM", "空消息");
			}

			std::vector<std::string> parts;
			std::string part;
			size_t pos = 0;
			size_t lastPos = 0;

			// 用"|"分割字符串，但要处理转义的"||"
			while ((pos = trimmedData.find('|', lastPos)) != std::string::npos) {
				// 检查是否是转义的"||"
				if (pos + 1 < trimmedData.length() && trimmedData[pos + 1] == '|') {
					// 是转义的"||"，将单个"|"添加到当前部分
					part += trimmedData.substr(lastPos, pos - lastPos + 1);
					lastPos = pos + 2;
				}
				else {
					// 是分隔符，添加当前部分并开始新部分
					part += trimmedData.substr(lastPos, pos - lastPos);
					parts.push_back(part);
					part.clear();
					lastPos = pos + 1;
				}
			}

			// 添加最后一个部分
			part += trimmedData.substr(lastPos);
			parts.push_back(part);

			if (parts.size() < 4)
			{
				// 格式错误，返回错误消息
				return NetworkMessage(MessageType::ERROR_MESSAGE, "SYSTEM", "消息格式错误");
			}

			// 解析消息类型
			MessageType type;
			try {
				// 首先验证类型字符串是否只包含数字
				const std::string& typeStr = parts[0];
				if (!isValidIntegerString(typeStr)) {
					return NetworkMessage(MessageType::ERROR_MESSAGE, "SYSTEM", "消息类型格式错误");
				}

				// 解析类型值
				int typeValue = std::stoi(typeStr);

				// 根据类型值设置消息类型
				switch (typeValue) {
				case 1: type = MessageType::TEXT_MESSAGE; break;
				case 2: type = MessageType::LOGIN_REQUEST; break;
				case 3: type = MessageType::LOGOUT_RESPONSE; break;
				case 4: type = MessageType::USER_JOIN; break;
				case 5: type = MessageType::USER_LEAVE; break;
				case 6: type = MessageType::USER_LIST; break;
				case 7: type = MessageType::ERROR_MESSAGE; break;
				default: return NetworkMessage(MessageType::ERROR_MESSAGE, "SYSTEM", "无效的消息类型");
				}
			}
			catch (const std::exception& e) {
				return NetworkMessage(MessageType::ERROR_MESSAGE, "SYSTEM", "消息类型格式错误");
			}

			// 解析发送者
			std::string sender = parts[1];

			// 解析时间戳
			long timestamp;
			try {
				// 验证时间戳字符串
				const std::string& timestampStr = parts[2];
				if (!isValidIntegerString(timestampStr)) {
					return NetworkMessage(MessageType::ERROR_MESSAGE, "SYSTEM", "时间戳格式错误");
				}

				timestamp = std::stoll(timestampStr);
			}
			catch (const std::exception& e) {
				return NetworkMessage(MessageType::ERROR_MESSAGE, "SYSTEM", "时间戳格式错误");
			}

			// 解析内容（可能是多段的，如果有|被转义的话）
			std::string content;
			for (size_t i = 3; i < parts.size(); i++)
			{
				if (i > 3)content += "|";// 恢复被分割的"|"
				content += parts[i];
			}

			// 恢复转义的"||"为单个"|"
			pos = 0;
			while ((pos = content.find("||", pos)) != std::string::npos) {
				content.replace(pos, 2, "|");
				pos += 1;
			}

			NetworkMessage msg(type, sender, content);
			msg.timestamp = timestamp;

			return msg;
		}

		// 检查是否是有效的消息格式
		static bool isValidFormat(const std::string& data)
		{
			// 检查消息长度
			if (data.empty()) {
				return false;
			}

			// 至少需要3个|分隔符
			int count = 0;
			size_t pos = 0;
			while ((pos = data.find('|', pos)) != std::string::npos)
			{
				// 跳过转义的"||"
				if (pos + 1 < data.length() && data[pos + 1] == '|')
					pos += 2;
				else
				{
					count++;
					pos++;
				}
			}

			return count >= 3;
		}

	private:
		// 验证消息类型值是否有效
		static bool isValidMessageType(int typeValue) {
			// 消息类型范围：1-7
			return typeValue >= 1 && typeValue <= 7;
		}

		// 验证字符串是否是有效的整数
		static bool isValidIntegerString(const std::string& str) {
			if (str.empty()) {
				return false;
			}

			for (char c : str) {
				if (!std::isdigit(c)) {
					return false;
				}
			}

			return true;
		}
	};
}

#endif