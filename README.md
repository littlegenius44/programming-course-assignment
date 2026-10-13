# 消消乐课程设计 · MatchLab

本项目包含网页版、C 命令行版和 C + raylib 图形版三个版本，可以在线游玩，也可以下载 Windows 程序运行。

**[在线游玩网页版](https://littlegenius44.github.io/programming-course-assignment/)** · **[命令行版 1.0 下载](https://github.com/littlegenius44/programming-course-assignment/releases/download/console-v1.0.0/Matchgame-console-v1.0.0-windows-x64.zip)** · **[raylib 版 1.0 下载](https://github.com/littlegenius44/programming-course-assignment/releases/download/raylib-v1.0.0/Matchgame-raylib-v1.0.0-windows-x64.zip)**

## 三个版本如何选择

| 版本 | 运行环境 | 操作方式 | 消除长度 k | 主要特点 |
| --- | --- | --- | --- | --- |
| 网页版 MatchLab | 浏览器 | 点击两个相邻格子 | 3 或 4 | 提示、自动洗牌、暂停、算法演示及变速播放 |
| 命令行版 1.0 | Windows x64 | 输入行、列与 WASD 方向 | 3、4 或 5 | 昵称输入、字符棋盘，适合学习 C 游戏逻辑 |
| raylib 图形版 1.0 | Windows x64，支持 OpenGL 3.3 | 鼠标点击两个相邻格子 | 固定为 3 | 彩色数字棋盘，交换、回退、消除、下落和补充动画 |

三个版本均采用 20 行、30 列棋盘和九种物品，支持横向/纵向匹配、交叉去重、重力下落、随机补充、连锁、计分、计时及三关挑战。累计达到 300、700 分时升级，达到 1200 分时获胜。

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

## 命令行版 1.0：下载并运行

下载 **[Windows x64 ZIP](https://github.com/littlegenius44/programming-course-assignment/releases/download/console-v1.0.0/Matchgame-console-v1.0.0-windows-x64.zip)**，解压后运行 `Matchgame.exe`。无需安装 VS2022。

1. 输入昵称。
2. 选择消除长度 `3`、`4` 或 `5`。
3. 按提示输入 `行 列 方向`；行号范围为 `1～20`，列号范围为 `1～30`。

例如：

```text
3 5 D
```

表示将第 3 行第 5 列的物品与其右侧物品交换。`W`、`A`、`S`、`D` 分别表示上、左、下、右，支持小写。单独输入 `Q` 退出。

无法产生有效匹配时恢复交换，越界或非法方向会显示提示。有效交换会自动完成全部连锁，再等待下一次输入。

如果希望退出后保留终端中的最终成绩，在解压目录打开 PowerShell，执行：

```powershell
.\Matchgame.exe
```

[命令行版发布说明与 SHA-256 校验文件](https://github.com/littlegenius44/programming-course-assignment/releases/tag/console-v1.0.0)。

## raylib 图形版 1.0：下载并运行

下载 **[Windows x64 ZIP](https://github.com/littlegenius44/programming-course-assignment/releases/download/raylib-v1.0.0/Matchgame-raylib-v1.0.0-windows-x64.zip)**，解压后运行 `MatchGameRaylib.exe`。无需安装 VS2022 或 raylib，显卡驱动需要支持 OpenGL 3.3。

- 单击一个方块选中，再单击上下左右相邻方块交换。
- 再点同一个方块取消选择；点击不相邻方块切换选择。
- 无效交换自动播放回退，有效交换播放消除、下落、补充和连锁动画。
- 关闭窗口或按 `Esc` 退出。

**本版是初始演示版本：昵称固定为 `DemoPlayer`，消除长度固定为 `k=3`，尚未提供设置界面。** 提示、自动洗牌和暂停功能目前属于网页版，两个桌面版 1.0 尚未提供。

[raylib 版发布说明与 SHA-256 校验文件](https://github.com/littlegenius44/programming-course-assignment/releases/tag/raylib-v1.0.0)。运行包附带 raylib 许可证，程序不需要额外图片、字体或音频文件。

## 获取源码与本地开发

源码按分支分开管理：

- **`main`**：仓库主页说明、[网页版源码](Matchgamevisual/)和 GitHub Pages 部署工作流。
- **`release/desktop-v1.0`**：[命令行版与 raylib 版源码](https://github.com/littlegenius44/programming-course-assignment/tree/release/desktop-v1.0/Matchgame)，包含 VS2022 解决方案和 raylib 依赖源码。
- **`console-v1.0.0` / `raylib-v1.0.0` 标签**：分别对应两个桌面版 1.0 的发布记录，均指向本次桌面发布提交。

桌面发布分支未合并到网页版主分支，两个版本使用各自的发布标签和运行包。

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
git clone --branch release/desktop-v1.0 --single-branch https://github.com/littlegenius44/programming-course-assignment.git Matchgame-Desktop
Set-Location .\Matchgame-Desktop
```

使用 VS2022 打开 `Matchgame/Matchgame.sln`，选择 **Release | x64**，生成解决方案。两个 EXE 输出到 `Matchgame/x64/Release/`，raylib 依赖自动构建，不需要再次下载。

也可在 VS2022 Developer PowerShell 中运行：

```powershell
msbuild .\Matchgame\Matchgame.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m
```

更多说明见[桌面版本 README](https://github.com/littlegenius44/programming-course-assignment/blob/release/desktop-v1.0/Matchgame/README.md)。
