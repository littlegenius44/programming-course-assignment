# Desktop Algorithm Demo Implementation Plan

**Goal:** 在现有四个业务文件中实现两个 C 入口共用的算法阶段展示与演示模式。

**Architecture:** game.c 每次推进一个实际阶段并保存说明数据，两个入口只负责输入和展示。保留原二维数组和固定计分，使用回合快照支持取消恢复。

**Tech Stack:** C、VS2022、Windows、现有 raylib。

## Task 1: 验证接口与核心算法

- [x] 在隔离目录 Matchgame/x64/verification/algorithm_tests.c 写正式断言：初始化稳定且可玩、横纵交叉只计一次、阶段停留、无效回退、取消恢复、正常与单步终态一致。
- [x] 先使用原 game.c 编译，确认新阶段接口缺失导致测试不能通过。
- [x] 修改 game.h，声明 GameStage、MatchResult、CellChange、RoundExecution，以及 beginRound、advanceRound、cancelRound、findLegalMove、setGamePaused。
- [x] 修改 game.c，统一扫描、去重、下落、补充、计分和阶段切换；移出 printf，保留 playMove 包装接口。
- [x] 在 VS2022 Developer 环境执行 `cl /nologo /utf-8 /W4 /TC algorithm_tests.c game.c /Fe:algorithm_tests.exe`，运行后要求全部断言通过。

## Task 2: 命令行入口

- [x] 修改 main.c，使用 fgets 和 strtol 严格读取昵称、k、模式及行列方向。
- [x] 增加阶段棋盘、标记、匹配段、下落与补充说明；等待函数只接受回车或单个 Q。
- [x] 加入 H、R、Q，非法及超长输入不改变棋盘，EOF 正常结束；演示中断调用 cancelRound。
- [x] 将 tester、3、2、H、提示坐标和方向、空行序列输入进程，要求在每个阶段等待且退出汇总正确。

## Task 3: raylib 入口

- [x] 修改 gui_main.c，新增设置页、正常/演示切换、下一步、自动播放和暂停按钮，不新增业务文件。
- [x] 使用共享 MatchResult 绘制匹配标记，使用共享 CellChange 插值绘制下落、交换和补充。
- [x] 通过核心 advanceRound 推进，按钮和重绘不能自行结算；活动回合不能接受新交换。
- [x] 设置与提示均有效，退出或重开未完成回合时恢复；正常模式与演示模式必须共用相同规则。

## Task 4: 构建与交付

- [x] 使用 `MSBuild Matchgame/Matchgame.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m`，Debug 也须通过。
- [x] 验证两个入口启动、错误输入、正常退出，图形设置页、H、单步、自动播放、暂停、重开与结果页。
- [x] 补充算法复杂度、操作方法和验收结果到 Matchgame/README.md。
- [x] 将验证后的四个业务文件回写原目录，项目配置仅修正编译所需设置；备份保留，不删除用户文件。
- [x] 仅提交隔离分支的本次改动，不推送，不改变 1.0 标签或 Web 代码。

## 验收结果（2026-10-14）

核心断言、命令行真实进程、raylib 窗口消息操作、隔离与原目录 Debug/Release x64 均通过。命令行追加了 FINISH 阶段停留的失败后通过回归，清屏后持久保留上一条反馈。代码复核未发现遗留问题。

代码及操作说明已回写原目录；备份和隔离目录保留。测试程序只存在隔离目录的已忽略 x64/verification，不进入课程工程或提交。原目录既有的网页版及课程文档改动未触碰。仅保留本地开发分支，不合并、不推送、不发布新 Release。
