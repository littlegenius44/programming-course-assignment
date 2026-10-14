#include "raylib.h"
#include "../game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WINDOW_WIDTH 1240   // 固定窗口宽度
#define WINDOW_HEIGHT 760   // 固定窗口高度
#define CELL_SIZE 27        // 数字格尺寸
#define BOARD_X 32          // 棋盘左边缘
#define BOARD_Y 108         // 棋盘上边缘
#define PANEL_X 878         // 算法说明面板左边缘

typedef enum { SCREEN_SETUP, SCREEN_PLAY, SCREEN_RESULT } Screen; // 页面类型

typedef struct
{
    Game game; // 共用算法状态
    RoundExecution round; // 共用阶段结果
    Screen screen; // 当前页面
    char name[PLAYER_NAME_LENGTH]; // UTF-8昵称输入
    int k; // 消除长度
    int demo; // 是否演示模式
    int automatic; // 演示是否自动推进
    int paused; // 用户主动暂停
    int selectedRow; // 选中行，-1为无
    int selectedCol; // 选中列
    int hintVisible; // 是否展示提示
    LegalMove hint; // 只读提示结果
    float stageTime; // 当前阶段显示时间
    float duration; // 当前阶段显示时长
    char message[192]; // 界面提示
    Font font; // 系统中文字体
    int customFont; // 是否需要释放自定义字体
} InterfaceState;

/* 加载本机系统字体。name：需要额外显示的昵称；返回：字体。
 * 仅从Windows字体目录加载，不下载或分发字体文件。 */
static Font loadCourseFont(const char* name)
{
    const char* windir = getenv("WINDIR"); // Windows安装目录
    const char* candidates[] = { "simhei.ttf", "STXIHEI.TTF", "Deng.ttf" }; // raylib支持的系统TTF字体
    char path[512]; // 字体路径
    char glyphs[4096]; // 本界面所有字符与昵称
    int count = 0; // UTF-8字符数量
    int* codepoints; // raylib解码出的字符码点
    Font font; // 加载结果
    snprintf(glyphs, sizeof(glyphs),
        " !\"#$%%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
        "abcdefghijklmnopqrstuvwxyz{}~固定窗口宽度高数字格尺寸棋盘左边缘上算法说明面板页类型共用状态阶段结果当前昵称输入消除长是否演示模式自动推进户主暂停选中行，为无列展提只读显时间界系统文体需要释放义加载本机。：额外的；返回仅从目录不下或分发件安装候路径所有符与量解码出点编号课程位置颜色判断按钮被范围可击绘制亮底择观察秒执一次异常连锁已恢复原和成绩设开始重两个相邻子交换束局请求操作新生保留第向（计）右处理刷形使节非空检查游标再鼠偏移方取戏控在台效未单步条能接受等待播正物品透表根据真实变化记运最终插值近仍横坐纵起核心轨迹补充屏幕顶部先续匹配合去黄框还没轮每落际压缩顺序填测产→累达到三关完尝试过乐二维组语言业同直退删车！卡大闭白蓝后继止图参支持、修改内存侧初见副拟调找种会于确认限较随周期植证案稳信息扫描避免漏掉线清写剩余影响对阈更墙钟扣也力%s", name);
    codepoints = LoadCodepoints(glyphs, &count);
    for (int i = 0; i < 3; ++i) // i：候选字体编号
    {
        snprintf(path, sizeof(path), "%s/Fonts/%s", windir ? windir : "C:/Windows", candidates[i]);
        if (!FileExists(path)) continue;
        font = LoadFontEx(path, 24, codepoints, count);
        if (font.texture.id != 0 && font.texture.id != GetFontDefault().texture.id)
        {
            UnloadCodepoints(codepoints);
            return font;
        }
    }
    UnloadCodepoints(codepoints);
    return GetFontDefault();
}

/* 用课程字体显示文本。ui：只读界面；text：UTF-8文本；x/y：位置；
 * size：字号；color：颜色；返回：无。 */
static void textAt(const InterfaceState* ui, const char* text, int x, int y, int size, Color color)
{
    DrawTextEx(ui->font, text, (Vector2){ (float)x, (float)y }, (float)size, 1.0f, color);
}

