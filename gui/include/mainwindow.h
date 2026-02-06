#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QAction>
#include <QTimer>
#include <QDateTime>
#include <QScrollBar>
#include <QLabel>
#include "chatclient.h"
#include "ui_mainwindow.h"

/**
 * @brief MainWindow 类 - 聊天室主窗口
 * 
 * 提供聊天消息显示、消息发送、用户列表等核心功能
 * 用户登录成功后显示此窗口
 */
class MainWindow : public QMainWindow, private Ui::MainWindow
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param chatClient 聊天客户端实例
     * @param parent 父窗口
     */
    explicit MainWindow(ChatClient *chatClient, QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~MainWindow();

protected:
    /**
     * @brief 关闭事件处理
     * @param event 关闭事件
     */
    void closeEvent(QCloseEvent *event) override;
    
    /**
     * @brief 按键事件处理（快捷键）
     * @param event 按键事件
     */
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    /**
     * @brief 发送消息
     */
    void sendMessage();
    
    /**
     * @brief 处理接收到的消息
     * @param username 用户名
     * @param message 消息内容
     */
    void onMessageReceived(const QString &username, const QString &message);
    
    /**
     * @brief 处理连接状态变化
     * @param connected 连接状态
     */
    void onConnectionStatusChanged(bool connected);
    
    /**
     * @brief 更新时间显示
     */
    void updateTime();
    
    /**
     * @brief 切换用户列表显示
     */
    void toggleUserList();
    
    /**
     * @brief 显示关于对话框
     */
    void showAbout();

private:
    /**
     * @brief 初始化菜单栏
     */
    void setupMenuBar();
    
    /**
     * @brief 初始化信号槽连接
     */
    void setupConnections();
    
    /**
     * @brief 初始化状态栏
     */
    void setupStatusBar();
    
    /**
     * @brief 初始化聊天显示区域
     */
    void setupChatDisplay();
    
    /**
     * @brief 设置界面启用状态
     * @param enabled 是否启用
     */
    void setUIEnabled(bool enabled);
    
    /**
     * @brief 添加消息到聊天显示区域
     * @param username 用户名
     * @param message 消息内容
     * @param color 消息颜色
     */
    void appendMessage(const QString &username, const QString &message, const QColor &color);
    
    /**
     * @brief 更新用户列表
     * @param users 用户列表
     */
    void updateUserList(const QStringList &users);

private:
    // 核心组件
    ChatClient *m_chatClient;           // 聊天客户端
    
    // 菜单栏组件
    QMenu *m_fileMenu;                  // 文件菜单
    QMenu *m_viewMenu;                  // 视图菜单
    QMenu *m_helpMenu;                  // 帮助菜单
    
    // 状态栏组件
    QStatusBar *m_statusBar;            // 状态栏
    QLabel *m_statusLabel;              // 状态标签
    QLabel *m_timeLabel;                // 时间标签
    
    // 定时器
    QTimer *m_statusTimer;              // 状态栏更新时间定时器
    
    // 状态标志
    bool m_connected;                    // 连接状态
    QString m_currentUsername;          // 当前用户名
};

#endif // MAINWINDOW_H