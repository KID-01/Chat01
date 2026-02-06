#include "../include/loginwindow.h"
#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QStyleFactory>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QMessageBox>
#include <QDebug>

/**
 * @brief 应用程序消息处理函数
 * 
 * 捕获Qt应用程序的调试、警告、严重错误消息
 * 可以重定向到文件或进行自定义处理
 */
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";
    
    // 时间戳
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    
    // 日志文件路径（在用户主目录下创建Chat01日志目录）
    QString logDirPath = QDir::homePath() + "/.Chat01/logs";
    QDir logDir(logDirPath);
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }
    
    QString logFilePath = logDirPath + "/chat01_gui_" + 
                         QDateTime::currentDateTime().toString("yyyyMMdd") + ".log";
    
    // 打开日志文件（追加模式）
    QFile logFile(logFilePath);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&logFile);
        
        // 根据消息类型格式化输出
        switch (type) {
        case QtDebugMsg:
            stream << QString("[%1] [DEBUG] %2 (%3:%4, %5)\n")
                      .arg(timestamp).arg(msg).arg(file).arg(context.line).arg(function);
            break;
        case QtInfoMsg:
            stream << QString("[%1] [INFO]  %2\n").arg(timestamp).arg(msg);
            break;
        case QtWarningMsg:
            stream << QString("[%1] [WARN]  %2 (%3:%4, %5)\n")
                      .arg(timestamp).arg(msg).arg(file).arg(context.line).arg(function);
            break;
        case QtCriticalMsg:
            stream << QString("[%1] [ERROR] %2 (%3:%4, %5)\n")
                      .arg(timestamp).arg(msg).arg(file).arg(context.line).arg(function);
            break;
        case QtFatalMsg:
            stream << QString("[%1] [FATAL] %2 (%3:%4, %5)\n")
                      .arg(timestamp).arg(msg).arg(file).arg(context.line).arg(function);
            break;
        }
        
        logFile.close();
    }
    
    // 同时在控制台输出（便于调试）
    QTextStream out(stdout);
    switch (type) {
    case QtDebugMsg:
        out << QString("[DEBUG] %1\n").arg(msg);
        break;
    case QtInfoMsg:
        out << QString("[INFO]  %1\n").arg(msg);
        break;
    case QtWarningMsg:
        out << QString("[WARN]  %1\n").arg(msg);
        break;
    case QtCriticalMsg:
        out << QString("[ERROR] %1\n").arg(msg);
        break;
    case QtFatalMsg:
        out << QString("[FATAL] %1\n").arg(msg);
        break;
    }
    out.flush();
}

/**
 * @brief 加载应用程序样式表
 * 
 * 可以加载自定义的QSS样式文件来美化界面
 * 如果文件不存在，使用默认的Fusion样式
 */
void loadApplicationStyle(QApplication &app)
{
    // 尝试加载自定义样式表
    QString stylePath = ":/styles/default.qss";
    QFile styleFile(stylePath);
    
    if (styleFile.exists() && styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&styleFile);
        QString styleSheet = stream.readAll();
        app.setStyleSheet(styleSheet);
        styleFile.close();
        qDebug() << "自定义样式表加载成功";
    } else {
        // 使用系统Fusion样式（跨平台一致性）
        app.setStyle(QStyleFactory::create("Fusion"));
        
        // 设置Fusion样式的调色板
        QPalette palette;
        palette.setColor(QPalette::Window, QColor(240, 240, 240));
        palette.setColor(QPalette::WindowText, Qt::black);
        palette.setColor(QPalette::Base, QColor(255, 255, 255));
        palette.setColor(QPalette::AlternateBase, QColor(240, 240, 240));
        palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 220));
        palette.setColor(QPalette::ToolTipText, Qt::black);
        palette.setColor(QPalette::Text, Qt::black);
        palette.setColor(QPalette::Button, QColor(240, 240, 240));
        palette.setColor(QPalette::ButtonText, Qt::black);
        palette.setColor(QPalette::BrightText, Qt::red);
        palette.setColor(QPalette::Link, QColor(0, 0, 255));
        palette.setColor(QPalette::Highlight, QColor(0, 120, 215));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        
        app.setPalette(palette);
        qDebug() << "使用Fusion样式";
    }
}

/**
 * @brief 检查应用程序单例运行
 * 
 * 使用文件锁确保同一时间只有一个应用程序实例运行
 * 防止多个聊天窗口同时运行造成冲突
 */