/* 判断按钮被按下。rect：范围；enabled：是否可用；返回：是否点击。 */
static int clicked(Rectangle rect, int enabled)
{
    return enabled && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointRec(GetMousePosition(), rect);
}

/* 只绘制按钮，不推进算法。ui：界面；rect：位置；label：文本；
 * enabled：是否可用；selected：是否高亮；返回：无。 */
static void drawButton(const InterfaceState* ui, Rectangle rect,
    const char* label, int enabled, int selected)
{
    Color color = selected ? (Color){ 42, 113, 126, 255 } :
        enabled ? (Color){ 48, 62, 83, 255 } : (Color){ 37, 41, 49, 255 }; // 按钮底色
    DrawRectangleRounded(rect, 0.12f, 4, color);
    textAt(ui, label, (int)rect.x + 10, (int)rect.y + 10, 18, enabled ? RAYWHITE : GRAY);
}

/* 选择显示时长。ui：模式；stage：阶段；返回：阶段观察秒数。 */
static float stageDuration(const InterfaceState* ui, GameStage stage)
{
    if (ui->demo && !ui->automatic) return 0.0f;
    if (stage == STAGE_MATCH || stage == STAGE_LEVEL || stage == STAGE_SHUFFLE)
        return ui->demo ? 1.1f : 0.25f;
    return ui->demo ? 0.75f : 0.3f;
}

/* 执行一次共用阶段。ui：界面状态；返回：无。 */
static void performStep(InterfaceState* ui)
{
    if (!advanceRound(&ui->game, &ui->round)) return;
    ui->stageTime = 0.0f;
    ui->duration = stageDuration(ui, ui->round.stage);
    snprintf(ui->message, sizeof(ui->message), "阶段：%s", getStageName(ui->round.stage));
    TraceLog(LOG_INFO, "COURSE_STAGE %d chain=%d cleared=%d score=%d active=%d",
        (int)ui->round.stage, ui->round.chain, ui->round.totalCleared,
        ui->game.score, ui->round.active);
    if (ui->round.stage == STAGE_ERROR)
        snprintf(ui->message, sizeof(ui->message), "异常连锁已恢复原棋盘和成绩。");
}

/* 按原设置开始或重开。ui：设置与状态；返回：无。 */
static void startGame(InterfaceState* ui)
{
    cancelRound(&ui->game, &ui->round);
    initializeGame(&ui->game, ui->name, ui->k);
    memset(&ui->round, 0, sizeof(ui->round));
    ui->screen = SCREEN_PLAY;
    ui->automatic = 0;
    ui->paused = 0;
    ui->selectedRow = ui->selectedCol = -1;
    ui->hintVisible = 0;
    ui->stageTime = ui->duration = 0.0f;
    snprintf(ui->message, sizeof(ui->message), "点击两个相邻格子交换；H 提示。");
    TraceLog(LOG_INFO, "COURSE_START k=%d demo=%d", ui->k, ui->demo);
}

/* 结束本局。ui：界面状态；返回：无。 */
static void finishGame(InterfaceState* ui)
{
    cancelRound(&ui->game, &ui->round);
    setGamePaused(&ui->game, 1);
    ui->screen = SCREEN_RESULT;
    ui->hintVisible = 0;
    TraceLog(LOG_INFO, "COURSE_RESULT score=%d moves=%d", ui->game.score, ui->game.successfulMoves);
}

/* 请求只读提示。ui：状态；返回：无。 */
static void requestHint(InterfaceState* ui)
{
    ui->selectedRow = ui->selectedCol = -1;
    if (!findLegalMove(&ui->game, &ui->hint))
    {
        initializeBoard(&ui->game);
        snprintf(ui->message, sizeof(ui->message), "无可用操作，重新生成棋盘，成绩保留。");
        ui->hintVisible = 0;
        return;
    }
    ui->hintVisible = 1;
    snprintf(ui->message, sizeof(ui->message), "提示：第%d行第%d列向%s交换（不计分）。",
        ui->hint.row, ui->hint.col, ui->hint.direction == 'D' ? "右" : "下");
    TraceLog(LOG_INFO, "COURSE_HINT row=%d col=%d direction=%c", ui->hint.row, ui->hint.col, ui->hint.direction);
}

