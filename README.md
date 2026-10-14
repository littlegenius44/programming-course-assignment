# 消消乐课程设计 · MatchLab

本项目包含网页版、C 命令行版和 C + raylib 图形版三个版本，可以在线游玩，也可以下载 Windows 程序运行。

**[在线游玩网页版](https://littlegenius44.github.io/programming-course-assignment/)** · **[命令行版 2.0 下载](https://github.com/littlegenius44/programming-course-assignment/releases/download/console-v2.0.0/Matchgame-console-v2.0.0-windows-x64.zip)** · **[raylib 版 2.0 下载](https://github.com/littlegenius44/programming-course-assignment/releases/download/raylib-v2.0.0/Matchgame-raylib-v2.0.0-windows-x64.zip)**

## 三个版本如何选择

| 版本 | 运行环境 | 操作方式 | 消除长度 k | 主要特点 |
| --- | --- | --- | --- | --- |
| 网页版 MatchLab | 浏览器 | 点击两个相邻格子 | 3 或 4 | 提示、自动洗牌、暂停、算法演示及变速播放 |
| 命令行版 2.0 | Windows x64 | 输入行、列与 WASD 方向 | 3、4 或 5 | 三档棋盘、真实算法阶段展示、回车单步、提示与统计 |
| raylib 图形版 2.0 | Windows x64，支持 OpenGL 3.3 | 鼠标点击两个相邻格子 | 3、4 或 5 | 三档棋盘、大窗口、算法面板、单步/自动/暂停 |

网页版保持20行、30列；两个C桌面版2.0可以选择10×10、20×15或30×20（列×行），棋盘大小与k独立。三个版本均有九种物品，支持横纵匹配、交叉去重、重力下落、随机补充、连锁、计分、计时和三关挑战。累计达到300、700分时升级，达到1200分时获胜。

计分存在版本区别：两个 C 桌面版本每消除一个物品固定得 10 分；网页版同次交换第 1 轮每格 10 分、第 2 轮每格 20 分、第 3 轮起每格 30 分。

## 网页版：直接在线游玩

打开 **[MatchLab 在线游戏](https://littlegenius44.github.io/programming-course-assignment/)**，无需下载安装。

1. 输入昵称，选择 `k=3` 或 `k=4`，开始游戏。
2. 点击一个格子选中，再点击上下左右相邻的格子交换。
3. 再点同一个格子取消选中；点击不相邻格子切换选中位置。
4. 无法产生匹配时自动交换回来；有效交换后自动播放消除、下落、补充和连锁。
5. 达到第三关目标后显示最终成绩，也可以主动退出或重新开始。

其他操作：

- **提示**：按 `H` 或点击“提示（H）”，显示一个有效交换并高亮两格。提示免费，不自动执行交换。
- **自动洗牌**：棋盘无有效交换时自动生成新的可玩棋盘，保留成绩和游戏设置。
- **暂停/继续**：使用界面按钮。暂停或切到后台时停止计时和动画；回到页面后点击“继续游戏”。
- **算法演示**：勾选“算法演示”，观察交换、匹配、消除、下落、补充和再次检测的步骤说明，可选择 `1×`、`0.5×` 或 `0.25×`。
- **重开/修改设置**：重新开始保留昵称和 k，清零分数和时间；返回开始页面可以修改设置。

动画、洗牌和暂停期间不能继续交换。

## 命令行版 2.0：下载并运行

下载 **[Windows x64 ZIP](https://github.com/littlegenius44/programming-course-assignment/releases/download/console-v2.0.0/Matchgame-console-v2.0.0-windows-x64.zip)**，解压后运行 `Matchgame.exe`。无需安装VS2022。

1. 输入昵称。
2. 选择棋盘：`1` 小型10×10、`2` 中型20×15、`3` 大型30×20（列×行）。
3. 独立选择消除长度 `3`、`4` 或 `5`。
4. 选择 `1` 正常模式或 `2` 演示模式。
5. 输入 `行 列 方向`，例如 `3 5 D`；坐标范围由当前棋盘决定。

W/A/S/D表示上/左/下/右，支持小写。H提示有效交换，R按原设置重开，Q结束本局。
正常模式自动执行完整回合；演示模式逐阶段展示匹配段、交叉去重、空位、得分及真实下落/补充记录，按回车才推进下一阶段（含回合结束）。

演示期间输入Q会取消未完成回合并恢复棋盘与成绩；已完成回合不会撤销。结束界面R重开、N修改设置、Q退出程序。超长、越界和格式错误输入会被拒绝。

建议从解压目录的PowerShell启动，以保留结束统计：

```powershell
.\Matchgame.exe
```

[命令行2.0发布说明与SHA-256校验文件](https://github.com/littlegenius44/programming-course-assignment/releases/tag/console-v2.0.0)。

## raylib 图形版 2.0：下载并运行

下载 **[Windows x64 ZIP](https://github.com/littlegenius44/programming-course-assignment/releases/download/raylib-v2.0.0/Matchgame-raylib-v2.0.0-windows-x64.zip)**，解压后运行 `MatchGameRaylib.exe`。无需安装VS2022或raylib；显卡驱动需要支持OpenGL 3.3。

设置页输入昵称，独立选择三档棋盘、k=3/4/5和正常/算法演示模式，再开始游戏。

- 点击一个格子选中，再点击相邻格子交换；再次点击同格取消，不相邻格切换选择。
- Space/下一步：演示模式只推进一个真实算法阶段。
- A/自动播放：演示阶段自动推进或停止。
- P/暂停：暂停播放与计时；窗口失焦会自动暂停，返回后需继续。
- H/提示：稳定棋盘上高亮一个有效交换，不自动操作或计分。
- R/重开：按原昵称、大小和k开始新局；结束页可返回设置。
- 结束本局：显示统计，并恢复未完成回合；关闭窗口或Esc退出程序。

棋盘旁的算法面板展示匹配段、去重、连锁、得分与实际变化。界面以1600×1000为逻辑布局，放大格子、数字、说明与按钮；初始窗口适配Windows工作区，窗口调整后绘制和鼠标点击同步等比缩放。

图形版使用本机Windows中文TTF字体，不打包或下载字体；运行包附带raylib与GLFW许可证。

[raylib 2.0发布说明与SHA-256校验文件](https://github.com/littlegenius44/programming-course-assignment/releases/tag/raylib-v2.0.0)。

## 获取源码与本地开发

源码按分支分开管理：

- **`main`**：仓库主页说明、[网页版源码](Matchgamevisual/)和 GitHub Pages 部署工作流。
- **`release/desktop-v2.0`**：[命令行版与 raylib 版源码](https://github.com/littlegenius44/programming-course-assignment/tree/release/desktop-v2.0/Matchgame)，包含 VS2022 解决方案和 raylib 依赖源码。
- **`console-v2.0.0` / `raylib-v2.0.0` 标签**：分别对应两个桌面版 2.0 的发布记录，均指向同一桌面2.0发布提交。

桌面2.0分支未合并到网页版主分支，两个入口使用各自标签与运行包。main仅更新主页说明，Web源码与部署配置保持原样。

1.0历史版本仍保留：[命令行1.0](https://github.com/littlegenius44/programming-course-assignment/releases/tag/console-v1.0.0)、[raylib 1.0](https://github.com/littlegenius44/programming-course-assignment/releases/tag/raylib-v1.0.0)及[1.0源码分支](https://github.com/littlegenius44/programming-course-assignment/tree/release/desktop-v1.0)。

### 本地运行网页版

需要 Node.js `22.12+`（22.x）或 `24+`，建议使用 Node.js 24。

```powershell
git clone --branch main --single-branch https://github.com/littlegenius44/programming-course-assignment.git MatchLab-Web
Set-Location .\MatchLab-Web\Matchgamevisual
npm ci
npm run dev
```

打开终端显示的本地地址。检查与构建命令：

```powershell
npm run typecheck
npm run test
npm run build
npm run preview
```

更多说明见[网页版 README](Matchgamevisual/README.md)。

### 编译两个 C 桌面版本

需要 VS2022 的“使用 C++ 的桌面开发”工作负载、v143 编译工具和 Windows SDK。项目源码采用 C，注释使用 UTF-8。

```powershell
git clone --branch release/desktop-v2.0 --single-branch https://github.com/littlegenius44/programming-course-assignment.git Matchgame-Desktop
Set-Location .\Matchgame-Desktop
```

使用 VS2022 打开 `Matchgame/Matchgame.sln`，选择 **Release | x64**，生成解决方案。两个 EXE 输出到 `Matchgame/x64/Release/`，raylib 依赖自动构建，不需要再次下载。

也可在 VS2022 Developer PowerShell 中运行：

```powershell
msbuild .\Matchgame\Matchgame.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m
```

更多说明见[桌面版本 README](https://github.com/littlegenius44/programming-course-assignment/blob/release/desktop-v2.0/Matchgame/README.md)。
