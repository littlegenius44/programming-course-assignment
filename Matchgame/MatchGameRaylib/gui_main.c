#include "raylib.h"
#include "../game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
// 避免Windows同名声明与raylib的CloseWindow/ShowCursor及Rectangle冲突。
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#include <windows.h>
#undef CloseWindow
#undef ShowCursor
#undef DrawText
#undef DrawTextEx
#undef LoadImage
#endif

#define WINDOW_WIDTH 1600   // 逻辑画布宽度，实际窗口可等比缩放
#define WINDOW_HEIGHT 1000  // 逻辑画布高度
#define PANEL_X 1168        // 算法说明面板左边缘
#define PANEL_Y 132         // 面板与棋盘区域上边缘

typedef struct
{
    int cell; // 根据当前棋盘计算的格子尺寸
    int boardX; // 左侧区域内居中的棋盘横坐标
    int boardY; // 左侧区域内居中的棋盘纵坐标
    int boardWidth; // 有效棋盘像素宽度
    int boardHeight; // 有效棋盘像素高度
} BoardLayout;

typedef struct
{
    float scale; // 实际窗口与逻辑画布之间的统一缩放比例
    Rectangle area; // 实际窗口中画布的位置，外侧留白不接受点击
} CanvasViewport;

typedef struct
{
    int width; // 客户区宽度
    int height; // 客户区高度
    int x; // 外框起点横坐标
    int y; // 外框起点纵坐标
} InitialWindowLayout;

typedef enum
{
    CONTROL_NEXT, CONTROL_AUTO, CONTROL_PAUSE,
    CONTROL_HINT, CONTROL_RESTART, CONTROL_END
} Control; // 底部六个操作按钮

typedef enum { SCREEN_SETUP, SCREEN_PLAY, SCREEN_RESULT } Screen; // 页面类型

typedef struct
{
    Game game; // 共用算法状态
    RoundExecution round; // 共用阶段结果
    Screen screen; // 当前页面
    char name[PLAYER_NAME_LENGTH]; // UTF-8昵称输入
    int k; // 消除长度
    int boardSize; // 棋盘档位，独立于k
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

/* 限制整个外窗在工作区内。work：可用桌面；frameWidth/frameHeight：非客户区尺寸；
 * 返回：客户区大小与居中的外框起点。 */
static InitialWindowLayout initialWindowLayout(Rectangle work, int frameWidth, int frameHeight)
{
    InitialWindowLayout layout; // 工作区四周各留16像素
    int availableWidth = (int)work.width - frameWidth - 32; // 扣除边框
    int availableHeight = (int)work.height - frameHeight - 32; // 扣除标题栏和边框
    layout.width = availableWidth < WINDOW_WIDTH ? availableWidth : WINDOW_WIDTH;
    layout.height = availableHeight < WINDOW_HEIGHT ? availableHeight : WINDOW_HEIGHT;
    if (layout.width < 1) layout.width = 1;
    if (layout.height < 1) layout.height = 1;
    layout.x = (int)work.x + ((int)work.width - layout.width - frameWidth) / 2;
    layout.y = (int)work.y + ((int)work.height - layout.height - frameHeight) / 2;
    return layout;
}

/* 按实际工作区设置初始窗口。参数：无；返回：无；调用时窗口已经创建且隐藏。 */
static void fitWindowToDesktop(void)
{
    int monitor = GetCurrentMonitor(); // raylib当前显示器
    Vector2 position = GetMonitorPosition(monitor); // 非Windows的后备位置
    Rectangle work = { position.x, position.y, (float)GetMonitorWidth(monitor),
        (float)GetMonitorHeight(monitor) }; // 后备显示器范围
    int frameWidth = 32, frameHeight = 64; // 无法读取外框时保守预留
#ifdef _WIN32
    HWND window = (HWND)GetWindowHandle(); // GLFW实际的原生窗口
    MONITORINFO info = { 0 }; // 当前显示器真实工作区，不含任务栏
    RECT frame, client; // 窗口外框与客户区
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &info))
        work = (Rectangle){ (float)info.rcWork.left, (float)info.rcWork.top,
            (float)(info.rcWork.right - info.rcWork.left), (float)(info.rcWork.bottom - info.rcWork.top) };
    if (GetWindowRect(window, &frame) && GetClientRect(window, &client))
    {
        frameWidth = (int)((frame.right - frame.left) - (client.right - client.left));
        frameHeight = (int)((frame.bottom - frame.top) - (client.bottom - client.top));
    }