/* 处理设置输入。ui：昵称和模式；返回：无。 */
static void handleSettings(InterfaceState* ui)
{
    int codepoint; // 新输入的Unicode字符
    int changed = 0; // 是否需要刷新昵称字形
    while ((codepoint = GetCharPressed()) > 0)
    {
        int length = 0; // 当前字符的UTF-8长度
        const char* encoded = CodepointToUTF8(codepoint, &length); // 字符编码
        size_t used = strlen(ui->name); // 已使用昵称字节数
        if (codepoint >= 32 && used + (size_t)length < sizeof(ui->name))
        {
            memcpy(ui->name + used, encoded, (size_t)length);
            ui->name[used + (size_t)length] = '\0';
            changed = 1;
        }
    }
    if (IsKeyPressed(KEY_BACKSPACE) && ui->name[0])
    {
        size_t length = strlen(ui->name); // 当前昵称长度
        do { --length; } while (length > 0 && ((unsigned char)ui->name[length] & 0xc0) == 0x80);
        ui->name[length] = '\0';
        changed = 1;
    }
    if (changed)
    {
        if (ui->customFont) UnloadFont(ui->font);
        ui->font = loadCourseFont(ui->name);
        ui->customFont = ui->font.texture.id != GetFontDefault().texture.id;
    }
    for (int k = 3; k <= 5; ++k) // k：消除长度按钮
        if (clicked((Rectangle){ 425.0f + (float)(k - 3) * 130.0f, 365, 110, 42 }, 1)) ui->k = k;
    if (clicked((Rectangle){ 425, 425, 180, 42 }, 1)) ui->demo = 0;
    if (clicked((Rectangle){ 625, 425, 180, 42 }, 1)) ui->demo = 1;
    if (clicked((Rectangle){ 425, 495, 380, 48 }, 1) || IsKeyPressed(KEY_ENTER))
    {
        const char* p = ui->name; // 非空昵称检查游标
        while (*p == ' ' || *p == '\t') ++p;
        if (*p) startGame(ui);
        else snprintf(ui->message, sizeof(ui->message), "请输入非空昵称再开始。");
    }
}

/* 处理棋盘点击。ui：当前状态；返回：无。 */
static void handleBoardClick(InterfaceState* ui)
{
    Vector2 mouse = GetMousePosition(); // 鼠标位置
    int row; // 点击行下标
    int col; // 点击列下标
    int dr; // 行偏移
    int dc; // 列偏移
    char direction; // 相邻交换方向
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
        mouse.x < BOARD_X || mouse.x >= BOARD_X + BOARD_COLS * CELL_SIZE ||
        mouse.y < BOARD_Y || mouse.y >= BOARD_Y + BOARD_ROWS * CELL_SIZE) return;
    row = ((int)mouse.y - BOARD_Y) / CELL_SIZE;
    col = ((int)mouse.x - BOARD_X) / CELL_SIZE;
    ui->hintVisible = 0;
    if (ui->selectedRow == row && ui->selectedCol == col)
    {
        ui->selectedRow = ui->selectedCol = -1;
        snprintf(ui->message, sizeof(ui->message), "已取消选择。");
        return;
    }
    if (ui->selectedRow < 0 ||
        abs(row - ui->selectedRow) + abs(col - ui->selectedCol) != 1)
    {
        ui->selectedRow = row;
        ui->selectedCol = col;
        snprintf(ui->message, sizeof(ui->message), "已选中第%d行第%d列，请点相邻格子。", row + 1, col + 1);
        return;
    }
    dr = row - ui->selectedRow;
    dc = col - ui->selectedCol;
    direction = dr < 0 ? 'W' : dr > 0 ? 'S' : dc < 0 ? 'A' : 'D';
    if (beginRound(&ui->game, &ui->round, ui->selectedRow + 1,
        ui->selectedCol + 1, direction) == 1) performStep(ui);
    ui->selectedRow = ui->selectedCol = -1;
}

