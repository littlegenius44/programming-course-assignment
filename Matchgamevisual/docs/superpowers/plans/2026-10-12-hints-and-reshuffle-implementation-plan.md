# 可用操作、自动洗牌与免费H提示实施计划

> 状态：用户确认整体实施后，已使用executing-plans在当前分支完成五个检查点（2026-10-13）。规则以已批准[设计方案](../specs/2026-10-12-hints-and-reshuffle-design.md)为准。D2课程交付继续推迟，不执行打包、发布、产物清理或交付版本处理。

**目标：** 保持k=3/4，保证新棋盘至少有一个有效操作；死局自动重新生成并保留成绩与用时；提供免费H提示。

**架构：** Core提供只读的相邻交换查找、可玩生成器及保留元数据的洗牌状态。Controller在完整回合提交后进入独立洗牌阶段，Phaser按SHUFFLE快照播放；提示只读取稳定棋盘并高亮坐标。继续使用既有暂停、演示时长和取消接口。

**技术栈：** 现有TypeScript、Phaser4、Vite、Vitest、HTML/CSS；不新增依赖。

## 执行约束与基线

- 本次仅实现自动洗牌与免费提示。k5、提示次数限制、最优操作推荐及D2另行安排。
- 已有验收基线为96项正式测试；原死局行为的应用测试需要按新规则更新，其他核心、计分、播放、计时与演示断言保留。
- 保留当前项目目录和分支，不修改同级C版或课程材料。所有注释UTF-8；不删除文件，不制作截图、可视化辅助或一次性测试脚本。
- 开局与换关使用可玩生成器；补充函数仍正常随机并允许连锁。洗牌没有CLEAR步骤，不额外计分。
- 重新开始仍将分数与时间归零；自动洗牌保留昵称、k、分数、关卡、累计用时和播放偏好。
- 每个检查点先写正式测试并看到预期失败，再实现、类型检查和验证；浏览器检查仅针对本次功能，不能将D2提前标为完成。

## 文件与职责

| 文件 | 本次职责 |
| --- | --- |
| `src/core/types.ts` | 增加只读LegalMove以及SHUFFLE步骤 |
| `src/core/moves.ts`（新增） | 枚举相邻交换并调用已有匹配扫描；只依赖types/match |
| `src/core/board.ts` | 共用候选初始化，新增有限重试及保证操作图案的可玩生成 |
| `src/core/game.ts` | 对外查找接口、保留元数据的reshuffleGame、新局/换关使用可玩生成 |
| `src/app.ts` | 提交回合、检测死局、独立洗牌、提示入口与中止语义 |
| `src/presentation/StepPlayer.ts` | 切换播放队列时保留暂停状态 |
| `src/presentation/playback.ts` | 增加洗牌提示的基准观察时长，仍按本步倍率统一缩放 |
| `src/presentation/BoardScene.ts` | SHUFFLE过渡、提示高亮和清理 |
| `src/presentation/explanation.ts` | SHUFFLE真实阶段说明 |
| `src/ui/page.ts`、`src/main.ts`、`index.html` | H与按钮的同一入口、启用条件及键盘过滤 |
| `tests/moves.test.ts`、`tests/playable-board.test.ts`（新增） | 查找与保证可玩生成的独立正式测试 |
| 现有核心、应用、播放与演示测试 | 按新规则补充生命周期契约；保留历史规则覆盖 |

## 检查点1：共用有效操作查找

修改types/game，新增moves与正式测试。

- [x] 在types中定义数据接口：

```typescript
export interface LegalMove {
  readonly from: Position;
  readonly to: Position;
}
```

- [x] 写有解、无解、k3/k4、边界和输入不变测试。用20×30拉丁式棋盘`(row + col) % 9 + 1`构造稳定死局；修改少量格子构造确定的边界有效交换。
- [x] 将模拟算法迁移到moves.ts，不依赖board.ts，避免生成器与查找器之间循环引用。实现以下完整查找逻辑；调用方保证棋盘稳定：