#endif
    InitialWindowLayout layout = initialWindowLayout(work, frameWidth, frameHeight); // 包含非客户区的计算
    SetWindowSize(layout.width, layout.height);
    SetWindowMinSize(layout.width < 640 ? layout.width : 640, layout.height < 400 ? layout.height : 400);
#ifdef _WIN32
    // SetWindowPos接收外框起点；raylib的SetWindowPosition接收客户区起点，不能直接混用。
    SetWindowPos(window, NULL, layout.x, layout.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
#else
    SetWindowPosition(layout.x, layout.y);
#endif
}


/* 根据有效行列计算棋盘布局。game：合法初始化的状态；返回：居中位置和格子大小。 */
static BoardLayout boardLayout(const Game* game)
{
    BoardLayout layout; // 左侧1080×720区域内的布局
    int cell = 1080 / game->cols; // 按横向空间限制尺寸
    if (cell > 720 / game->rows) cell = 720 / game->rows;
    if (cell > 60) cell = 60;
    layout.cell = cell;
    layout.boardWidth = game->cols * cell;
    layout.boardHeight = game->rows * cell;
    layout.boardX = 48 + (1080 - layout.boardWidth) / 2;
    layout.boardY = PANEL_Y + (720 - layout.boardHeight) / 2;
    return layout;
}

/* 计算等比画布视口。width/height：实际窗口尺寸；返回：画布范围和缩放比例。 */
static CanvasViewport viewportForSize(int width, int height)
{
    CanvasViewport viewport = { 0 }; // 空窗口使用零比例，不接收点击
    if (width <= 0 || height <= 0) return viewport;
    viewport.scale = (float)width / WINDOW_WIDTH;
    if (viewport.scale > (float)height / WINDOW_HEIGHT)
        viewport.scale = (float)height / WINDOW_HEIGHT;
    viewport.area.width = WINDOW_WIDTH * viewport.scale;
    viewport.area.height = WINDOW_HEIGHT * viewport.scale;
    viewport.area.x = ((float)width - viewport.area.width) / 2.0f;
    viewport.area.y = ((float)height - viewport.area.height) / 2.0f;
    return viewport;
}

/* 将实际坐标换回逻辑坐标。point：窗口位置；viewport：画布视口；
 * 返回：逻辑坐标；画布外返回(-1,-1)。 */
static Vector2 toLogicalPoint(Vector2 point, CanvasViewport viewport)
{
    if (viewport.scale <= 0.0f || point.x < viewport.area.x || point.y < viewport.area.y ||
        point.x >= viewport.area.x + viewport.area.width ||
        point.y >= viewport.area.y + viewport.area.height) return (Vector2){ -1, -1 };
    return (Vector2){ (point.x - viewport.area.x) / viewport.scale,
        (point.y - viewport.area.y) / viewport.scale };
}

/* 读取缩放后的鼠标位置。参数：无；返回：统一的逻辑坐标。 */
static Vector2 virtualMouse(void)
{
    return toLogicalPoint(GetMousePosition(), viewportForSize(GetScreenWidth(), GetScreenHeight()));
}

/* 获取操作按钮范围。control：按钮编号；返回：固定逻辑范围。 */
static Rectangle controlRect(Control control)
{
    const Rectangle rectangles[] = { {48,920,148,52}, {208,920,180,52}, {400,920,144,52},
        {556,920,144,52}, {712,920,144,52}, {868,920,164,52} }; // 显示和输入共用的位置
    return rectangles[control];
}