/* 处理游戏控制与自动推进。ui：状态；返回：无。 */
static void updateGame(InterfaceState* ui)
{
    int focused = IsWindowFocused(); // 窗口是否在前台
    int animating = ui->stageTime < ui->duration; // 本阶段效果是否未结束
    int stepAllowed = ui->round.active && ui->demo && !ui->paused && focused && !animating; // 单步条件
    int idle = !ui->round.active && !animating && !ui->paused && focused; // 能否接受交换
    if (!focused) ui->paused = 1;
    if (clicked((Rectangle){ 32, 695, 112, 42 }, stepAllowed) ||
        (stepAllowed && IsKeyPressed(KEY_SPACE))) performStep(ui);
    if (clicked((Rectangle){ 154, 695, 132, 42 }, ui->demo) ||
        (ui->demo && IsKeyPressed(KEY_A)))
    {
        ui->automatic = !ui->automatic;
        ui->duration = stageDuration(ui, ui->round.stage);
        ui->stageTime = 0.0f;
    }
    if (clicked((Rectangle){ 296, 695, 100, 42 }, 1) || IsKeyPressed(KEY_P))
    {
        ui->paused = !ui->paused;
        TraceLog(LOG_INFO, "COURSE_PAUSE %d", ui->paused);
    }
    if (clicked((Rectangle){ 406, 695, 100, 42 }, idle) || (idle && IsKeyPressed(KEY_H)))
        requestHint(ui);
    if (clicked((Rectangle){ 516, 695, 100, 42 }, 1) || IsKeyPressed(KEY_R)) startGame(ui);
    if (clicked((Rectangle){ 626, 695, 100, 42 }, 1))
    { finishGame(ui); return; }
    if (idle) handleBoardClick(ui);

    // 单步等待与主动暂停不计时；自动播放和正常操作计时。
    setGamePaused(&ui->game, ui->paused || !focused ||
        (ui->demo && !ui->automatic && ui->round.active));
    if (!ui->paused && focused)
    {
        ui->stageTime += GetFrameTime();
        if (ui->round.active && (!ui->demo || ui->automatic) &&
            ui->stageTime >= ui->duration) performStep(ui);
        if (!ui->round.active && ui->round.stage == STAGE_FINISH &&
            ui->round.levelResult == 2) finishGame(ui);
    }
}

/* 绘制一个数字格。ui：字体；value：物品；x/y：位置；alpha：透明度；返回：无。 */
static void drawTile(const InterfaceState* ui, int value, float x, float y, float alpha)
{
    const Color colors[ITEM_TYPES] = { {181,74,86,255}, {179,118,67,255}, {163,143,54,255},
        {65,139,112,255}, {55,115,162,255}, {100,96,161,255}, {133,83,151,255},
        {162,76,121,255}, {80,129,68,255} }; // 物品颜色表
    char label[2] = { (char)('0' + value), '\0' }; // 数字文本
    if (value < 1 || value > ITEM_TYPES) return;
    DrawRectangleRec((Rectangle){ x, y, CELL_SIZE - 2, CELL_SIZE - 2 }, Fade(colors[value - 1], alpha));
    textAt(ui, label, (int)x + 8, (int)y + 3, 17, Fade(WHITE, alpha));
}