```typescript
export function findAvailableSwap(board: Board, k: MatchLength): LegalMove | null {
  const work = board.map(row => [...row]);
  for (let row = 0; row < BOARD_ROWS; row++) {
    for (let col = 0; col < BOARD_COLS; col++) {
      for (const to of [{ row, col: col + 1 }, { row: row + 1, col }]) {
        if (to.row >= BOARD_ROWS || to.col >= BOARD_COLS) continue;
        const first = work[row][col]!;
        const second = work[to.row][to.col]!;
        if (first.kind === second.kind) continue;
        work[row][col] = second;
        work[to.row][to.col] = first;
        const rows = to.row === row ? [work[row]] : [work[row], work[to.row]];
        const columns = to.col === col ? [work.map(line => line[col])]
          : [work.map(line => line[col]), work.map(line => line[to.col])];
        const matched = findMatches(rows, k).cells.length > 0 || findMatches(columns, k).cells.length > 0;
        work[row][col] = first;
        work[to.row][to.col] = second;
        if (matched) return Object.freeze({
          from: Object.freeze({ row, col }), to: Object.freeze({ ...to }),
        });
      }
    }
  }
  return null;
}
```

- [x] game.ts对外新增`findLegalMove(state: GameState): LegalMove | null`：won返回null，其余先调用现有validateStableState，再调用findAvailableSwap。
- [x] `hasLegalMove`保留接口，仅返回`findLegalMove(state) !== null`。不调用resolveMove，不消费随机数。
- [x] 正式测试验证返回位置相邻、实际交换可消除、交换检测后JSON和物品身份保持原样、重复查询返回相同位置、won无结果。

运行：`npm run test -- tests/moves.test.ts tests/core.test.ts`、`npm run typecheck`。预期查找测试和既有核心规则通过；这一步仍不改变页面死局处理。

## 检查点2：保证新棋盘可玩

修改board/game，新增生成测试，更新旧应用死局测试。

- [x] 将createBoard内部初始化循环整理为一个私有buildBoard，保留公开createBoard的参数、结果和原随机消费规则。私有函数只增加可选的两种植入物品参数，普通生成不使用它。
- [x] 新增`MAX_PLAYABLE_ATTEMPTS = 3`与`createPlayableBoard(k, random, nextTileId = 1)`：前三次依次调用createBoard并查找有效操作，返回首个成功结果。
- [x] 三次失败后随机选不同种类A、B；填充时前两行、前k列采用下列确切赋值，其余位置仍从原有合法候选中选取：

```typescript
const forced = pattern && row < 2 && col < matchLength
  ? row === 0
    ? col < matchLength - 1 ? pattern.a : pattern.b
    : col < matchLength - 1 ? pattern.b : pattern.a
  : null;
const kind = forced ?? candidates[randomIndex(random, candidates.length)];
if (!candidates.includes(kind)) throw new Error('可玩图案与初始化约束冲突。');
board[row][col] = { id: nextTileId++, kind };
```

其中pattern.a从1～9选取，pattern.b从排除a的8个值中选取。植入格不消费候选随机数；选择a/b消费两次。禁止在已生成棋盘上直接改几格。

- [x] 最后校验无初始匹配且有有效交换；若兜底契约失败则抛错，不返回无操作棋盘。丢弃候选不发布id，所有候选从同一个传入nextTileId开始，返回棋盘的下一id为nextTileId+600。
- [x] createGame和resolveMove的LEVEL_CHANGE生成改用createPlayableBoard；refillBoard保持原规则。
- [x] 新增`reshuffleGame(state, random): GameState`：拒绝won状态且不消费随机数，验证稳定输入，再生成新棋盘并通过现有snapshot冻结。返回状态保留score/level/nickname/matchLength/status，只更新board/nextTileId。
- [x] 用周期随机源强制三次生成死局，检验兜底有限结束、植入图案确实可交换、600格完整、id唯一、无初始匹配。补充固定值与固定种子测试。
- [x] 将旧应用测试“死局只能整局重开”调整为“同一周期随机源开局也能生成可玩棋盘”；保留纯查找接口对死局返回null的测试。

验收断言：

```typescript
expect(findMatches(generated.board, k).cells).toHaveLength(0);
expect(findAvailableSwap(generated.board, k)).not.toBeNull();
expect(new Set(generated.board.flat().map(tile => tile!.id)).size).toBe(600);
expect(generated.nextTileId).toBe(startId + 600);
expect([after.score, after.level, after.nickname, after.matchLength])
  .toEqual([before.score, before.level, before.nickname, before.matchLength]);
```

运行：`npm run test -- tests/playable-board.test.ts tests/core.test.ts tests/app.test.ts`、`npm run typecheck`、`npm run build`。

## 检查点3：自动洗牌与独立提交