/* 获取尺寸或k按钮范围。index：0～2；sizeButton：是否尺寸按钮；返回：逻辑范围。 */
static Rectangle optionRect(int index, int sizeButton)
{
    return (Rectangle){ 470.0f + (float)index * 225.0f, sizeButton ? 400.0f : 512.0f, 210, 52 };
}

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
        "abcdefghijklmnopqrstuvwxyz{}~固定窗口宽度高数字格尺寸棋盘左边缘上算法说明面板页类型共用状态阶段结果当前昵称输入消除长是否演示模式自动推进户主暂停选中行，为无列展提只读显时间界系统文体需要释放义加载本机。：额外的；返回仅从目录不下或分发件安装候路径所有符与量解码出点编号课程位置颜色判断按钮被范围可击绘制亮底择观察秒执一次异常连锁已恢复原和成绩设开始重两个相邻子交换束局请求操作新生保留第向（计）右处理刷形使节非空检查游标再鼠偏移方取戏控在台效未单步条能接受等待播正物品透表根据真实变化记运最终插值近仍横坐纵起核心轨迹补充屏幕顶部先续匹配合去黄框还没轮每落际压缩顺序填测产→累达到三关完尝试过乐二维组语言业同直退删车！卡大闭白蓝后继止图参支持、修改内存侧初见副拟调找种会于确认限较随周期植证案稳信息扫描避免漏掉线清写剩余影响对阈更墙钟扣也力小型规模档位列行大小×调整窗口等比缩放留边工作区·独立%s", name);
    codepoints = LoadCodepoints(glyphs, &count);
    for (int i = 0; i < 3; ++i) // i：候选字体编号
    {
        snprintf(path, sizeof(path), "%s/Fonts/%s", windir ? windir : "C:/Windows", candidates[i]);
        if (!FileExists(path)) continue;
        font = LoadFontEx(path, 36, codepoints, count);
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
        CheckCollisionPointRec(virtualMouse(), rect);
}

/* 只绘制按钮，不推进算法。ui：界面；rect：位置；label：文本；
 * enabled：是否可用；selected：是否高亮；返回：无。 */
