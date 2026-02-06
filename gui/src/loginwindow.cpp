#include "loginwindow.h"
#include "../include/mainwindow.h"
#include <QCloseEvent>
#include <QMessageBox>
#include <QApplication>
#include <QIntValidator>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QTimer>
#include <QDebug>

/**
 * @brief LoginWindow 构造函数
 */
LoginWindow::LoginWindow(QWidget *parent)
    : QWidget(parent)
    , m_chatClient(nullptr)
    , m_settings(nullptr)
    , m_isConnecting(false)
{
    // 设置UI界面（使用UI文件中定义的界面）
    setupUi(this);
    
    // 创建设置对象
    m_settings = new QSettings("Chat01", "GUI", this);
    
    // 创建聊天客户端
    m_chatClient = new ChatClient(this);
    
    // 初始化信号槽连接
    setupConnections();
    
    // 加载保存的设置
    loadSettings();
    
    // 设置窗口属性
    setWindowTitle("Chat01 - 登录");
    // UI文件中已经设置了窗口尺寸，这里不再硬编码
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
    
    // 设置输入验证器
    setupValidators();
    
    qDebug() << "LoginWindow: 登录窗口创建完成";
}

/**
 * @brief LoginWindow 析构函数
 */
LoginWindow::~LoginWindow()
{
    qDebug() << "LoginWindow: 登录窗口销毁";
}

/**
 * @brief 设置输入验证器
 */