/* 根据真实变化记录绘制棋盘，只读状态。ui：界面；返回：无。 */
static void drawBoard(const InterfaceState* ui)
{
    int moving[BOARD_ROWS][BOARD_COLS] = { 0 }; // 运动物品的最终位置
    float t = ui->duration > 0 ? ui->stageTime / ui->duration : 1.0f; // 插值进度
    const RoundExecution* round = &ui->round; // 最近阶段结果
    int animation = t < 1.0f; // 是否仍在插值
    if (t > 1.0f) t = 1.0f;
    if (animation && (round->stage == STAGE_GRAVITY || round->stage == STAGE_REFILL))
        for (int i = 0; i < round->changeCount; ++i) // i：运动物品
            moving[round->changes[i].toRow][round->changes[i].toCol] = 1;
    if (animation && (round->stage == STAGE_SWAP || round->stage == STAGE_REVERT))
    {
        moving[round->fromRow][round->fromCol] = 1;
        moving[round->toRow][round->toCol] = 1;
    }
    for (int col = 0; col < BOARD_COLS; ++col) // col：列标
        textAt(ui, TextFormat("%d", col + 1), BOARD_X + col * CELL_SIZE + 3, BOARD_Y - 20, 12, LIGHTGRAY);
    BeginScissorMode(BOARD_X, BOARD_Y, BOARD_COLS * CELL_SIZE, BOARD_ROWS * CELL_SIZE);
    for (int row = 0; row < BOARD_ROWS; ++row) // row：绘制行
    {
        for (int col = 0; col < BOARD_COLS; ++col) // col：绘制列
        {
            float x = (float)(BOARD_X + col * CELL_SIZE); // 格子横坐标
            float y = (float)(BOARD_Y + row * CELL_SIZE); // 格子纵坐标
            int value = ui->game.board[row][col]; // 最终物品
            DrawRectangleLines((int)x, (int)y, CELL_SIZE - 1, CELL_SIZE - 1, (Color){ 44, 53, 64, 255 });
            if (!moving[row][col])
            {
                if (animation && round->stage == STAGE_CLEAR && round->matches.marked[row][col])
                    drawTile(ui, round->before[row][col], x, y, 1.0f - t);
                else if (value) drawTile(ui, value, x, y, 1.0f);
                else textAt(ui, ".", (int)x + 10, (int)y + 3, 17, GRAY);
            }
            if (round->stage == STAGE_MATCH && round->matches.marked[row][col])
                DrawRectangleLinesEx((Rectangle){ x, y, CELL_SIZE - 2, CELL_SIZE - 2 }, 2, YELLOW);
        }
    }
    if (animation && (round->stage == STAGE_SWAP || round->stage == STAGE_REVERT))
    {
        for (int i = 0; i < 2; ++i) // i：交换中的两个物品
        {
            int r1 = i ? round->toRow : round->fromRow; // 起点行
            int c1 = i ? round->toCol : round->fromCol; // 起点列
            int r2 = i ? round->fromRow : round->toRow; // 终点行
            int c2 = i ? round->fromCol : round->toCol; // 终点列
            drawTile(ui, round->before[r1][c1],
                (float)BOARD_X + ((float)c1 + (float)(c2 - c1) * t) * CELL_SIZE,
                (float)BOARD_Y + ((float)r1 + (float)(r2 - r1) * t) * CELL_SIZE, 1);
        }
    }
    if (animation && (round->stage == STAGE_GRAVITY || round->stage == STAGE_REFILL))
    {
        for (int i = 0; i < round->changeCount; ++i) // i：核心记录的真实变化
        {
            const CellChange* change = &round->changes[i]; // 当前物品轨迹
            int fromRow = change->fromRow; // 起始行，补充时计算屏幕外位置
            if (fromRow < 0)
            {
                int empty = 0; // 本列顶部空位数
                while (empty < BOARD_ROWS && round->before[empty][change->toCol] == 0) ++empty;
                fromRow = change->toRow - empty;
            }
            drawTile(ui, change->value, (float)(BOARD_X + change->toCol * CELL_SIZE),
                (float)BOARD_Y + ((float)fromRow + (float)(change->toRow - fromRow) * t) * CELL_SIZE, 1);
        }
    }
    EndScissorMode();
    for (int row = 0; row < BOARD_ROWS; ++row) // row：行标
        textAt(ui, TextFormat("%d", row + 1), 7, BOARD_Y + row * CELL_SIZE + 5, 12, LIGHTGRAY);
    if (ui->selectedRow >= 0)
        DrawRectangleLinesEx((Rectangle){ (float)(BOARD_X + ui->selectedCol * CELL_SIZE),
            (float)(BOARD_Y + ui->selectedRow * CELL_SIZE), CELL_SIZE - 2, CELL_SIZE - 2 }, 3, WHITE);
    if (ui->hintVisible)
    {
        int r = ui->hint.row - 1; // 提示起始行
        int c = ui->hint.col - 1; // 提示起始列
        for (int i = 0; i < 2; ++i) // i：两个提示位置
            DrawRectangleLinesEx((Rectangle){ (float)(BOARD_X + (c + (i && ui->hint.direction == 'D')) * CELL_SIZE),
                (float)(BOARD_Y + (r + (i && ui->hint.direction == 'S')) * CELL_SIZE),
                CELL_SIZE - 2, CELL_SIZE - 2 }, 3, SKYBLUE);
    }
}