static void drawButton(const InterfaceState* ui, Rectangle rect,
    const char* label, int enabled, int selected)
{
    Color color = selected ? (Color){ 42, 113, 126, 255 } :
        enabled ? (Color){ 48, 62, 83, 255 } : (Color){ 37, 41, 49, 255 }; // 按钮底色
    DrawRectangleRounded(rect, 0.12f, 4, color);
    Vector2 labelSize = MeasureTextEx(ui->font, label, 22, 1); // 按钮文字居中
    textAt(ui, label, (int)(rect.x + (rect.width - labelSize.x) / 2),
        (int)(rect.y + (rect.height - labelSize.y) / 2), 22, enabled ? RAYWHITE : GRAY);
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
    initializeGameWithSize(&ui->game, ui->name, ui->k, ui->boardSize);
    memset(&ui->round, 0, sizeof(ui->round));
    ui->screen = SCREEN_PLAY;
    ui->automatic = 0;
    ui->paused = 0;
    ui->selectedRow = ui->selectedCol = -1;
    ui->hintVisible = 0;
    ui->stageTime = ui->duration = 0.0f;
    snprintf(ui->message, sizeof(ui->message), "点击两个相邻格子交换；H 提示。");
    TraceLog(LOG_INFO, "COURSE_START k=%d demo=%d size=%d cols=%d rows=%d",
        ui->k, ui->demo, ui->boardSize, ui->game.cols, ui->game.rows);
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
    for (int size = 1; size <= BOARD_SIZE_COUNT; ++size) // size：三档棋盘按钮
        if (clicked(optionRect(size - 1, 1), 1)) ui->boardSize = size;
    for (int k = 3; k <= 5; ++k) // k：独立的消除长度按钮
        if (clicked(optionRect(k - 3, 0), 1)) ui->k = k;
    if (clicked((Rectangle){ 470, 606, 320, 52 }, 1)) ui->demo = 0;
    if (clicked((Rectangle){ 810, 606, 320, 52 }, 1)) ui->demo = 1;
    if (clicked((Rectangle){ 470, 700, 660, 58 }, 1) || IsKeyPressed(KEY_ENTER))
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
    Vector2 mouse = virtualMouse(); // 使用与画布一致的逻辑位置
    BoardLayout layout = boardLayout(&ui->game); // 当前有效棋盘布局
    int row; // 点击行下标
    int col; // 点击列下标
    int dr; // 行偏移
    int dc; // 列偏移
    char direction; // 相邻交换方向
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
        mouse.x < layout.boardX || mouse.x >= layout.boardX + ui->game.cols * layout.cell ||
        mouse.y < layout.boardY || mouse.y >= layout.boardY + ui->game.rows * layout.cell) return;
    row = ((int)mouse.y - layout.boardY) / layout.cell;
    col = ((int)mouse.x - layout.boardX) / layout.cell;
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
    if (clicked(controlRect(CONTROL_NEXT), stepAllowed) ||
        (stepAllowed && IsKeyPressed(KEY_SPACE))) performStep(ui);
    if (clicked(controlRect(CONTROL_AUTO), ui->demo) ||
        (ui->demo && IsKeyPressed(KEY_A)))
    {
        ui->automatic = !ui->automatic;
        ui->duration = stageDuration(ui, ui->round.stage);
        ui->stageTime = 0.0f;
    }
    if (clicked(controlRect(CONTROL_PAUSE), 1) || IsKeyPressed(KEY_P))
    {
        ui->paused = !ui->paused;
        TraceLog(LOG_INFO, "COURSE_PAUSE %d", ui->paused);
    }
    if (clicked(controlRect(CONTROL_HINT), idle) || (idle && IsKeyPressed(KEY_H)))
        requestHint(ui);
    if (clicked(controlRect(CONTROL_RESTART), 1) || IsKeyPressed(KEY_R)) startGame(ui);
    if (clicked(controlRect(CONTROL_END), 1))
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
    BoardLayout layout = boardLayout(&ui->game); // 格子大小随棋盘档位改变
    const Color colors[ITEM_TYPES] = { {181,74,86,255}, {179,118,67,255}, {163,143,54,255},
        {65,139,112,255}, {55,115,162,255}, {100,96,161,255}, {133,83,151,255},
        {162,76,121,255}, {80,129,68,255} }; // 物品颜色表
    char label[2] = { (char)('0' + value), '\0' }; // 数字文本
    if (value < 1 || value > ITEM_TYPES) return;
    DrawRectangleRec((Rectangle){ x, y, (float)(layout.cell - 2), (float)(layout.cell - 2) }, Fade(colors[value - 1], alpha));
    int fontSize = (int)((float)layout.cell * 0.62f); // 数字比原版更大
    Vector2 labelSize = MeasureTextEx(ui->font, label, (float)fontSize, 1); // 格子文字尺寸
    textAt(ui, label, (int)(x + (layout.cell - labelSize.x) / 2),
        (int)(y + (layout.cell - labelSize.y) / 2), fontSize, Fade(WHITE, alpha));
}

