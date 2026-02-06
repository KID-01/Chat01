#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QMessageBox>
#include <QSettings>
#include "chatclient.h"
#include "ui_loginwindow.h"

/**
 * @brief LoginWindow 类 - 聊天室登录窗口
 * 
 * 提供用户身份验证界面，连接服务器配置，
 * 登录成功后跳转到主聊天窗口
 */
class LoginWindow : public QWidget, private Ui::LoginWindow
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit LoginWindow(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~LoginWindow();

protected:
    /**
     * @brief 关闭事件处理
     * @param event 关闭事件
     */
    void closeEvent(QCloseEvent *event) override;

private slots:
    /**
     * @brief 处理登录按钮点击
     */
    void onLoginClicked();
    
    /**
     * @brief 处理连接状态变化
     * @param connected 连接状态
     */
    void onConnectionStatusChanged(bool connected);
    
    /**
     * @brief 处理连接成功
     */
    void onConnected();
    
    /**
     * @brief 处理连接断开
     */
    void onDisconnected();
    
    /**
     * @brief 处理网络错误
     * @param error 错误信息
     */
    void onErrorOccurred(const QString& error);

private:
    /**
     * @brief 设置输入验证器
     */
    void setupValidators();
    
    /**
     * @brief 初始化信号槽连接
     */
    void setupConnections();
    
    /**
     * @brief 加载保存的登录设置
     */
    void loadSettings();
    
    /**
     * @brief 保存登录设置
     */
    void saveSettings();
    
    /**
     * @brief 验证输入有效性
     * @return 输入是否有效
     */
    bool validateInput();
    
    /**
     * @brief 设置界面启用状态
     * @param enabled 是否启用
     */
    void setUIEnabled(bool enabled);

private:
    // 业务逻辑
    ChatClient *m_chatClient;       // 聊天客户端实例
    QSettings *m_settings;          // 应用设置
    
    // 状态标志
    bool m_isConnecting;            // 是否正在连接中
};

#endif // LOGINWINDOW_H