/* 显示阶段解释。ui：只读真实数据；返回：无。 */
static void drawExplanation(const InterfaceState* ui)
{
    const RoundExecution* r = &ui->round; // 本阶段数据
    int y = 315; // 说明起始纵坐标
    textAt(ui, TextFormat("阶段：%s", getStageName(r->stage)), PANEL_X + 14, y, 22, YELLOW);
    y += 38;
    switch (r->stage)
    {
    case STAGE_SWAP:
    case STAGE_REVERT:
        textAt(ui, TextFormat("位置：(%d,%d) 与 (%d,%d)", r->fromRow + 1,
            r->fromCol + 1, r->toRow + 1, r->toCol + 1), PANEL_X + 14, y, 17, WHITE);
        textAt(ui, r->stage == STAGE_SWAP ? "先交换，再判断是否形成连续段。" :
            "无有效匹配，恢复棋盘，不计分。", PANEL_X + 14, y + 28, 17, LIGHTGRAY);
        break;
    case STAGE_MATCH:
        textAt(ui, TextFormat("匹配段：%d；段长度合计：%d", r->matches.segmentCount,
            r->matches.rawCount), PANEL_X + 14, y, 17, WHITE);
        textAt(ui, TextFormat("去重：%d格；重复：%d格", r->matches.uniqueCount,
            r->matches.rawCount - r->matches.uniqueCount), PANEL_X + 14, y + 28, 17, WHITE);
        textAt(ui, "黄色边框是标记，还未消除。", PANEL_X + 14, y + 56, 17, LIGHTGRAY);
        for (int i = 0; i < r->matches.segmentCount && i < 6; ++i) // i：显示的匹配段
        {
            const MatchSegment* s = &r->matches.segments[i]; // 当前真实匹配段
            textAt(ui, TextFormat("%s(%d,%d)，数字%d，长度%d", s->vertical ? "纵" : "横",
                s->row + 1, s->col + 1, s->value, s->length), PANEL_X + 14, y + 91 + i * 25, 16, LIGHTGRAY);
        }
        if (!r->matches.uniqueCount)
            textAt(ui, r->chain ? "没有新匹配，连锁结束。" : "没有匹配，下一步恢复交换。",
                PANEL_X + 14, y + 91, 17, GOLD);
        break;
    case STAGE_CLEAR:
        textAt(ui, TextFormat("第%d轮；每格%d分", r->chain, POINTS_PER_ITEM), PANEL_X + 14, y, 18, WHITE);
        textAt(ui, TextFormat("本轮 +%d；本回合消除%d格", r->scoreDelta, r->totalCleared),
            PANEL_X + 14, y + 30, 18, WHITE);
        textAt(ui, "空位为0，先消除再进行下落。", PANEL_X + 14, y + 64, 17, LIGHTGRAY);
        break;
    case STAGE_GRAVITY:
    case STAGE_REFILL:
        textAt(ui, TextFormat("%s数量：%d", r->stage == STAGE_GRAVITY ? "实际移动" : "新生成",
            r->changeCount), PANEL_X + 14, y, 18, WHITE);
        textAt(ui, r->stage == STAGE_GRAVITY ? "按列向下压缩，保留物品顺序。" :
            "只填空位，再检测是否产生连锁。", PANEL_X + 14, y + 30, 17, LIGHTGRAY);
        for (int i = 0; i < r->changeCount && i < 6; ++i) // i：展示真实轨迹
        {
            const CellChange* c = &r->changes[i]; // 当前记录
            const char* line = c->fromRow < 0 ? TextFormat("新数字%d → (%d,%d)", c->value, c->toRow + 1, c->toCol + 1) :
                TextFormat("%d：(%d,%d) → (%d,%d)", c->value, c->fromRow + 1, c->fromCol + 1, c->toRow + 1, c->toCol + 1); // 轨迹说明
            textAt(ui, line, PANEL_X + 14, y + 68 + i * 25, 16, LIGHTGRAY);
        }
        break;
    case STAGE_LEVEL:
        textAt(ui, r->levelResult == 2 ? "累计达到1200分，三关完成。" :
            r->levelResult == 1 ? "达到目标，进入新关，分数保留。" : "分数未达到下一关目标。",
            PANEL_X + 14, y, 17, WHITE);
        break;
    case STAGE_SHUFFLE:
        textAt(ui, r->changeCount ? "无有效交换，已重新生成棋盘。" : "有可用操作，无需重新生成。",
            PANEL_X + 14, y, 17, WHITE);
        break;
    case STAGE_FINISH:
        textAt(ui, TextFormat("回合：%d轮，%d格，%d分", r->chain, r->totalCleared,
            r->totalCleared * POINTS_PER_ITEM), PANEL_X + 14, y, 18, WHITE);
        break;
    case STAGE_ERROR:
        textAt(ui, "异常已恢复，可重新尝试。", PANEL_X + 14, y, 17, WHITE);
        break;
    default:
        textAt(ui, "选择相邻格子，观察真实算法过程。", PANEL_X + 14, y, 17, WHITE);
    }
}

