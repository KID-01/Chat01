# Chat01 GUI 项目配置文件
# Qt5 项目文件 - 定义项目的基本配置和依赖

# 项目基本信息
TARGET = Chat01GUI
TEMPLATE = app

# Qt模块依赖
QT += core gui widgets network

# C++标准
CONFIG += c++14

# 源代码目录
SOURCES += src/main.cpp \
           src/mainwindow.cpp \
           src/loginwindow.cpp \
           src/chatclient.cpp \
           ../client/src/ClientNetworkManager.cpp \
           ../common/src/PlatformNetwork.cpp

# 头文件目录
HEADERS += include/mainwindow.h \
           include/loginwindow.h \
           include/chatclient.h \
           ../client/include/ClientNetworkManager.h

# UI表单文件
FORMS += forms/mainwindow.ui \
         forms/loginwindow.ui

# 资源文件（图标、图片等）
RESOURCES += resources/resources.qrc

# 编译输出目录
DESTDIR = ../build/gui
OBJECTS_DIR = build/obj
MOC_DIR = build/moc
RCC_DIR = build/rcc
UI_DIR = build/ui

# 包含路径
INCLUDEPATH += include \
               ../common/include \
               ../client/include

# 链接库路径
# LIBS += -L../build/client -lChatClient  # 暂时注释掉，因为ChatClient库可能不存在

# 调试和发布模式配置
CONFIG(release, debug|release) {
    DEFINES += QT_NO_DEBUG_OUTPUT
    QMAKE_CXXFLAGS += -O2
}

CONFIG(debug, debug|release) {
    DEFINES += QT_DEBUG
    QMAKE_CXXFLAGS += -g -O0
}

# 平台特定配置
unix {
    # Linux特定配置
    QMAKE_CXXFLAGS += -std=c++14
}

win32 {
    # Windows特定配置（MinGW 不识别 #pragma comment(lib)，须显式链接 Winsock）
    QMAKE_CXXFLAGS += -std=c++14
    LIBS += -lws2_32
}