/* 根据真实变化记录绘制棋盘，只读状态。ui：界面；返回：无。 */
static void drawBoard(const InterfaceState* ui)
{
    BoardLayout layout = boardLayout(&ui->game); // 显示与点击使用同一棋盘布局
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
    for (int col = 0; col < ui->game.cols; ++col) // col：列标
        textAt(ui, TextFormat("%d", col + 1), layout.boardX + col * layout.cell + (layout.cell - 22) / 2, layout.boardY - 25, 18, LIGHTGRAY);
    BeginScissorMode(layout.boardX, layout.boardY, ui->game.cols * layout.cell, ui->game.rows * layout.cell);
    for (int row = 0; row < ui->game.rows; ++row) // row：绘制行
    {
        for (int col = 0; col < ui->game.cols; ++col) // col：绘制列
        {
            float x = (float)(layout.boardX + col * layout.cell); // 格子横坐标
            float y = (float)(layout.boardY + row * layout.cell); // 格子纵坐标
            int value = ui->game.board[row][col]; // 最终物品
            DrawRectangleLines((int)x, (int)y, layout.cell - 1, layout.cell - 1, (Color){ 44, 53, 64, 255 });
            if (!moving[row][col])
            {
                if (animation && round->stage == STAGE_CLEAR && round->matches.marked[row][col])
                    drawTile(ui, round->before[row][col], x, y, 1.0f - t);
                else if (value) drawTile(ui, value, x, y, 1.0f);
                else textAt(ui, ".", (int)x + layout.cell / 2 - 4, (int)y + layout.cell / 2 - 12, 22, GRAY);
            }
            if (round->stage == STAGE_MATCH && round->matches.marked[row][col])
                DrawRectangleLinesEx((Rectangle){ x, y, (float)(layout.cell - 2), (float)(layout.cell - 2) }, 2, YELLOW);
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
                (float)layout.boardX + ((float)c1 + (float)(c2 - c1) * t) * layout.cell,
                (float)layout.boardY + ((float)r1 + (float)(r2 - r1) * t) * layout.cell, 1);
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
                while (empty < ui->game.rows && round->before[empty][change->toCol] == 0) ++empty;
                fromRow = change->toRow - empty;
            }
            drawTile(ui, change->value, (float)(layout.boardX + change->toCol * layout.cell),
                (float)layout.boardY + ((float)fromRow + (float)(change->toRow - fromRow) * t) * layout.cell, 1);
        }
    }
    EndScissorMode();
    for (int row = 0; row < ui->game.rows; ++row) // row：行标
        textAt(ui, TextFormat("%d", row + 1), layout.boardX - 32, layout.boardY + row * layout.cell + (layout.cell - 18) / 2, 18, LIGHTGRAY);
    if (ui->selectedRow >= 0)
        DrawRectangleLinesEx((Rectangle){ (float)(layout.boardX + ui->selectedCol * layout.cell),
            (float)(layout.boardY + ui->selectedRow * layout.cell), (float)(layout.cell - 2), (float)(layout.cell - 2) }, 3, WHITE);
    if (ui->hintVisible)
    {
        int r = ui->hint.row - 1; // 提示起始行
        int c = ui->hint.col - 1; // 提示起始列
        for (int i = 0; i < 2; ++i) // i：两个提示位置
            DrawRectangleLinesEx((Rectangle){ (float)(layout.boardX + (c + (i && ui->hint.direction == 'D')) * layout.cell),
                (float)(layout.boardY + (r + (i && ui->hint.direction == 'S')) * layout.cell),
                (float)(layout.cell - 2), (float)(layout.cell - 2) }, 3, SKYBLUE);
    }
}