void LoginWindow::setupValidators()
{
    // IP地址验证器
    QRegularExpression ipRegex("^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
    QRegularExpressionValidator *ipValidator = new QRegularExpressionValidator(ipRegex, serverAddressEdit);
    serverAddressEdit->setValidator(ipValidator);
    
    // 端口号验证器
    portEdit->setValidator(new QIntValidator(1, 65535, portEdit));
    
    // 用户名长度限制
    usernameEdit->setMaxLength(20);
}

/**
 * @brief 初始化信号槽连接
 */
void LoginWindow::setupConnections()
{
    // 按钮点击信号
    connect(loginButton, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    
    // 回车键快捷登录
    connect(usernameEdit, &QLineEdit::returnPressed, this, &LoginWindow::onLoginClicked);
    connect(serverAddressEdit, &QLineEdit::returnPressed, this, &LoginWindow::onLoginClicked);
    connect(portEdit, &QLineEdit::returnPressed, this, &LoginWindow::onLoginClicked);
    
    // 聊天客户端信号
    connect(m_chatClient, &ChatClient::connectionStatusChanged, 
            this, &LoginWindow::onConnectionStatusChanged);
    connect(m_chatClient, &ChatClient::connected, 
            this, &LoginWindow::onConnected);
    connect(m_chatClient, &ChatClient::disconnected, 
            this, &LoginWindow::onDisconnected);
    connect(m_chatClient, &ChatClient::errorOccurred, 
            this, &LoginWindow::onErrorOccurred);
}

/**
 * @brief 加载保存的登录设置
 */
void LoginWindow::loadSettings()
{
    // 从设置中加载保存的值
    QString username = m_settings->value("login/username").toString();
    QString serverAddress = m_settings->value("login/serverAddress", "localhost").toString();
    int port = m_settings->value("login/port", 8080).toInt();
    bool remember = m_settings->value("login/remember", false).toBool();
    
    usernameEdit->setText(username);
    serverAddressEdit->setText(serverAddress);
    portEdit->setText(QString::number(port));
    rememberCheckBox->setChecked(remember);
}

/**
 * @brief 保存登录设置
 */
void LoginWindow::saveSettings()
{
    if (rememberCheckBox->isChecked()) {
        m_settings->setValue("login/username", usernameEdit->text());
        m_settings->setValue("login/serverAddress", serverAddressEdit->text());
        m_settings->setValue("login/port", portEdit->text().toInt());
        m_settings->setValue("login/remember", true);
    } else {
        // 如果不记住，清除保存的设置
        m_settings->remove("login/username");
        m_settings->remove("login/remember");
    }
    
    m_settings->sync();
}

/**
 * @brief 验证输入
 */
bool LoginWindow::validateInput()
{
    QString username = usernameEdit->text().trimmed();
    QString serverAddress = serverAddressEdit->text().trimmed();
    QString portText = portEdit->text().trimmed();
    
    // 检查用户名
    if (username.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请输入用户名");
        usernameEdit->setFocus();
        return false;
    }
    
    // 检查服务器地址
    if (serverAddress.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请输入服务器地址");
        serverAddressEdit->setFocus();
        return false;
    }
    
    // 检查端口号
    if (portText.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请输入端口号");
        portEdit->setFocus();
        return false;
    }
    
    bool ok;
    int port = portText.toInt(&ok);
    if (!ok || port < 1 || port > 65535) {
        QMessageBox::warning(this, "输入错误", "请输入有效的端口号 (1-65535)");
        portEdit->setFocus();
        return false;
    }
    
    return true;
}

/**
 * @brief 设置界面可用状态
 */
void LoginWindow::setUIEnabled(bool enabled)
{
    usernameEdit->setEnabled(enabled);
    serverAddressEdit->setEnabled(enabled);
    portEdit->setEnabled(enabled);
    rememberCheckBox->setEnabled(enabled);
    loginButton->setEnabled(enabled);
}

/**
 * @brief 处理登录按钮点击
 */
void LoginWindow::onLoginClicked()
{
    if (!validateInput()) {
        return;
    }
    
    // 保存设置
    saveSettings();
    
    // 更新界面状态
    m_isConnecting = true;
    setUIEnabled(false);
    statusLabel->setText("正在连接服务器...");
    statusLabel->setStyleSheet("color: #f39c12; font-size: 12px;");
    
    // 获取输入参数
    QString username = usernameEdit->text().trimmed();
    QString serverAddress = serverAddressEdit->text().trimmed();
    int port = portEdit->text().toInt();
    
    qDebug() << "LoginWindow: 尝试连接到服务器" << serverAddress << ":" << port;
    
    // 调用聊天客户端连接
    m_chatClient->connectToServer(serverAddress, port, username);
}

/**
 * @brief 处理连接状态变化
 */
void LoginWindow::onConnectionStatusChanged(bool connected)
{
    qDebug() << "LoginWindow: 连接状态变化:" << connected;
    
    if (connected) {
        statusLabel->setText("连接成功!");
        statusLabel->setStyleSheet("color: #27ae60; font-size: 12px;");
    } else {
        statusLabel->setText("连接已断开");
        statusLabel->setStyleSheet("color: #e74c3c; font-size: 12px;");
        setUIEnabled(true);
        m_isConnecting = false;
    }
}

/**
 * @brief 处理连接成功
 */
void LoginWindow::onConnected()
{
    qDebug() << "LoginWindow: 连接成功";
    
    // 创建并显示主窗口
    MainWindow *mainWindow = new MainWindow(m_chatClient);
    
    // 设置主窗口关闭时退出应用程序
    mainWindow->setAttribute(Qt::WA_DeleteOnClose);
    
    // 连接主窗口关闭信号到应用程序退出
    connect(mainWindow, &MainWindow::destroyed, qApp, &QApplication::quit);
    
    mainWindow->show();
    
    // 隐藏登录窗口
    hide();
}

/**
 * @brief 处理连接断开
 */
void LoginWindow::onDisconnected()
{
    qDebug() << "LoginWindow: 连接断开";
    
    statusLabel->setText("连接已断开");
    statusLabel->setStyleSheet("color: #e74c3c; font-size: 12px;");
    setUIEnabled(true);
    m_isConnecting = false;
}

/**
 * @brief 处理连接错误
 */
void LoginWindow::onErrorOccurred(const QString &error)
{
    qDebug() << "LoginWindow: 连接错误:" << error;
    
    QMessageBox::critical(this, "连接错误", "连接服务器失败: " + error);
    statusLabel->setText("连接失败: " + error);
    statusLabel->setStyleSheet("color: #e74c3c; font-size: 12px;");
    setUIEnabled(true);
    m_isConnecting = false;
}

/**
 * @brief 关闭事件处理
 */
void LoginWindow::closeEvent(QCloseEvent *event)
{
    qDebug() << "LoginWindow: 窗口关闭事件";
    
    // 如果正在连接，先断开连接
    if (m_isConnecting) {
        m_chatClient->disconnectFromServer();
    }
    
    // 保存设置
    saveSettings();
    
    event->accept();
}