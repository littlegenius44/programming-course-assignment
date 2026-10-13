# 临时生成素材与替换说明

2026-10-12，按用户“图片先自行生成、之后提供替换素材”的要求，使用内置 imagegen 生成。文件原样复制到项目，未对PNG进行离线编辑。原始生成结果保留在Codex生成目录。

| 文件 | 实际规格 | 当前用途 |
| --- | --- | --- |
| `public/assets/backgrounds/orchard.png` | 1672×941，RGB，约2.47MB | 页面共用果园背景 |
| `public/assets/items/fruits.png` | 1254×1254，RGBA，约1.01MB | 九种水果图集，已接入棋盘 |
| `public/assets/effects/burst.png` | 1254×1254，RGBA，约0.91MB | 第2轮起的连锁爆炸，已接入缩放/淡出 |

水果与爆炸图包含真实透明通道。九种物品对应：1苹果、2橘子、3柠檬、4梨、5葡萄、6蓝莓、7草莓、8桃、9猕猴桃。程序额外叠加统一数字，图片中不含数字。

生成图集没有严格对齐等分网格。`src/presentation/assets.ts` 使用显式切片矩形，避免水果被截断；按种类生成九张包含底板、物品和数字的共享纹理，600个格子不各自创建文字纹理。切片范围只影响显示，不影响格子坐标或物品编号。

## 用户素材到位后的替换

背景路径集中在 `BACKGROUND_URL`。新增用户背景文件并修改配置即可，不需要改变页面结构；优先另存文件，避免覆盖这套临时素材。

单张物品配置包含 `kind`、`textureKey`、可选 `url/frame`、备用颜色。提供独立图片时，为对应条目设置独立的 `textureKey` 和本地 `url`，去掉图集 `frame`。九张图可分别替换；程序使用统一显示框并等比缩放，仍按 `kind` 选图、按 `id` 跟踪物品。正式图片的透明留白与主体占比需在收到图片后确认，当前生成版主题为果园。

例如将对应条目改为以下配置，并另存新图片到`public/assets/items/item-1.png`，保留现有生成图集：

```typescript
{
  kind: 1,
  textureKey: 'user-item-1',
  url: `${assetRoot}items/item-1.png`,
  fallbackColor: '#dc5b53',
}
```

`assetRoot`已使用Vite的`BASE_URL`；不要写死根目录地址。`textureKey`使用独立名称，省略`frame`即可按整张PNG加载。现有图片不需要删除或覆盖。

背景缺失时保留CSS底色，物品图加载失败或图集帧名错误时显示对应数字色块；加载故障不影响核心棋盘生成。单张独立PNG替换、错误帧名和图集缺失回退已实际浏览器验证。

爆炸路径集中在`BURST_ASSET`。当前使用单张静态透明PNG缩放淡出；用户提供连续帧图后，再按实际帧宽高、排列与帧数增加帧动画配置，不改计分或消除规则。本轮未修改或重新生成PNG。

## C部分的显示与生命周期

- 第1轮轻圆环，第2轮起使用爆炸图，第3轮起显示强度封顶。每条真实匹配段在本轮消除格中选择中心，相同中心去重，每轮最多12处；全部真实消除物品仍正常消失。
- 爆炸图缺失时回退到程序生成的共享圆环纹理。备用纹理仅在内存中创建，不写新图片文件。
- `GRAVITY.moves`与`REFILL.spawned`里的物品到位后进行55ms轻压缩、55ms恢复。只有比例变化，最终位置、id和30×30逻辑显示大小保持不变。
- 位移、装饰和落地Tween均由当前步骤等待，暂停一起冻结，取消一起停止。装饰按批清理，旧回调不能销毁新局效果；每步结束校验实际物品与`after`快照。
- `VISUAL_EFFECTS.enabled`集中控制装饰和落地反馈，默认开启。关闭后仍播放交换、高亮、物品消失、下落和补充；分数与终态相同，实际用时仍按播放时间累计。

C验收（2026-10-12）：85项正式测试及类型检查通过。实际四轮消除30/60/90/90分、终态270，开启/关闭装饰的整个GameState相同；爆炸暂停/重开、落地暂停/退出均无旧回调污染。合成600格渲染场景全部清除并补充600物品，装饰12处。强制爆炸图与水果图集404后仍完成四轮270分，爆炸全部回退圆环；复用已有透明PNG模拟单张替换验证接口，不代表正式美术定稿。`/course/matchlab/`生产子路径下背景、图集、爆炸图均200，基本游玩流程与暂停通过，控制台错误/警告0。遵照用户要求未截图、未建立可视化辅助或临时测试脚本。

## 生成提示词

采用内置 imagegen，共三个独立资产请求。没有生成网页设计稿或截图。

### 背景

```text
Use case: stylized-concept. Asset type: landscape background illustration for a lightweight browser match-three game called MatchLab, not a UI mockup. Generate a polished gentle orchard garden in a clean modern illustrated game style. Landscape 16:9 composition, ideally 1920x1080 or a landscape native size. Muted sage green trees, soft pale warm sky, subtle sunlit leaves and small fruit trees on the outer edges. Broad calm pale central area so a dense rectangular 30-column by 20-row game board can sit over it. Charming, restrained and crisp, soft daylight, coherent rounded forms, no characters. No title, no logo, no text, no HUD, no board, no buttons, no watermark. This is a separable project background asset, not a screenshot.
```

### 九种物品图集

```text
Use case: stylized-concept. Asset type: ONE coherent game sprite atlas, transparent background. Square image with an EXACT 4-column by 4-row equal-cell layout, ideally 1024x1024 so each cell is 256x256. NO visible grid lines. Place precisely nine isolated fruit icons in the FIRST NINE cells in row-major order: row 1 red apple, orange mandarin, yellow lemon, green pear; row 2 purple grape bunch, blue blueberry cluster, red strawberry with green top, pink peach; row 3 cell 1 a green kiwi half with brown outer skin. Remaining seven cells completely transparent and empty. Each fruit is centered in its own cell and occupies about 65 percent of the cell, with consistent generous transparent margins. No overlap between cells. Clean polished rounded modern game illustration, simple recognizable silhouettes, consistent soft upper-left lighting, restrained glossy highlights, no tiny details, high readability at 24px. Actual transparency, no ground plane, no backdrop, no frames, no shadows extending outside individual fruit bounds. No text, no digits, no labels, no logos, no watermark. This single reusable atlas will be displayed as nine kinds of board items.
```

### 通用爆炸图

```text
Use case: stylized-concept. Asset type: single reusable transparent game effect sprite, square. One centered compact golden-orange and pale cream burst with a soft circular outward ring and six rounded spark rays, polished illustrated style matching a friendly fruit orchard puzzle game. Generous transparent margin, bright but restrained, clean crisp shape readable at small size, no dark soot, no fireball photograph. This is a single static sprite for future scale-and-fade clear/chain feedback, NOT a spritesheet or UI screenshot. Actual transparent background, no ground plane, no text, no numbers, no logos, no watermark.
```