/* 显示阶段解释。ui：只读真实数据；返回：无。 */
static void drawExplanation(const InterfaceState* ui)
{
    const RoundExecution* r = &ui->round; // 本阶段数据
    int y = 386; // 放大面板的说明起始纵坐标
    textAt(ui, TextFormat("阶段：%s", getStageName(r->stage)), PANEL_X + 18, y, 22, YELLOW);
    y += 42;
    switch (r->stage)
    {
    case STAGE_SWAP:
    case STAGE_REVERT:
        textAt(ui, TextFormat("位置：(%d,%d) 与 (%d,%d)", r->fromRow + 1,
            r->fromCol + 1, r->toRow + 1, r->toCol + 1), PANEL_X + 18, y, 21, WHITE);
        textAt(ui, r->stage == STAGE_SWAP ? "先交换，再判断是否形成连续段。" :
            "无有效匹配，恢复棋盘，不计分。", PANEL_X + 18, y + 28, 21, LIGHTGRAY);
        break;
    case STAGE_MATCH:
        textAt(ui, TextFormat("匹配段：%d；段长度合计：%d", r->matches.segmentCount,
            r->matches.rawCount), PANEL_X + 18, y, 21, WHITE);
        textAt(ui, TextFormat("去重：%d格；重复：%d格", r->matches.uniqueCount,
            r->matches.rawCount - r->matches.uniqueCount), PANEL_X + 18, y + 28, 21, WHITE);
        textAt(ui, "黄色边框是标记，还未消除。", PANEL_X + 18, y + 56, 21, LIGHTGRAY);
        for (int i = 0; i < r->matches.segmentCount && i < 6; ++i) // i：显示的匹配段
        {
            const MatchSegment* s = &r->matches.segments[i]; // 当前真实匹配段
            textAt(ui, TextFormat("%s(%d,%d)，数字%d，长度%d", s->vertical ? "纵" : "横",
                s->row + 1, s->col + 1, s->value, s->length), PANEL_X + 18, y + 91 + i * 30, 20, LIGHTGRAY);
        }
        if (!r->matches.uniqueCount)
            textAt(ui, r->chain ? "没有新匹配，连锁结束。" : "没有匹配，下一步恢复交换。",
                PANEL_X + 18, y + 91, 21, GOLD);
        break;
    case STAGE_CLEAR:
        textAt(ui, TextFormat("第%d轮；每格%d分", r->chain, POINTS_PER_ITEM), PANEL_X + 18, y, 22, WHITE);
        textAt(ui, TextFormat("本轮 +%d；本回合消除%d格", r->scoreDelta, r->totalCleared),
            PANEL_X + 18, y + 30, 22, WHITE);
        textAt(ui, "空位为0，先消除再进行下落。", PANEL_X + 18, y + 64, 21, LIGHTGRAY);
        break;
    case STAGE_GRAVITY:
    case STAGE_REFILL:
        textAt(ui, TextFormat("%s数量：%d", r->stage == STAGE_GRAVITY ? "实际移动" : "新生成",
            r->changeCount), PANEL_X + 18, y, 22, WHITE);
        textAt(ui, r->stage == STAGE_GRAVITY ? "按列向下压缩，保留物品顺序。" :
            "只填空位，再检测是否产生连锁。", PANEL_X + 18, y + 30, 21, LIGHTGRAY);
        for (int i = 0; i < r->changeCount && i < 6; ++i) // i：展示真实轨迹
        {
            const CellChange* c = &r->changes[i]; // 当前记录
            const char* line = c->fromRow < 0 ? TextFormat("新数字%d → (%d,%d)", c->value, c->toRow + 1, c->toCol + 1) :
                TextFormat("%d：(%d,%d) → (%d,%d)", c->value, c->fromRow + 1, c->fromCol + 1, c->toRow + 1, c->toCol + 1); // 轨迹说明
            textAt(ui, line, PANEL_X + 18, y + 68 + i * 30, 20, LIGHTGRAY);
        }
        break;
    case STAGE_LEVEL:
        textAt(ui, r->levelResult == 2 ? "累计达到1200分，三关完成。" :
            r->levelResult == 1 ? "达到目标，进入新关，分数保留。" : "分数未达到下一关目标。",
            PANEL_X + 18, y, 21, WHITE);
        break;
    case STAGE_SHUFFLE:
        textAt(ui, r->changeCount ? "无有效交换，已重新生成棋盘。" : "有可用操作，无需重新生成。",
            PANEL_X + 18, y, 21, WHITE);
        break;
    case STAGE_FINISH:
        textAt(ui, TextFormat("回合：%d轮，%d格，%d分", r->chain, r->totalCleared,
            r->totalCleared * POINTS_PER_ITEM), PANEL_X + 18, y, 22, WHITE);
        break;
    case STAGE_ERROR:
        textAt(ui, "异常已恢复，可重新尝试。", PANEL_X + 18, y, 21, WHITE);
        break;
    default:
        textAt(ui, "选择相邻格子，观察真实算法过程。", PANEL_X + 18, y, 21, WHITE);
    }
}

