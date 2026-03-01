#include "mainwindow.h"
#include "loginwindow.h"
#include <QCloseEvent>
#include <QKeyEvent>
#include <QMessageBox>
#include <QApplication>
#include <QScrollBar>
#include <QDateTime>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QListWidgetItem>
#include <QInputDialog>
#include <QDebug>

/**
 * @brief MainWindow 构造函数
 */
MainWindow::MainWindow(ChatClient *chatClient, QWidget *parent)
    : QMainWindow(parent)
    , m_chatClient(chatClient)
    , m_fileMenu(nullptr)
    , m_viewMenu(nullptr)
    , m_helpMenu(nullptr)
    , m_statusBar(nullptr)
    , m_statusLabel(nullptr)
    , m_timeLabel(nullptr)
    , m_statusTimer(nullptr)
    , m_connected(true)
    , m_currentUsername("")
{
    // 设置UI界面（使用UI文件中定义的界面）
    setupUi(this);

    // 1. 先初始化状态栏（创建 m_statusLabel 指针）
    setupStatusBar(); 
    
    // 2. 再初始化其他 UI 逻辑
    setupMenuBar();
    setupChatDisplay();
    setupConnections();
    
    // 3. 最后再调用涉及到这些指针的状态设置函数
    setUIEnabled(true);
    
    // 4. 设置定时器定期请求用户列表
    QTimer *userListTimer = new QTimer(this);
    connect(userListTimer, &QTimer::timeout, [this]() {
        if (m_chatClient && m_chatClient->isConnected()) {
            m_chatClient->requestUserList();
        }
    });
    userListTimer->start(5000); // 每5秒请求一次用户列表
    
    qDebug() << "MainWindow: 主窗口创建完成";
}

/**
 * @brief MainWindow 析构函数
 */
MainWindow::~MainWindow()
{
    qDebug() << "MainWindow: 主窗口销毁";
}

/**
 * @brief 初始化菜单栏
 */
void MainWindow::setupMenuBar()
{
    // 文件菜单
    m_fileMenu = menuBar()->addMenu("文件");
    
    QAction *exitAction = new QAction("退出", this);
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &MainWindow::close);
    m_fileMenu->addAction(exitAction);
    
    // 视图菜单
    m_viewMenu = menuBar()->addMenu("视图");
    
    QAction *toggleUserListAction = new QAction("显示/隐藏用户列表", this);
    toggleUserListAction->setShortcut(QKeySequence("Ctrl+L"));
    connect(toggleUserListAction, &QAction::triggered, this, &MainWindow::toggleUserList);
    m_viewMenu->addAction(toggleUserListAction);
    
    // 帮助菜单
    m_helpMenu = menuBar()->addMenu("帮助");
    
    QAction *aboutAction = new QAction("关于", this);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
    m_helpMenu->addAction(aboutAction);
}

/**
 * @brief 初始化状态栏
 */
void MainWindow::setupStatusBar()
{
    m_statusBar = statusBar();
    
    // 状态标签
    m_statusLabel = new QLabel("已连接");
    m_statusLabel->setStyleSheet("color: #28a745; font-weight: bold;");
    m_statusBar->addWidget(m_statusLabel);
    
    // 时间标签
    m_timeLabel = new QLabel();
    m_timeLabel->setStyleSheet("color: #6c757d;");
    m_statusBar->addPermanentWidget(m_timeLabel);
    
    // 更新时间显示
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::updateTime);
    m_statusTimer->start(1000); // 每秒更新一次
    
    updateTime();
}

/**
 * @brief 初始化聊天显示区域
 */
void MainWindow::setupChatDisplay()
{
    // 设置聊天显示区域为只读
    chatDisplay->setReadOnly(true);
    
    // 添加欢迎消息
    appendMessage("系统", "欢迎使用 Chat01 聊天室！", QColor("#6c757d"));
    appendMessage("系统", "您可以开始发送消息了。", QColor("#6c757d"));
}

/**
 * @brief 初始化信号槽连接
 */
void MainWindow::setupConnections()
{
    // 发送按钮点击
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::sendMessage);
    
    // 回车键发送消息
    connect(messageInput, &QLineEdit::returnPressed, this, &MainWindow::sendMessage);
    
    // 聊天客户端信号
    connect(m_chatClient, &ChatClient::messageReceived, 
            this, &MainWindow::onMessageReceived);
    connect(m_chatClient, &ChatClient::connectionStatusChanged, 
            this, &MainWindow::onConnectionStatusChanged);
    // 用户列表更新信号
    connect(m_chatClient, &ChatClient::userListUpdated,
            this, &MainWindow::updateUserList);
}

/**
 * @brief 设置界面启用状态
 */
