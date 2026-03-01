#include "chatclient.h"
#include "../../client/include/ClientNetworkManager.h"
#include <QDebug>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include <iostream>

/**
 * @brief ChatClient 构造函数
 * @param parent 父对象
 */
ChatClient::ChatClient(QObject *parent)
    : QObject(parent)
    , m_connected(false)
    , m_currentUsername("")
{
    qDebug() << "ChatClient: 创建ChatClient实例";
    
    // 创建网络管理器（不使用线程）
    m_networkManager = std::make_unique<ClientNetworkManager>();
}

/**
 * @brief ChatClient 析构函数
 */
ChatClient::~ChatClient()
{
    qDebug() << "ChatClient: 清理ChatClient实例";
    
    // 确保断开连接
    if (m_connected) {
        disconnectFromServer();
    }
    
    // 清理网络管理器
    m_networkManager.reset();
}

/**
 * @brief 连接到聊天服务器
 */
void ChatClient::connectToServer(const QString& address, int port, const QString& username)
{
    qDebug() << "ChatClient: 尝试连接到服务器" << address << ":" << port << "用户名:" << username;
    
    m_lastConnectionError.clear();
    
    if (m_connected) {
        qWarning() << "ChatClient: 已经连接到服务器，请先断开连接";
        emit errorOccurred("已经连接到服务器，请先断开连接");
        return;
    }
    
    m_currentUsername = username;
    
    try {
        std::string serverAddress = address.toStdString();
        std::string user = username.toStdString();
        
        QPointer<ChatClient> self(this);
        
        // 设置回调函数（将C++回调转换为Qt信号）
        m_networkManager->setOnConnected([self]() {
            if (!self) {
                qWarning() << "ChatClient: 连接成功回调被调用，但对象已销毁";
                return;
            }
            qDebug() << "ChatClient: 连接成功回调被调用";
            self->m_connected = true;
            emit self->connectionStatusChanged(true);
            emit self->connected();
            qDebug() << "ChatClient: 连接服务器成功";
        });
        
        m_networkManager->setOnDisconnected([self]() {
            if (!self) {
                qWarning() << "ChatClient: 断开连接回调被调用，但对象已销毁";
                return;
            }
            qDebug() << "ChatClient: 断开连接回调被调用";
            self->m_connected = false;
            emit self->connectionStatusChanged(false);
            emit self->disconnected();
            qDebug() << "ChatClient: 与服务器断开连接";
        });
        
        m_networkManager->setOnMessageReceived([self](const std::string& sender, const std::string& content) {
            if (!self) {
                qWarning() << "ChatClient: 消息接收回调被调用，但对象已销毁";
                return;
            }
            
            QString qSender = QString::fromStdString(sender);
            QString qContent = QString::fromStdString(content);
            
            // 特殊处理用户列表消息
            if (qSender == "USER_LIST") {
                // 解析用户列表（逗号分隔的用户名列表）
                QStringList userList = qContent.split(",", Qt::SkipEmptyParts);
                emit self->userListUpdated(userList);
                qDebug() << "ChatClient: 收到用户列表，用户数量:" << userList.size();
            } else {
                emit self->messageReceived(qSender, qContent);
                qDebug() << "ChatClient: 收到消息 from" << qSender << ":" << qContent;
            }
        });
        
        m_networkManager->setOnError([self](const std::string& error) {
            if (!self) return;
            QString qError = QString::fromStdString(error);
            self->m_lastConnectionError = qError;
            emit self->errorOccurred(qError);
            qWarning() << "ChatClient: 网络错误:" << qError;
        });
        
        bool success = m_networkManager->connectToServer(serverAddress, port, user);
        
        if (!success) {
            qWarning() << "ChatClient: 连接服务器失败";
            // 仅当网络层未上报具体原因时再发通用提示（避免与 onError 重复弹窗）
            if (m_lastConnectionError.isEmpty())
                emit errorOccurred("连接服务器失败，请检查服务器地址和端口");
        } else {
            qDebug() << "ChatClient: 连接服务器成功（真实连接）";
        }
        
    } catch (const std::exception& e) {
        QString error = QString("连接异常: %1").arg(e.what());
        emit errorOccurred(error);
        qCritical() << "ChatClient: 连接异常:" << error;
    }
}

/**
 * @brief 发送聊天消息
 */
void ChatClient::sendMessage(const QString& message)
{
    if (!m_connected) {
        qWarning() << "ChatClient: 未连接到服务器，无法发送消息";
        emit errorOccurred("未连接到服务器，无法发送消息");
        return;
    }
    
    if (message.trimmed().isEmpty()) {
        qWarning() << "ChatClient: 消息内容为空";
        return;
    }
    
    try {
        std::string msg = message.toStdString();
        bool success = m_networkManager->sendMessage(msg);
        
        if (!success) {
            qWarning() << "ChatClient: 发送消息失败";
            emit errorOccurred("发送消息失败");
        } else {
            qDebug() << "ChatClient: 消息发送成功:" << message;
        }
        
    } catch (const std::exception& e) {
        QString error = QString("发送消息异常: %1").arg(e.what());
        emit errorOccurred(error);
        qCritical() << "ChatClient: 发送消息异常:" << error;
    }
}

/**
 * @brief 断开与服务器的连接
 */
void ChatClient::disconnectFromServer()
{
    qDebug() << "ChatClient: 断开服务器连接";
    
    if (!m_connected) {
        qDebug() << "ChatClient: 未连接到服务器，无需断开";
        return;
    }
    
    try {
        m_networkManager->disconnect();
        m_connected = false;
        emit connectionStatusChanged(false);
        emit disconnected();
        qDebug() << "ChatClient: 已断开服务器连接";
        
    } catch (const std::exception& e) {
        QString error = QString("断开连接异常: %1").arg(e.what());
        emit errorOccurred(error);
        qCritical() << "ChatClient: 断开连接异常:" << error;
    }
}

/**
 * @brief 请求用户列表
 */
void ChatClient::requestUserList()
{
    if (!m_connected) {
        qWarning() << "ChatClient: 未连接到服务器，无法请求用户列表";
        return;
    }
    
    try {
        bool success = m_networkManager->requestUserList();
        
        if (!success) {
            qWarning() << "ChatClient: 请求用户列表失败";
            emit errorOccurred("请求用户列表失败");
        } else {
            qDebug() << "ChatClient: 用户列表请求已发送";
        }
        
    } catch (const std::exception& e) {
        QString error = QString("请求用户列表异常: %1").arg(e.what());
        emit errorOccurred(error);
        qCritical() << "ChatClient: 请求用户列表异常:" << error;
    }
}

/**
 * @brief 检查是否已连接
 * @return 连接状态
 */
bool ChatClient::isConnected() const
{
    return m_connected;
}