# 消消乐桌面版本 1.0

本目录包含两个 C 语言桌面版本：命令行版和 raylib 图形版，使用同一份 `game.c`、`game.h` 游戏规则。

发布分支为 `release/desktop-v1.0`，标签分别为 `console-v1.0.0`、`raylib-v1.0.0`。桌面版本不合并到 `main` 或可视化开发分支，现有 Web 页面及 `Matchgamevisual/` 不参与本次发布。

## 直接运行

从 GitHub Releases 下载对应的 Windows x64 ZIP，解压后运行其中的 EXE。二进制使用 Release 配置，并静态链接 C 运行库；图形版也静态链接 raylib。图形版需要支持 OpenGL 3.3 的显卡及驱动。当前程序无需外部图片、字体或音频资源。

- 命令行版：运行 `Matchgame.exe`，输入昵称和消除长度 3、4 或 5；输入 `行 列 W/A/S/D` 交换相邻物品，输入 `Q` 退出。
- 图形版：运行 `MatchGameRaylib.exe`，点击一个方块，再点击相邻方块尝试交换；再次点击同一个方块取消选择，点击不相邻方块改变选择。无效交换播放回退动画，有效交换播放消除、下落、补充及连锁动画。关闭窗口或按 Esc 退出。

图形版 1.0 是初始演示版本，昵称固定为 `DemoPlayer`，消除长度固定为 3，尚未提供昵称和 k 的设置界面。命令行版保留现有输入逻辑；自动提示、无解检测和洗牌尚未实现。

## 游戏规则

棋盘为 20 行、30 列，共九种物品。横向或纵向连续数量达到 k 时消除整串，每个物品计 10 分，交叉位置只计一次。消除后向下掉落并随机补充，自动处理连锁。累计达到 300、700 分时升级，达到 1200 分时胜利，显示累计用时。

## 从源码编译

需要 Visual Studio 2022 的“使用 C++ 的桌面开发”工作负载、v143 编译工具和 Windows SDK。源码和注释采用 UTF-8。

打开 `Matchgame.sln`，选择 `Release | x64`，生成解决方案。两个 EXE 输出在 `Matchgame/x64/Release/`。raylib 项目已纳入解决方案并通过项目引用自动构建。

也可在 VS2022 Developer PowerShell 中从仓库根目录执行：

```powershell
msbuild .\Matchgame\Matchgame.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m
```

`raylib-sdk/vendor/raylib/` 保存本地开发所用的 raylib `6.1-dev` 源码快照及许可证，编译时无需重新下载上游 master。第三方源码保持原样，授权信息见其 `LICENSE` 和各源码文件头部。

## 发布时的构建调整与验证

相对于用户本地项目，发布副本补充了图形版的 `raymath.h` 引用，修正 raylib 与 Windows 库的链接顺序，将依赖路径改为仓库内的 vendor 路径，并补全 Release 的 UTF-8、静态运行库和依赖自动构建配置。游戏规则未作重构，原本地文件保持原样。

2026-10-13 发布检查：两个项目的 Release x64 构建成功；命令行版检查了三种 k、错误 k、非法坐标和方向、Q 退出；raylib 版检查了窗口与 OpenGL 初始化、正常关闭，退出码为 0。EXE 依赖检查未发现 raylib DLL 或 MSVC 动态运行库依赖。图形鼠标动画与完整通关过程未进行自动化验收。

构建仍存在图形坐标 int 到 float 转换警告及 raylib 第三方源码警告，未为消除警告改变游戏行为。
