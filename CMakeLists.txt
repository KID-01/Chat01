cmake_minimum_required(VERSION 3.16)

project(Chat01)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# =====================================================
# MSVC 处理：去掉静态运行时强制约束
# =====================================================
if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/utf-8)
    # 动态链接模式下，建议移除强制 set(CMAKE_MSVC_RUNTIME_LIBRARY ...) 
    # 让 CMake 自动匹配 Qt 动态库的运行时 (通常是 /MD)
endif()

# =====================================================
# Qt 路径 (请确认此路径下有 bin/*.dll 和 lib/cmake/Qt5)
# =====================================================
if(MSVC)
    set(QT_ROOT "E:/QTLearning/Qt5.14.2/5.14.2/msvc2017_64") 
    set(Qt5_DIR "${QT_ROOT}/lib/cmake/Qt5")
    list(APPEND CMAKE_PREFIX_PATH "${QT_ROOT}")
    message(STATUS "DEBUG: QT_ROOT is ${QT_ROOT}")
endif()

message(STATUS "DEBUG: Searching for Qt5...")
find_package(Threads REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Network)
message(STATUS "DEBUG: Found Qt5? ${Qt5_FOUND}")
message(STATUS "DEBUG: Qt5Widgets_LIBRARIES: ${Qt5Widgets_LIBRARIES}")

# =====================================================
# 1. CommonLib
# =====================================================
add_library(CommonLib STATIC
    common/src/Message.cpp
    common/src/User.cpp
    common/src/PlatformNetwork.cpp
)
target_include_directories(CommonLib PUBLIC common/include)

# =====================================================
# 2. Server
# =====================================================
add_executable(ChatServer
    server/src/main.cpp
    server/src/ChatServer.cpp
    server/src/ServerNetworkManager.cpp
    server/src/AccountManager.cpp
)
target_include_directories(ChatServer PRIVATE server/include)
target_link_libraries(ChatServer PRIVATE CommonLib Threads::Threads ws2_32)

# =====================================================
# 3. Client Logic
# =====================================================
add_library(ChatClientLogic STATIC
    client/src/ClientNetworkManager.cpp
)
target_include_directories(ChatClientLogic PUBLIC client/include)
target_link_libraries(ChatClientLogic PUBLIC CommonLib)

# =====================================================
# 4. GUI (恢复简洁模式)
# =====================================================
file(GLOB_RECURSE GUI_SOURCES   "gui/src/*.cpp")
file(GLOB_RECURSE GUI_HEADERS   "gui/include/*.h")
file(GLOB_RECURSE GUI_UI_FILES  "gui/forms/*.ui")
file(GLOB_RECURSE GUI_RESOURCES "gui/resources/*.qrc")

set(CMAKE_AUTOUIC_SEARCH_PATHS gui/forms)

# WIN32 确保运行程序时不弹出黑色控制台窗口
add_executable(Chat01GUI WIN32
    ${GUI_SOURCES}
    ${GUI_HEADERS}
    ${GUI_UI_FILES}
    ${GUI_RESOURCES}
)

target_include_directories(Chat01GUI PRIVATE gui/include)

# 移除之前的 QT_STATICPLUGIN 定义
# 移除之前那一长串的静态插件链接（Qt5::QWindowsIntegrationPlugin等）

target_link_libraries(Chat01GUI PRIVATE
    ChatClientLogic
    Qt5::Widgets
    Qt5::Network
    Qt5::Gui
    Qt5::Core
    Threads::Threads
    ws2_32
)