修改types、app、StepPlayer、BoardScene和explanation，扩充正式播放/应用测试。

- [x] 增加步骤：

```typescript
| { readonly kind: 'SHUFFLE'; readonly reason: 'no-legal-move' }
```

GameStep现有index/after继续沿用。应用为独立洗牌队列构造index=0，after必须由reshuffleGame生成并冻结；不加入CLEAR或新的计分入口。

- [x] 写“已完成回合先提交，再洗牌”“洗牌中退出保留新得分”“洗牌失败保留已提交成绩”“won不洗牌”“时间不重置”的失败测试。
- [x] StepPlayer.play在开始新队列时保留原paused值。显式cancel仍解除暂停以支持重开/退出；新play保存paused后取消旧队列，再恢复该暂停值，最后进入现有等待门。正式测试验证暂停状态跨两段播放连续保留。
- [x] 应用增加reshuffling标记，保持busy直到检查及必要洗牌完成。完整回合提交后：won直接结果；有操作恢复输入；无操作进入同一私有洗牌方法。阶段衔接中不发布可操作状态。
- [x] start、返回开始页、退出和当前会话的故障恢复清除reshuffling；restart通过start清除。旧会话的finally或完成回调不得写新会话标记，所有写入受现有generation校验约束。
- [x] 自动洗牌先发布提示与输入锁，再调用reshuffleGame并播放SHUFFLE。完成且会话编号仍有效后提交新棋盘，清除deadlocked与reshuffling，恢复输入；clock不调用start或reset。
- [x] 退出的interrupted只表示未完成的消除回合：`busy && !reshuffling`。洗牌中退出保留原稳定提交状态，结果页不能说已完成回合的分数被丢弃。
- [x] 洗牌故障恢复已提交棋盘并暂停，提示“棋盘未能重新生成，已保留成绩，请重新开始或退出”；原消除故障仍沿用上一稳定状态。取消后不触发故障弹窗。
- [x] PlaybackTiming增加shuffleNoticeMs：普通500ms、演示1000ms；更新timingFor与正式时长断言。SHUFFLE显示旧稳定棋盘与提示，再用140ms淡出旧物品、根据after重建棋盘、140ms淡入新物品。所有段使用当前捕获的timing.scale和已有animate；过渡间检查AbortSignal，结束后断言数量、id、kind、坐标、alpha及显示大小。
- [x] showState、洗牌、重开和场景销毁入口先清理既有选中与特效；检查点4创建提示图形后，将其接入这些清理入口。新步骤说明phase=null，标题“自动洗牌”，说明保留成绩并重生成可玩棋盘；不改最近CLEAR轮次。
- [x] 开局、换关通过可玩生成器直接得到可玩状态；正常操作后检测死局，胜利不再生成。检测不进入每帧tick。

运行：`npm run test -- tests/app.test.ts tests/playback.test.ts tests/core.test.ts tests/demo.test.ts`、`npm run typecheck`、`npm run build`。用可控渲染器停在SHUFFLE并执行pause/quit/restart，检查旧回调不提交、不解锁新局。

## 检查点4：免费H提示

修改GameRenderer、BoardScene、app、PageUI、main和index，补应用与浏览器验证。

- [x] GameRenderer增加`showHint(move: LegalMove | null): void`；正式测试渲染器记录最近提示，兼容既有测试。
- [x] Controller新增`requestHint(): Promise<void>`，只允许game/playing、非busy、非paused、非faulted状态。调用findLegalMove，有结果仅显示提示与高亮；null进入同一自动洗牌方法，不返回旧坐标。
- [x] 提示文本固定按返回的from转为1基坐标；to.col较大为向右，to.row较大为向下。示例格式“提示：第3行第5列向右交换，可以产生消除。”
- [x] BoardScene复用一个独立hintOutline绘制两格，showHint先清除半次选择，避免下一次点击意外执行旧选择。高亮在下一次棋盘点击、交换、洗牌、重开或销毁时清理；不增加定时器。
- [x] index加入“提示（H）”按钮，PageUI将按钮与keydown连接到同一hint动作，main调用requestHint并由控制器处理异常。
- [x] H/h都支持；忽略repeat、Ctrl/Alt/Meta组合。文本/数值input、textarea、select以及contentEditable目标保留原生行为；checkbox和按钮聚焦时仍允许快捷键。PageUI.dispose移除全局监听。
- [x] 按钮启用条件与Controller一致，动画/暂停/洗牌/结束/故障时禁用。提示免费不限次数，正常提示不调用随机源、不交换、不修改GameState或累计分数；实际经过的时间正常累计。