/* 绘制当前页面。ui：只读状态；返回：无。 */
static void drawPage(const InterfaceState* ui)
{
    ClearBackground((Color){ 19, 26, 36, 255 });
    textAt(ui, "消消乐 · 二维数组算法演示", 32, 22, 30, RAYWHITE);
    if (ui->screen == SCREEN_SETUP)
    {
        textAt(ui, "C语言课程作业：同一算法，正常运行或阶段演示", 280, 155, 24, LIGHTGRAY);
        textAt(ui, "昵称：直接输入，退格删除，回车开始", 425, 245, 20, RAYWHITE);
        DrawRectangle(425, 285, 380, 48, (Color){ 42, 53, 69, 255 });
        textAt(ui, ui->name[0] ? ui->name : "请输入昵称", 438, 298, 22, WHITE);
        for (int k = 3; k <= 5; ++k) // k：绘制长度按钮
            drawButton(ui, (Rectangle){ 425.0f + (float)(k - 3) * 130.0f, 365, 110, 42 },
                TextFormat("k = %d", k), 1, ui->k == k);
        drawButton(ui, (Rectangle){ 425, 425, 180, 42 }, "正常模式", 1, !ui->demo);
        drawButton(ui, (Rectangle){ 625, 425, 180, 42 }, "算法演示", 1, ui->demo);
        drawButton(ui, (Rectangle){ 425, 495, 380, 48 }, "开始（Enter）", 1, 1);
        textAt(ui, ui->message, 425, 565, 18, GOLD);
        return;
    }
    if (ui->screen == SCREEN_RESULT)
    {
        textAt(ui, ui->game.score >= 1200 ? "三关完成！" : "本局已结束", 425, 170, 32, GOLD);
        textAt(ui, TextFormat("昵称：%s；k=%d", ui->game.playerName, ui->game.matchLength), 425, 245, 22, WHITE);
        textAt(ui, TextFormat("关卡：%d；最终分数：%d", ui->game.level, ui->game.score), 425, 290, 22, WHITE);
        textAt(ui, TextFormat("有效用时：%ld秒；有效交换：%d次", getElapsedSeconds(&ui->game),
            ui->game.successfulMoves), 425, 335, 20, WHITE);
        textAt(ui, TextFormat("累计消除：%d格；最大连锁：%d轮", ui->game.totalCleared,
            ui->game.maxChain), 425, 380, 20, WHITE);
        drawButton(ui, (Rectangle){ 425, 455, 170, 44 }, "原设置重开", 1, 0);
        drawButton(ui, (Rectangle){ 615, 455, 190, 44 }, "返回设置", 1, 0);
        textAt(ui, "未完成回合退出时，棋盘和成绩已恢复。", 425, 535, 18, LIGHTGRAY);
        textAt(ui, "关闭窗口或 Esc 退出程序。", 425, 565, 18, LIGHTGRAY);
        return;
    }
    textAt(ui, TextFormat("模式：%s；黄色=匹配；白色=选中；蓝色=提示",
        ui->demo ? "算法演示" : "正常"), 32, 67, 18, LIGHTGRAY);
    drawBoard(ui);
    DrawRectangle(PANEL_X, BOARD_Y, 330, 548, (Color){ 29, 40, 54, 255 });
    textAt(ui, TextFormat("昵称：%s", ui->game.playerName), PANEL_X + 14, 122, 20, WHITE);
    textAt(ui, TextFormat("k=%d；第%d关", ui->game.matchLength, ui->game.level), PANEL_X + 14, 159, 20, WHITE);
    textAt(ui, TextFormat("分数：%d / %d", ui->game.score, getLevelTarget(&ui->game)), PANEL_X + 14, 196, 20, WHITE);
    textAt(ui, TextFormat("有效用时：%ld秒", getElapsedSeconds(&ui->game)), PANEL_X + 14, 233, 20, WHITE);
    textAt(ui, TextFormat("本回合消除轮数：%d", ui->round.chain), PANEL_X + 14, 270, 20, GOLD);
    drawExplanation(ui);
    textAt(ui, ui->paused ? "已暂停（窗口返回后需点击继续）" : ui->message, 32, 664, 18, GOLD);
    drawButton(ui, (Rectangle){ 32, 695, 112, 42 }, "下一步",
        ui->demo && ui->round.active && !ui->paused && IsWindowFocused() && ui->stageTime >= ui->duration, 0);
    drawButton(ui, (Rectangle){ 154, 695, 132, 42 }, ui->automatic ? "停止自动" : "自动播放", ui->demo, ui->automatic);
    drawButton(ui, (Rectangle){ 296, 695, 100, 42 }, ui->paused ? "继续" : "暂停", 1, ui->paused);
    drawButton(ui, (Rectangle){ 406, 695, 100, 42 }, "提示 H",
        !ui->round.active && !ui->paused && IsWindowFocused() && ui->stageTime >= ui->duration, 0);
    drawButton(ui, (Rectangle){ 516, 695, 100, 42 }, "重开 R", 1, 0);
    drawButton(ui, (Rectangle){ 626, 695, 100, 42 }, "结束本局", 1, 0);
    textAt(ui, "Space下一步 / A自动 / P暂停", 751, 709, 16, LIGHTGRAY);
}

