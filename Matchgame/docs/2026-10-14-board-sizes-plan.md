# Board Size and Larger Window Implementation Plan

> 执行方式：当前会话按 executing-plans 顺序执行；不新增业务模块。

**Goal:** 两个C入口选择三档棋盘，并扩大raylib交互界面。

**Architecture:** 最大容量不变，Game记录当前尺寸；图形界面使用统一逻辑画布及鼠标反变换。

**Tech Stack:** C、VS2022、现有raylib、Windows系统字体。

## Task 1：共享核心

- [x] 在已忽略的 x64/verification/board_size_tests.c 添加三档、三k、30种子断言及未启用区域哨兵检查，先编译确认新接口缺失失败。
- [x] game.h 增加 boardSize/rows/cols、initializeGameWithSize 和 getBoardSizeName；保留 initializeGame 三参数兼容。
- [x] game.c 用 rows/cols 代替容量常量作循环和校验；shuffle计数改 rows*cols；所有快照仍复制完整容量。
- [x] 编译运行新测试和现有 algorithm_tests，要求全部断言通过。

## Task 2：命令行

- [x] main.c 设置增加1/2/3档选择，所有初始化/重开传档位，绘制与输入范围采用当前大小；首页和结果显示列×行。
- [x] 真进程测试三档×三k设置、错误档位重试、当前边界拒绝、H提示、正常回合、演示停留/取消与原设置重开。

## Task 3：raylib

- [x] gui_main.c 增加设置档位和统一设置按钮范围；保留现有全部快捷键和演示状态规则。
- [x] 逻辑画布设为1600×1000；boardLayout按当前大小计算36～60格子及居中坐标；数字、按钮与面板字体扩大。
- [x] RenderTexture绘制逻辑画布，保持等比居中；virtualMouse采用逆变换；初始窗口适配Windows工作区且可调整。
- [x] GUI真窗口消息检查三档选择、提示点击、单步、暂停、取消、重开、自动播放和缩放后点击；不使用截图辅助。

## Task 4：交付

- [x] 隔离/原目录 Debug、Release x64 构建通过，UTF-8与diff检查通过。
- [x] README补充尺寸和窗口操作说明；仅回写四个业务文件及本次文档，不触碰其他用户改动。
- [x] 仅在现有本地开发分支保存提交，不推送、不删除备份或测试文件。

## 验收记录（2026-10-14）

核心：270组尺寸/k/种子初始化、有效范围与域外哨兵、提示、交叉去重、下落/补充、正常/单步一致、取消、换关与重开均通过，原有算法回归通过。

命令行：原目录九种尺寸/k组合、非法档位与坐标、H提示、正常回合、两处重开、N返回设置、演示单步/取消及FINISH停留均通过。

图形：隔离目录三档完整消息操作及原目录小型回归通过；含1100×850缩放点击、留白、暂停、取消、重开、自动播放和真实工作区外框边界。测试驱动同步实际鼠标，避免硬件鼠标事件覆盖合成消息；临时输入诊断已移除。工作区纯测试先失败后通过，覆盖侧边任务栏、负坐标显示器与窗口边框。

隔离和原目录Debug/Release x64构建通过，业务代码无编译警告，UTF-8及文本同步检查通过。README已更新。原文件备份保留于E:/AI/.worktrees/matchgame-before-board-sizes-20261014；未修改Web/1.0，未下载、删除或推送。测试文件仅留在隔离目录已忽略x64/verification。