bool isApplicationAlreadyRunning()
{
    // 在临时目录创建锁文件
    QString lockFilePath = QDir::tempPath() + "/chat01_gui.lock";
    QFile lockFile(lockFilePath);
    
    // 尝试以独占方式打开文件
    if (!lockFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        // 如果无法打开，说明文件已被其他进程锁定
        return true;
    }
    
    // 写入当前进程ID
    QTextStream stream(&lockFile);
    stream << QCoreApplication::applicationPid();
    lockFile.close();
    
    // 应用程序退出时删除锁文件
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, [lockFilePath]() {
        QFile::remove(lockFilePath);
    });
    
    return false;
}

/**
 * @brief 应用程序主函数
 * 
 * Qt应用程序的入口点，负责：
 * 1. 创建QApplication实例
 * 2. 设置应用程序属性
 * 3. 初始化各种组件
 * 4. 显示主窗口
 * 5. 进入事件循环
 */
int main(int argc, char *argv[])
{
    // 设置应用程序属性
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);    // 高DPI支持
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);        // 高DPI图标

    // 确保样式表在高DPI下正确传播（解决样式渲染模糊）
    QCoreApplication::setAttribute(Qt::AA_UseStyleSheetPropagationInWidgetStyles);
    // 设置环境变量（针对远程渲染的强制缩放适配）
    // 根据你的Windows屏幕缩放比例调整（比如150%缩放就设为1.5，200%设为2.0）
    qputenv("QT_SCALE_FACTOR", "1.0"); 
    qputenv("QT_FONT_DPI", "96"); // 强制字体DPI为96（标准值，避免Linux/Windows DPI不兼容）
    
    // 创建应用程序实例
    QApplication app(argc, argv);

    // 跨平台清晰字体配置（优先使用系统易渲染的字体）
    QFont appFont;
    #ifdef Q_OS_LINUX
    appFont.setFamily("Noto Sans CJK SC"); // Linux下清晰的中文字体
    #elif defined(Q_OS_WIN)
    appFont.setFamily("Microsoft YaHei");   // Windows下清晰的中文字体
    #endif
    appFont.setPixelSize(12); // 使用像素大小（高DPI下更稳定）
    app.setFont(appFont);
    
    // 设置应用程序信息
    app.setApplicationName("Chat01");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Chat01 Project");
    app.setOrganizationDomain("chat01.example.com");
    app.setWindowIcon(QIcon(":/icons/app_icon.png")); // 需要添加图标资源
    
    // 安装自定义消息处理器
    qInstallMessageHandler(customMessageHandler);
    
    qDebug() << "=== Chat01 GUI 应用程序启动 ===";
    qDebug() << "应用程序名称:" << app.applicationName();
    qDebug() << "应用程序版本:" << app.applicationVersion();
    qDebug() << "Qt版本:" << qVersion();
    qDebug() << "编译时间:" << __DATE__ << __TIME__;
    
    // 检查是否已有实例运行
    if (isApplicationAlreadyRunning()) {
        QMessageBox::warning(nullptr, "应用程序已运行", 
                           "Chat01 聊天室已经在运行中！\n"
                           "请检查系统托盘或任务栏。");
        qWarning() << "应用程序实例已存在，退出当前实例";
        return 1;
    }
    
    // 加载应用程序样式
    loadApplicationStyle(app);
    
    // 设置中文语言环境（如果系统支持）
    QTranslator translator;
    QLocale locale = QLocale::system();
    
    if (locale.language() == QLocale::Chinese) {
        // 尝试加载中文翻译文件
        if (translator.load("chat01_zh_CN.qm", ":/translations")) {
            app.installTranslator(&translator);
            qDebug() << "中文翻译文件加载成功";
        } else {
            qDebug() << "使用默认语言（中文）";
        }
    }
    
    try {
        // 创建并显示登录窗口
        LoginWindow loginWindow;
        loginWindow.show();
        
        qDebug() << "登录窗口显示完成，进入事件循环";
        
        // 进入主事件循环
        return app.exec();
        
    } catch (const std::exception& e) {
        // 捕获并处理异常
        QString errorMsg = QString("应用程序启动异常: %1").arg(e.what());
        qCritical() << errorMsg;
        
        QMessageBox::critical(nullptr, "启动错误", 
                             QString("应用程序启动失败:\n%1\n\n"
                                     "请检查系统环境并重新启动。").arg(errorMsg));
        return -1;
        
    } catch (...) {
        // 捕获未知异常
        qCritical() << "未知异常导致应用程序启动失败";
        
        QMessageBox::critical(nullptr, "启动错误", 
                             "应用程序遇到未知错误，无法启动。\n"
                             "请尝试重新启动应用程序。");
        return -1;
    }
}