/* 图形入口。参数：无；返回：0正常退出。 */
int main(void)
{
    InterfaceState ui = { 0 }; // 当前页面与游戏状态
    ui.screen = SCREEN_SETUP;
    ui.k = 3;
    ui.selectedRow = ui.selectedCol = -1;
    srand((unsigned int)time(NULL));
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Matchgame - C algorithm demonstration");
    SetTargetFPS(60);
    ui.font = loadCourseFont("");
    ui.customFont = ui.font.texture.id != GetFontDefault().texture.id;
    snprintf(ui.message, sizeof(ui.message), "支持昵称、k=3/4/5、阶段展示与单步。");
    while (!WindowShouldClose())
    {
        if (ui.screen == SCREEN_SETUP) handleSettings(&ui);
        else if (ui.screen == SCREEN_PLAY) updateGame(&ui);
        else
        {
            if (clicked((Rectangle){ 425, 455, 170, 44 }, 1) || IsKeyPressed(KEY_R)) startGame(&ui);
            if (clicked((Rectangle){ 615, 455, 190, 44 }, 1))
            {
                ui.screen = SCREEN_SETUP;
                snprintf(ui.message, sizeof(ui.message), "修改设置后开始新局。");
            }
        }
        BeginDrawing();
        drawPage(&ui);
        EndDrawing();
    }
    if (ui.screen == SCREEN_PLAY) cancelRound(&ui.game, &ui.round);
    if (ui.customFont) UnloadFont(ui.font);
    CloseWindow();
    return 0;
}
