## YGOPro(Server)
[![Build status](https://ci.appveyor.com/api/projects/status/qgkqi6o0wq7qn922/branch/server?svg=true)](https://ci.appveyor.com/project/zh99998/ygopro/branch/server)
[![Build Status](https://travis-ci.org/moecube/ygopro.svg?branch=server)](https://travis-ci.org/moecube/ygopro)

YGOPro的服务端，在线残局。

### 游玩方法

使用原版客户端以进入游玩。

请在 `47.102.140.37:7911` 连接进行游戏。

进入房间请在聊天框输入 `/ai SecondHand` 添加人机，该人机只会出拳头。

### Linux下编译
* 需要以下组件或工具
 * gcc
 * premake5
 * libevent
 * lua5.3
 * sqlite3
* 可参考本项目 [.travis.yml](https://github.com/mycard/ygopro/blob/server/.travis.yml) 中的脚本

### Windows下编译
* 需要以下组件或工具
 * Visual Studio
 * premake5
 * libevent
 * lua5.3
 * sqlite3
* 可参考本项目 [appveyor.yml](https://github.com/mycard/ygopro/blob/server/appveyor.yml) 中的脚本

### 服务端部署

- 房间默认不检查卡组，起始手卡0，每回合抽卡0。

- 添加一个只会选择后手的AI。