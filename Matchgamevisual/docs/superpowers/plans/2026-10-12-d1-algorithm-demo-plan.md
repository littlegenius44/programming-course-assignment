# D1 算法演示实施计划

> 使用executing-plans在当前分支逐项实现。设计已由用户确认并要求开始，不重复请求批准，不删除文件，不创建可视化辅助。

**目标：** 同一次真实结算可按普通或演示设置播放，演示增加慢速、高亮与空格观察时间，显示阶段、真实匹配及计分数据。

**架构：** 不修改Core与步骤格式。StepPlayer保存播放偏好并在每步开始时生成独立时长配置，BoardScene统一应用到位移、特效、落地及观察等待。Controller展示当前步骤说明和最近一次真实CLEAR摘要，原生页面展示演示控制与简洁说明栏。

**技术栈：** 现有TypeScript、Phaser4、Vite和Vitest，不新增依赖。

## 确认范围

- 默认普通模式；演示有正常1×、慢速0.5×、很慢0.25×。关闭演示恢复普通时长，保留所选演示速度供再次开启。
- 演示基准匹配高亮650ms、空匹配检测300ms、消除后空格450ms；其他基础动画沿用现值。所有时长按速度一起调整，包括爆炸与落地反馈。
- 改设置从下一步骤生效，本步及其多个动画段共享一份捕获配置；暂停/后台、计时、取消与操作锁沿用已有约定。
- 说明栏展示交换、匹配、消除、下落、补充、再检测。MATCH说明真实匹配段数与去重格数，CLEAR展示真实轮次、消除数、每格分值及增量，GRAVITY/REFILL说明真实物品数量。
- CHAIN只由已经发生的CLEAR更新，不把空MATCH的检测轮次当成一次新连锁。稳定后保留本回合最后CLEAR摘要，下一交换开始清空。
- 说明和中间状态来自现有真实步骤；普通/演示结果相同。动画计入时间，暂停和后台不计入。

## 任务1：播放配置与串行契约

文件：新增`src/presentation/playback.ts`及`tests/demo.test.ts`，修改`StepPlayer.ts`、`tests/playback.test.ts`。

- [x] 先写普通/三档演示时长、非法速度拒绝、四轮步骤相同及设置从下一步生效的正式测试并看到失败。
- [x] 实现PlaybackOptions、PlaybackTiming与timingFor；StepRenderer新增第三个时长参数，StepPlayer每步捕获配置。
- [x] 运行`npm run test -- tests/demo.test.ts tests/playback.test.ts`，预期通过。

契约示例：

```typescript
player.setOptions({ demo: true, speed: 0.5 });
expect(timingFor({ demo: true, speed: 0.5 }).scale).toBe(2);
// 已启动步骤保留原timing，下一步骤收到新timing。
```

## 任务2：真实步骤说明与模型

文件：新增`src/presentation/explanation.ts`，修改`src/app.ts`及正式控制器测试。

- [x] 写横纵交叉5格50分、空MATCH不虚增CHAIN、四轮30/60/90/90及重开/退出清空摘要测试。
- [x] 实现`explainStep(step)`；Controller保存当前说明和四字段CLEAR摘要，设置通过播放器校验后发布。
- [x] 设置跨重新开始保留；故障、开始、新交换、退出和返回开始页清理过期说明与摘要。
- [x] 类型检查与回归测试通过。

## 任务3：棋盘与页面接入

文件：修改`BoardScene.ts`、`src/ui/page.ts`、`src/main.ts`、`index.html`、`src/ui/style.css`。

- [x] BoardScene对全部基础动画应用本步scale，MATCH使用相应观察时长，CLEAR真正删除物品后等待空格观察时间；等待使用现有Tween和AbortSignal。
- [x] 新增演示复选框与三档选择，动画中可修改，页面明确从下一步生效；普通模式禁用演示速度选择。
- [x] 棋盘下方增加紧凑说明栏、六阶段条及真实CLEAR数据卡；默认隐藏，使用textContent与aria-current展示。
- [x] 演示时适当预留页面高度，验证桌面完整棋盘和小屏无横向溢出。

## 任务4：浏览器与交付

- [x] 普通与三档演示采用同一夹具与补充序列，四轮终态270分完全一致；实际测出匹配/空格等待及所有动画按速度调整。
- [x] 空格观察期暂停/后台冻结，改速本步不被截断；退出、重开、关卡与胜利没有旧说明、特效或计分残留。
- [x] 交叉5格50分与k4四格40分真实显示，空MATCH后仍显示最后真实CHAIN轮次。
- [x] 使用已有Playwright CLI和Edge无头检查DOM与实际对象；不截图、不建立临时脚本。生产preview验证设置和基本流程，普通控制台无错误。
- [x] 按requesting-code-review进行只读复核，处理重要问题。`npm run test`、`npm run typecheck`、`npm run build`通过。
- [x] 更新README及总体路线D1状态，保留已有Phaser包体积提示与生成素材说明；D2正式素材与交付后续实施。