必要断言：

```typescript
const state = app.model.game;
const randomCalls = calls;
await app.requestHint();
expect(app.model.game).toBe(state);
expect(calls).toBe(randomCalls);
expect(renderer.hint).not.toBeNull();
```

运行：`npm run test -- tests/app.test.ts tests/moves.test.ts`、`npm run typecheck`、`npm run build`；浏览器验证按钮/H结果一致、坐标方向、高亮清理、提示后的真实交换，以及昵称输入H/组合键/重复键的行为。

## 检查点5：本次功能验收与文档同步

- [x] 使用正式夹具和可控随机源检查k3/k4死局→自动洗牌→提示→实际交换；固定及周期来源下新棋盘完整、无匹配、可操作，旧分数/关卡/昵称/规则/用时不归零。
- [x] 普通模式和三档演示得到相同洗牌终态；暂停/后台冻结、返回等待继续、中途重开/退出、洗牌失败都保留正确已提交成绩，无旧图形或Tween残留。
- [x] 在1366×768、1920×1080及390宽度检查新增按钮不溢出；生产preview验证新流程、资源和控制台健康。仅做本次变更所需适配，不提前执行D2交付清单。
- [x] 检测与生成浏览器实测：记录普通查询、死局扫描及强制兜底耗时；以实际卡顿证据决定是否需要局部扫描或让生成分批让出主线程。保持3次重试上限，不加入无限循环。
- [x] 按requesting-code-review只读复核，修复重要问题，再运行全部测试、类型检查和构建。记录新增后的实际测试数量，不预填通过数。
- [x] 更新PROJECT_BRIEF当前范围、README和现行方案的死局/提示规则，保留原阶段验收记录。原“检测不改变状态”的核心测试继续保留；应用侧旧“死局整局重开”行为按本次已批准要求替换。
- [x] 报告本次功能完成情况、测试和浏览器证据。D2保持推迟状态，正式美术、交付打包、发布及产物清理不在本轮执行。

## 2026-10-13执行与验收记录

五个检查点均完成，122项正式测试、类型检查、生产构建通过；新增查找与可玩棋盘两组测试，扩充会话、暂停与演示测试。基础createBoard保持600次随机消费；三次失败后兜底最多再消费2+600-2k次，最终编号从传入nextTileId连续发布。

实际Edge验收覆盖k3四轮后保留290分、k4第三关保留800分、自动洗牌及提示后真实交换；普通及三档演示终态由正式测试对照。洗牌普通/演示1×/0.5×/0.25×播放均通过，暂停/后台排除的时间仍正确；淡出中退出保留40分，淡入中重开恢复0分且旧回调不提交，故障恢复最近已提交状态。结束时600物品、603显示对象、0装饰及0残留Tween。

H与按钮同一结果、1基坐标和方向、高亮与半次选择清理、文本/数值输入、选择框、contentEditable、组合键/重复键过滤及监听释放均验证；无操作时提示入口进入同一洗牌。三个窗口尺寸均无横向溢出，1366×768演示说明底部约765px。生产preview提示→真实交换→暂停→退出→重开通过，三张图片200，控制台错误/警告0。

性能验证发现全盘模拟死局扫描约94ms、三次失败兜底约283–287ms，因此按已批准性能条款改为复用findMatches扫描受影响完整行列。列转为行后最多两条线小于k，稳定输入保证新匹配只涉及交换格所在行列；与完整模拟首结果对照一致。相同浏览器夹具扫描约19–21ms、兜底约55–56ms、固定兜底棋盘提示平均约0.2ms。只读复核原实现及优化均未发现重要缺陷。

规则和README、设计、架构、路线状态已同步；未新增依赖、临时脚本、截图、可视化辅助、删除操作或提交。D2交付、打包、发布及正式美术定稿继续推迟。

## 验收判定

稳定死局可自动恢复为可玩棋盘，洗牌不丢失已完成回合成绩和用时；H与按钮给出同一类真实可执行提示而不改变规则状态；暂停、取消、演示及既有计分规则持续成立。全部正式测试、类型检查、构建及本次实际浏览器验收通过后，这个新增阶段才能标记完成。
