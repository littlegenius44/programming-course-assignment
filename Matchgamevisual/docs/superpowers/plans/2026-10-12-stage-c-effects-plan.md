# MatchLab C部分：素材替换与消除反馈实施计划

> 使用 executing-plans 在当前目录逐项实施。用户已确认总体方案并要求继续C，不重复请求确认；保留当前分支，不删除文件，不创建可视化辅助。

**目标：** 使用已有生成图片完善素材接入、轻消除、连锁爆炸和真实下落的落地反馈，保持规则与步骤终态一致。

**架构：** Core与StepPlayer接口保持不变。新增纯效果位置选择函数和小型Effects对象管理器；BoardScene将位移及装饰Tween放入现有等待/取消生命周期。所有效果只读取真实步骤，不能消除格子或计分。

**技术栈：** 现有TypeScript、Phaser4、Vite、Vitest；不新增依赖、图片或物理引擎。

## 已确认设计与取舍

1. 继续使用果园背景和九种水果图集。用户正式素材尚未提供，本轮完成现有素材的整理与替换验证，正式美术最终定稿待用户提供图片。
2. 使用已有透明`burst.png`缩放淡出；比重新生成帧图或引入粒子系统更适合当前阶段。第1轮用轻量圆环，第2轮起用爆炸图，第3轮以上强度封顶。
3. 每条真实匹配段选其中实际被消除的一格为效果中心，交叉中心去重，每轮最多12处。全部真实消除格子仍正常消失，限制只针对装饰。
4. 只有GRAVITY.moves和REFILL.spawned中的物品获得55ms压缩+55ms恢复；不改变坐标、身份或最终显示大小。
5. Effects只管理装饰对象并返回Tween配置；BoardScene原有animate负责完成、暂停、AbortSignal取消。每批效果有独立dispose，避免旧播放清理新局对象。
6. 集中配置特效开关与爆炸图片路径。缺图使用运行时圆环纹理；关闭装饰只减少表现，不改变核心步骤、得分或终态。
7. 算法演示开关和速度档位按最新总体路线留给D1；本轮不增加其他玩法或页面流程。

## 任务1：先验证效果中心与强度契约

文件：新增`tests/effects.test.ts`、`src/presentation/effectPlan.ts`。

- [x] 写真实resolveMove生成的四轮夹具测试，断言CLEAR增量为30/60/90/90，效果中心均在removed内，读取函数不改变步骤。
- [x] 写交叉匹配中心去重、大范围匹配最多12处、禁用时零效果、圆环/爆炸轮次分界测试。
- [x] 运行`npm run test -- tests/effects.test.ts`，确认缺少实现导致失败。
- [x] 实现`planClearEffects(clear, segments, enabled=true)`，返回`centers`、`style`及显示直径，中心依据真实匹配段选择，强度在第3轮封顶。
- [x] 重跑效果及既有测试，预期全部通过。

测试接口示例：

```typescript
const plan = planClearEffects(clear, matches.segments);
expect(plan.centers.every(p => clear.removed.some(x => x.position.row === p.row && x.position.col === p.col))).toBe(true);
expect(planClearEffects(clear, matches.segments, false).centers).toEqual([]);
```

## 任务2：管理真实Phaser装饰并接入步骤

文件：新增`src/presentation/Effects.ts`；修改`BoardScene.ts`、`assets.ts`。

- [x] 在assets中配置`BURST_ASSET`和`VISUAL_EFFECTS`，加载已有PNG，保留BASE_URL。
- [x] Effects生成可复用圆环备用纹理，createClear返回当前批次的Tween配置及dispose；clear逐个销毁自有显示对象。
- [x] MATCH保存真实segments，CLEAR合并物品淡出与装饰Tween，finally只清理本批；取消/重开/场景销毁清理全部效果。
- [x] GRAVITY和REFILL到位后对真实移动id进行yoyo缩放，并增加最终显示大小的快照检查。
- [x] 类型检查及测试通过，现有StepPlayer等待契约无需改变。

生命周期示例：

```typescript
const batch = effects.createClear(step, matchedSegments);
try { await animate([...itemChanges, ...batch.changes], duration, signal); }
finally { batch.dispose(); }
```

## 任务3：实际浏览器验证

使用已有缓存Playwright CLI和Edge无头浏览器。Browser插件不可用；遵照用户要求不截图、不制作测试页面或临时脚本，检测实际对象与DOM。

- [x] 固定四轮夹具观察每步对象、得分与snapshot校验，终态270分；开启/关闭装饰对照终态。
- [x] CLEAR爆炸进行中暂停，计时、位置、alpha和scale不变；恢复后同一回合完成。
- [x] 爆炸/落地进行中重开和退出，旧回调不更改成绩，装饰与Tween没有残留。
- [x] 大范围匹配所有格子正常消失，装饰最多12处；k4、交叉中心去重均正确。
- [x] 缺爆炸图、缺水果图集仍可玩；独立物品替换不改变规则。1366×768、1920×1080及390宽度检查无横向溢出。
- [x] 使用Vite子路径生产构建验证背景、图集、爆炸图请求与基础流程，构建保留旧文件。

## 任务4：验收与进度同步

- [x] `npm run test`、`npm run typecheck`、`npm run build`成功，记录实际数量及已有Phaser包体积提示。
- [x] 按requesting-code-review进行只读复核，修正重要问题后再验收。
- [x] 更新README、docs/assets.md和总体计划，区别生成素材接入完成与用户正式素材尚待定稿。
- [x] 保留正式回归测试和开发服务；无提交、发布、删除或自动下载。

## 实际验收结果

2026-10-12完成以上任务，85项正式测试、类型检查及根路径/子路径构建通过。四轮真实消除得270分，开启/关闭装饰终态相同；爆炸暂停重开、落地暂停退出、600格合成渲染、缺图回退、单图替换及k4均验证。Edge开启特效以正常动画速度完成32次交换、243步及两次换关，1210分胜利；结束时600物品、602显示对象、1个输入监听、0装饰及0残留Tween。生产子目录3张素材均200，基本流程控制台错误/警告0。只读审查未发现重要问题。当前使用生成图片，正式美术定稿仍待用户素材；D1/D2尚未实施。保留Phaser已有包体积提示，未新增依赖、制作截图或创建临时脚本。
