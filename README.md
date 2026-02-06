


          
# Chat01 - Qt聊天应用

一个基于Qt框架开发的完整聊天应用程序，包含图形界面客户端和服务器端。(还没做完/Not finished yet)

## 🚀 项目特性

- **现代化GUI界面** - 使用Qt框架开发的用户友好界面
- **客户端-服务器架构** - 完整的网络通信系统
- **模块化设计** - 清晰的代码组织结构
- **多用户支持** - 支持多个用户同时在线聊天
- **账户管理** - 用户注册、登录和会话管理

## 📁 项目结构

```
Chat01/
├── client/           # 聊天客户端
│   ├── include/      # 头文件
│   └── src/          # 源代码
├── server/           # 聊天服务器
│   ├── include/      # 头文件
│   └── src/          # 源代码
├── common/           # 公共库
│   ├── include/      # 公共头文件
│   └── src/          # 公共源代码
├── gui/              # Qt图形界面
│   ├── forms/        # UI设计文件
│   ├── include/      # 头文件
│   ├── resources/    # 资源文件
│   └── src/          # 源代码
└── accounts/         # 账户数据管理
```

## 🛠️ 构建说明

### 依赖要求

- **Qt 5.x 或更高版本**
- **C++14 兼容编译器** (g++ 或 clang++)
- **CMake** (可选，项目使用Makefile)

### 构建步骤

1. **构建公共库**
```bash
cd common
make
```

2. **构建服务器**
```bash
cd server
make
```

3. **构建客户端**
```bash
cd client
make
```

4. **构建GUI应用**
```bash
cd gui
qmake Chat01GUI.pro
make
```

### 运行应用

**启动服务器：**
```bash
./build/ChatServer
```

**启动GUI客户端：**
```bash
./gui/build/Chat01GUI
```

**启动命令行客户端：**
```bash
./build/ChatClient
```

## 📋 功能特性

### 客户端功能
- ✅ 用户登录/注册界面
- ✅ 实时消息发送和接收
- ✅ 在线用户列表显示
- ✅ 聊天会话管理
- ✅ 消息历史记录

### 服务器功能
- ✅ 多客户端连接管理
- ✅ 消息路由和广播
- ✅ 用户认证和授权
- ✅ 会话状态维护
- ✅ 账户数据持久化

## 🔧 技术栈

- **前端**: Qt Widgets, QML (可选)
- **后端**: C++14, 多线程网络编程
- **网络**: TCP/IP Socket通信
- **构建**: Makefile, qmake
- **数据格式**: JSON (账户数据)

## 🚀 快速开始

1. 克隆项目到本地
2. 安装Qt开发环境
3. 按照构建说明编译项目
4. 先启动服务器，再启动客户端
5. 注册账户并开始聊天

## 🤝 贡献指南

欢迎提交Issue和Pull Request来改进这个项目！

## 📞 联系方式

如有问题或建议，请通过以下方式联系：
- 邮箱: 3177191187@qq.com
- GitHub Issues: [项目Issues页面]

---

*Connect. Chat. Done.*
        
