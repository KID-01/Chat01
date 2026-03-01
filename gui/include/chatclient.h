#ifndef CHATCLIENT_H
#define CHATCLIENT_H

#include <QObject>
#include <QString>
#include <QPointer>
#include <memory>

// 前向声明，避免包含完整的网络管理器头文件
class ClientNetworkManager;

/**
 * @brief ChatClient 类 - Qt GUI与网络层的桥梁
 * 
 * 这个类封装了现有的ClientNetworkManager功能，提供Qt信号槽接口
 * 让GUI界面能够以Qt的方式处理网络通信
 */
class ChatClient : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象（Qt对象树管理）
     */
    explicit ChatClient(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     * 确保网络资源正确清理
     */
    ~ChatClient();

public slots:
    /**
     * @brief 连接到聊天服务器
     * @param address 服务器地址
     * @param port 服务器端口
     * @param username 用户名
     */
    void connectToServer(const QString& address, int port, const QString& username);
    
    /**
     * @brief 发送聊天消息
     * @param message 消息内容
     */
    void sendMessage(const QString& message);
    
    /**
     * @brief 断开与服务器的连接
     */
    void disconnectFromServer();
    
    /**
     * @brief 检查是否已连接
     * @return 连接状态
     */
    bool isConnected() const;
    
    /**
     * @brief 请求用户列表
     */
    void requestUserList();

signals:
    /**
     * @brief 收到新消息信号
     * @param sender 发送者用户名
     * @param content 消息内容
     */
    void messageReceived(const QString& sender, const QString& content);
    
    /**
     * @brief 连接状态改变信号
     * @param connected 新的连接状态
     */
    void connectionStatusChanged(bool connected);
    
    /**
     * @brief 发生错误信号
     * @param error 错误信息
     */
    void errorOccurred(const QString& error);
    
    /**
     * @brief 连接成功信号
     */
    void connected();
    
    /**
     * @brief 断开连接信号
     */
    void disconnected();
    
    /**
     * @brief 用户列表更新信号
     * @param users 用户列表
     */
    void userListUpdated(const QStringList& users);

private:
    std::unique_ptr<ClientNetworkManager> m_networkManager;
    bool m_connected;
    QString m_currentUsername;
    QString m_lastConnectionError;  // 最近一次连接失败的具体原因（供界面显示）
};

#endif // CHATCLIENT_H