void MainWindow::setUIEnabled(bool enabled)
{
    messageInput->setEnabled(enabled);
    sendButton->setEnabled(enabled);
    
    if (enabled) {
        m_statusLabel->setText("已连接");
        m_statusLabel->setStyleSheet("color: #28a745; font-weight: bold;");
    } else {
        m_statusLabel->setText("连接中...");
        m_statusLabel->setStyleSheet("color: #ffc107; font-weight: bold;");
    }
}

/**
 * @brief 发送消息
 */
void MainWindow::sendMessage()
{
    QString message = messageInput->text().trimmed();
    if (message.isEmpty()) {
        return;
    }
    
    // 清空输入框
    messageInput->clear();
    
    // 显示自己发送的消息
    appendMessage("我", message, QColor("#007bff"));
    
    // 通过聊天客户端发送消息
    if (m_chatClient && m_chatClient->isConnected()) {
        m_chatClient->sendMessage(message);
    } else {
        appendMessage("系统", "未连接到服务器，消息发送失败", QColor("#dc3545"));
    }
}

/**
 * @brief 添加消息到聊天显示区域
 */
void MainWindow::appendMessage(const QString &username, const QString &message, const QColor &color)
{
    QTextCursor cursor = chatDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    
    // 设置时间戳
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    
    // 格式化消息
    QString formattedMessage = QString("[%1] %2: %3\n").arg(timestamp, username, message);
    
    // 插入消息
    cursor.insertText(formattedMessage);
    
    // 设置消息颜色
    QTextCharFormat format;
    format.setForeground(color);
    cursor.setCharFormat(format);
    
    // 自动滚动到底部
    chatDisplay->verticalScrollBar()->setValue(chatDisplay->verticalScrollBar()->maximum());
}

/**
 * @brief 更新用户列表
 */
void MainWindow::updateUserList(const QStringList &users)
{
    userList->clear();
    
    for (const QString &user : users) {
        QListWidgetItem *item = new QListWidgetItem(user);
        
        // 设置用户在线状态样式
        item->setForeground(QColor("#28a745"));
        
        userList->addItem(item);
    }
    
    // 更新用户数量显示
    userListLabel->setText(QString("在线用户 (%1)").arg(users.size()));
}

/**
 * @brief 更新时间显示
 */
void MainWindow::updateTime()
{
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    m_timeLabel->setText(currentTime);
}

/**
 * @brief 切换用户列表显示
 */
void MainWindow::toggleUserList()
{
    bool visible = userWidget->isVisible();
    userWidget->setVisible(!visible);
}

/**
 * @brief 显示关于对话框
 */
void MainWindow::showAbout()
{
    QMessageBox::about(this, "关于 Chat01", 
                       "Chat01 聊天室\n"
                       "版本 1.0\n"
                       "基于 Qt5 开发的跨平台聊天应用\n\n"
                       "功能特性：\n"
                       "• 实时消息收发\n"
                       "• 用户在线状态显示\n"
                       "• 多窗口聊天界面\n"
                       "• 自定义主题样式");
}

/**
 * @brief 处理接收到的消息
 */
void MainWindow::onMessageReceived(const QString &username, const QString &message)
{
    appendMessage(username, message, QColor("#28a745"));
}

/**
 * @brief 处理连接状态变化
 */
void MainWindow::onConnectionStatusChanged(bool connected)
{
    m_connected = connected;
    setUIEnabled(connected);
    
    if (!connected) {
        appendMessage("系统", "与服务器的连接已断开", QColor("#dc3545"));
    }
}

/**
 * @brief 按键事件处理（快捷键）
 */
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // 处理Ctrl+L快捷键：切换用户列表
    if (event->key() == Qt::Key_L && event->modifiers() == Qt::ControlModifier) {
        toggleUserList();
        event->accept();
        return;
    }
    
    // 处理Ctrl+Q快捷键：退出程序
    if (event->key() == Qt::Key_Q && event->modifiers() == Qt::ControlModifier) {
        close();
        event->accept();
        return;
    }
    
    // 处理Esc键：清空输入框或最小化窗口
    if (event->key() == Qt::Key_Escape) {
        if (!messageInput->text().isEmpty()) {
            messageInput->clear();
            event->accept();
            return;
        } else {
            showMinimized();
            event->accept();
            return;
        }
    }
    
    // 其他按键事件交给父类处理
    QMainWindow::keyPressEvent(event);
}

/**
 * @brief 关闭事件处理
 */
void MainWindow::closeEvent(QCloseEvent *event)
{
    qDebug() << "MainWindow: 窗口关闭事件";
    
    // 断开网络连接
    if (m_chatClient) {
        m_chatClient->disconnectFromServer();
    }
    
    event->accept();
}