/* 绘制当前页面。ui：只读状态；返回：无。 */
static void drawPage(const InterfaceState* ui)
{
    ClearBackground((Color){ 19, 26, 36, 255 });
    textAt(ui, "消消乐 · 二维数组算法演示", 48, 26, 36, RAYWHITE);
    if (ui->screen == SCREEN_SETUP)
    {
        const char* sizes[] = { "小型 10×10", "中型 20×15", "大型 30×20" }; // 尺寸按列×行显示
        textAt(ui, "C语言课程作业：同一算法，正常运行或阶段演示", 350, 162, 30, LIGHTGRAY);
        textAt(ui, "昵称：直接输入，退格删除，回车开始", 470, 240, 26, RAYWHITE);
        DrawRectangle(470, 280, 660, 58, (Color){ 42, 53, 69, 255 });
        textAt(ui, ui->name[0] ? ui->name : "请输入昵称", 488, 294, 28, WHITE);
        textAt(ui, "棋盘大小（列×行）：", 470, 360, 24, LIGHTGRAY);
        for (int size = 1; size <= BOARD_SIZE_COUNT; ++size) // size：尺寸按钮
            drawButton(ui, optionRect(size - 1, 1), sizes[size - 1], 1, ui->boardSize == size);
        textAt(ui, "消除长度 k（与棋盘大小独立）：", 470, 473, 24, LIGHTGRAY);
        for (int k = 3; k <= 5; ++k) // k：消除长度按钮
            drawButton(ui, optionRect(k - 3, 0), TextFormat("k = %d", k), 1, ui->k == k);
        drawButton(ui, (Rectangle){ 470, 606, 320, 52 }, "正常模式", 1, !ui->demo);
        drawButton(ui, (Rectangle){ 810, 606, 320, 52 }, "算法演示", 1, ui->demo);
        drawButton(ui, (Rectangle){ 470, 700, 660, 58 }, "开始（Enter）", 1, 1);
        textAt(ui, ui->message, 470, 784, 22, GOLD);
        textAt(ui, "窗口可调整大小，棋盘与点击位置同步等比缩放。", 470, 835, 22, LIGHTGRAY);
        return;
    }
    if (ui->screen == SCREEN_RESULT)
    {
        textAt(ui, ui->game.score >= 1200 ? "三关完成！" : "本局已结束", 450, 180, 40, GOLD);
        textAt(ui, TextFormat("昵称：%s；k=%d", ui->game.playerName, ui->game.matchLength), 450, 265, 28, WHITE);
        textAt(ui, TextFormat("棋盘：%s %d×%d（列×行）", getBoardSizeName(ui->game.boardSize),
            ui->game.cols, ui->game.rows), 450, 315, 28, WHITE);
        textAt(ui, TextFormat("关卡：%d；最终分数：%d", ui->game.level, ui->game.score), 450, 365, 28, WHITE);
        textAt(ui, TextFormat("有效用时：%ld秒；有效交换：%d次", getElapsedSeconds(&ui->game),
            ui->game.successfulMoves), 450, 415, 26, WHITE);
        textAt(ui, TextFormat("累计消除：%d格；最大连锁：%d轮", ui->game.totalCleared,
            ui->game.maxChain), 450, 465, 26, WHITE);
        drawButton(ui, (Rectangle){ 450, 550, 300, 58 }, "原设置重开", 1, 0);
        drawButton(ui, (Rectangle){ 770, 550, 330, 58 }, "返回设置", 1, 0);
        textAt(ui, "未完成回合退出时，棋盘和成绩已恢复。", 450, 655, 24, LIGHTGRAY);
        textAt(ui, "关闭窗口或 Esc 退出程序。", 450, 705, 24, LIGHTGRAY);
        return;
    }
    textAt(ui, TextFormat("模式：%s；棋盘：%d×%d（列×行）；黄色=匹配；白色=选中；蓝色=提示",
        ui->demo ? "算法演示" : "正常", ui->game.cols, ui->game.rows), 48, 82, 23, LIGHTGRAY);
    drawBoard(ui);
    DrawRectangle(PANEL_X, PANEL_Y, 400, 720, (Color){ 29, 40, 54, 255 });
    const char* nickname = TextFormat("昵称：%s", ui->game.playerName); // 防止长昵称越过面板
    int nameSize = 24; // 在不修改昵称的前提下适配单行空间
    while (nameSize > 14 && MeasureTextEx(ui->font, nickname, (float)nameSize, 1).x > 364) --nameSize;
    textAt(ui, nickname, PANEL_X + 18, 146, nameSize, WHITE);
    textAt(ui, TextFormat("%d×%d；k=%d；第%d关", ui->game.cols, ui->game.rows,
        ui->game.matchLength, ui->game.level), PANEL_X + 18, 190, 24, WHITE);
    textAt(ui, TextFormat("分数：%d / %d", ui->game.score, getLevelTarget(&ui->game)), PANEL_X + 18, 234, 24, WHITE);
    textAt(ui, TextFormat("有效用时：%ld秒", getElapsedSeconds(&ui->game)), PANEL_X + 18, 278, 24, WHITE);
    textAt(ui, TextFormat("本回合消除轮数：%d", ui->round.chain), PANEL_X + 18, 322, 24, GOLD);
    drawExplanation(ui);
    textAt(ui, ui->paused ? "已暂停（窗口返回后需点击继续）" : ui->message, 48, 876, 24, GOLD);
    drawButton(ui, controlRect(CONTROL_NEXT), "下一步",
        ui->demo && ui->round.active && !ui->paused && IsWindowFocused() && ui->stageTime >= ui->duration, 0);
    drawButton(ui, controlRect(CONTROL_AUTO), ui->automatic ? "停止自动" : "自动播放", ui->demo, ui->automatic);
    drawButton(ui, controlRect(CONTROL_PAUSE), ui->paused ? "继续" : "暂停", 1, ui->paused);
    drawButton(ui, controlRect(CONTROL_HINT), "提示 H",
        !ui->round.active && !ui->paused && IsWindowFocused() && ui->stageTime >= ui->duration, 0);
    drawButton(ui, controlRect(CONTROL_RESTART), "重开 R", 1, 0);
    drawButton(ui, controlRect(CONTROL_END), "结束本局", 1, 0);
    textAt(ui, "Space下一步 / A自动 / P暂停", 1080, 936, 20, LIGHTGRAY);
}

/* 图形入口。参数：无；返回：0正常退出。 */
int main(void)
{
    InterfaceState ui = { 0 }; // 当前页面与游戏状态
    ui.screen = SCREEN_SETUP;
    ui.k = 3;
    ui.boardSize = BOARD_SIZE_COUNT;
    ui.selectedRow = ui.selectedCol = -1;
    srand((unsigned int)time(NULL));
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIDDEN);
    InitWindow(800, 500, "Matchgame - C algorithm demonstration");
    fitWindowToDesktop();
    ClearWindowState(FLAG_WINDOW_HIDDEN);
    RenderTexture2D canvas = LoadRenderTexture(WINDOW_WIDTH, WINDOW_HEIGHT); // 固定逻辑画布
    if (!canvas.id) { CloseWindow(); return 1; }
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
    int lastWidth = 0, lastHeight = 0; // 只在实际窗口尺寸变化时打印记录
    SetTargetFPS(60);
    ui.font = loadCourseFont("");
    ui.customFont = ui.font.texture.id != GetFontDefault().texture.id;
    snprintf(ui.message, sizeof(ui.message), "支持昵称、k=3/4/5、阶段展示与单步。");
    while (!WindowShouldClose())
    {
        if (GetScreenWidth() != lastWidth || GetScreenHeight() != lastHeight)
        {
            lastWidth = GetScreenWidth();
            lastHeight = GetScreenHeight();
            TraceLog(LOG_INFO, "COURSE_WINDOW width=%d height=%d canvas=%dx%d",
                lastWidth, lastHeight, WINDOW_WIDTH, WINDOW_HEIGHT);
        }
        if (ui.screen == SCREEN_SETUP) handleSettings(&ui);
        else if (ui.screen == SCREEN_PLAY) updateGame(&ui);
        else
        {
            if (clicked((Rectangle){ 450, 550, 300, 58 }, 1) || IsKeyPressed(KEY_R)) startGame(&ui);
            if (clicked((Rectangle){ 770, 550, 330, 58 }, 1))
            {
                ui.screen = SCREEN_SETUP;
                snprintf(ui.message, sizeof(ui.message), "修改设置后开始新局。");
            }
        }
        BeginTextureMode(canvas);
        drawPage(&ui);
        EndTextureMode();
        CanvasViewport viewport = viewportForSize(GetScreenWidth(), GetScreenHeight()); // 与点击共用变换
        BeginDrawing();
        ClearBackground((Color){ 12, 17, 24, 255 });
        if (viewport.scale > 0.0f)
            DrawTexturePro(canvas.texture, (Rectangle){ 0, 0, WINDOW_WIDTH, -WINDOW_HEIGHT },
                viewport.area, (Vector2){ 0, 0 }, 0, WHITE);
        EndDrawing();
    }
    if (ui.screen == SCREEN_PLAY) cancelRound(&ui.game, &ui.round);
    if (ui.customFont) UnloadFont(ui.font);
    UnloadRenderTexture(canvas);
    CloseWindow();
    return 0;
}
