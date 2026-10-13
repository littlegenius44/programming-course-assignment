#include "raylib.h"
#include "../game.h"
#include "raymath.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>

// 窗口与棋盘的显示参数
#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 760
#define CELL_SIZE 28
#define BOARD_X 40
#define BOARD_Y 105
#define PANEL_X 920
#define SWAP_DURATION  0.30f
#define CLEAR_DURATION 0.35f
#define FALL_DURATION  0.35f
#define REFILL_DURATION 0.45f

typedef enum
{
    ANIM_IDLE,       // 等待玩家操作
    ANIM_FORWARD,    // 正向交换动画
    ANIM_BACKWARD,   // 无效交换回退
    ANIM_CLEARING,   // 匹配高亮及消失动画
    ANIM_FALLING,     // 重力下落动画
    ANIM_REFILLING   // 新物品从顶部进入
} AnimationState;

/*
 * 功能：记录一次交换动画的完整状态
 */
typedef struct
{
    AnimationState state; // 当前动画阶段

    int fromRow; // 第一个物品的行
    int fromCol; // 第一个物品的列
    int toRow;   // 第二个物品的行
    int toCol;   // 第二个物品的列

    int fromValue; // 第一个物品的数字
    int toValue;   // 第二个物品的数字

    char direction; // 交换方向

    float elapsed;  // 已播放时间（秒）
    float duration; // 单程动画时长（秒）
} SwapAnimation;



/*
 * 功能：绘制带透明度的数字方块
 * value：物品编号
 * x、y：屏幕位置
 * tileColors：物品颜色表
 * opacity：透明度，0表示不可见，1表示完全显示
 */
static void drawTile(
    int value,
    float x,
    float y,
    const Color tileColors[ITEM_TYPES],
    float opacity)
{
    if (value < 1 || value > ITEM_TYPES)
    {
        return;
    }

    Color color = Fade(tileColors[value - 1], opacity);

    // 绘制方块
    DrawRectangleRec(
        (Rectangle){
            x, y,
            CELL_SIZE - 2,
            CELL_SIZE - 2
        },
        color
    );

    // 将数字转换为字符串
    char label[2] = {
        (char)('0' + value), '\0'
    };

    int fontSize = 18;
    int textWidth = MeasureText(label, fontSize);

    // 文字使用相同的透明度
    DrawText(
        label,
        (int)(x + (CELL_SIZE - 2 - textWidth) / 2),
        (int)(y + 4),
        fontSize,
        Fade(WHITE, opacity)
    );
}


int main(void)
{
    Game game; // 当前游戏状态
    int selectedRow = -1; // 当前选中的行
    int selectedCol = -1; // 当前选中的列

    int gameWon = 0; // 是否取得胜利

    const char* message = "Select a tile";
    SwapAnimation anim = { 0 };

    anim.state = ANIM_IDLE;
    anim.duration = SWAP_DURATION;

    // 标记当前需要消除的格子
    int marked[BOARD_ROWS][BOARD_COLS] = { 0 };

    // 记录执行重力前的棋盘状态
    int fallStart[BOARD_ROWS][BOARD_COLS] = { 0 };

    // 补充之前的棋盘：顶部空位为0
    int refillStart[BOARD_ROWS][BOARD_COLS] = { 0 };

    // 当前一次交换中发生的消除轮数
    int chainCount = 0;

    // 九种物品对应的颜色
    Color tileColors[ITEM_TYPES] = {
        {201, 87, 99, 255},
        {215, 139, 78, 255},
        {196, 168, 71, 255},
        {86, 164, 142, 255},
        {82, 143, 189, 255},
        {121, 122, 195, 255},
        {160, 115, 181, 255},
        {189, 99, 138, 255},
        {102, 158, 98, 255}
    };

    // 初始化随机数和游戏数据
    srand((unsigned int)time(NULL));
    initializeGame(&game, "DemoPlayer", 3);

    // 初始化图形窗口
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
        "MatchGame - raylib");

    SetTargetFPS(60);

    // 图形主循环
    while (!WindowShouldClose())
    {

        // 仅在没有播放动画时允许鼠标操作
        if (!gameWon &&
            anim.state == ANIM_IDLE &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            Vector2 mouse = GetMousePosition();

            // 判断鼠标是否位于棋盘内
            if (mouse.x >= BOARD_X &&
                mouse.x < BOARD_X + BOARD_COLS * CELL_SIZE &&
                mouse.y >= BOARD_Y &&
                mouse.y < BOARD_Y + BOARD_ROWS * CELL_SIZE)
            {
                int col = (int)(mouse.x - BOARD_X) / CELL_SIZE;
                int row = (int)(mouse.y - BOARD_Y) / CELL_SIZE;

                // 第一次点击：选中物品
                if (selectedRow == -1)
                {
                    selectedRow = row;
                    selectedCol = col;
                    message = "Tile selected";
                }

                // 再次点击同一个位置：取消选中
                else if (selectedRow == row &&
                    selectedCol == col)
                {
                    selectedRow = -1;
                    selectedCol = -1;
                    message = "Selection cancelled";
                }

                else
                {
                    // 计算第二次点击相对第一次的位置差
                    int dr = row - selectedRow;
                    int dc = col - selectedCol;

                    // 必须是上下左右相邻格子
                    if (abs(dr) + abs(dc) == 1)
                    {
                        // 记录交换的两个位置
                        anim.fromRow = selectedRow;
                        anim.fromCol = selectedCol;
                        anim.toRow = row;
                        anim.toCol = col;

                        // 记录交换前的数字
                        anim.fromValue =
                            game.board[selectedRow][selectedCol];

                        anim.toValue =
                            game.board[row][col];

                        // 确定交换方向
                        if (dr == -1)
                            anim.direction = 'W';
                        else if (dr == 1)
                            anim.direction = 'S';
                        else if (dc == -1)
                            anim.direction = 'A';
                        else
                            anim.direction = 'D';

                        // 启动正向交换动画
                        anim.elapsed = 0.0f;
                        anim.state = ANIM_FORWARD;

                        // 清除选中状态
                        selectedRow = -1;
                        selectedCol = -1;

                        message = "Swapping...";
                    }
                    else
                    {
                        // 点击不相邻的格子，切换选中位置
                        selectedRow = row;
                        selectedCol = col;
                        message = "Tile selected";
                    }
                }
            }
        }



        // 每帧更新动画状态
        if (anim.state != ANIM_IDLE)
        {
            anim.elapsed += GetFrameTime();

            if (anim.elapsed >= anim.duration)
            {
                // 阶段1：正向交换动画完成
                if (anim.state == ANIM_FORWARD)
                {
                    int result = trySwapOnly(
                        &game,
                        anim.fromRow + 1,
                        anim.fromCol + 1,
                        anim.direction
                    );

                    if (result == 1)
                    {
                        chainCount = 1; // 第一次消除为第1轮
                        // 找到交换后形成的连续匹配
                        findMatches(&game, marked);

                        anim.state = ANIM_CLEARING;
                        anim.elapsed = 0.0f;
                        anim.duration = CLEAR_DURATION;

                        message = "Clearing...";
                    }
                    else if (result == 0)
                    {
                        // 无效交换，开始回退动画
                        anim.state = ANIM_BACKWARD;
                        anim.elapsed = 0.0f;
                        anim.duration = SWAP_DURATION;

                        message = "Reverting...";
                    }
                    else
                    {
                        anim.state = ANIM_IDLE;
                        message = "Invalid move";
                    }
                }

                // 阶段2：无效交换回退完成
                else if (anim.state == ANIM_BACKWARD)
                {
                    anim.state = ANIM_IDLE;
                    message = "Move cancelled";
                }

                // 阶段3：消除动画完成
                else if (anim.state == ANIM_CLEARING)
                {
                    // 真正清除物品并计分
                    clearMatches(&game, marked);

                    // 保存执行重力前的棋盘
                    memcpy(fallStart, game.board,
                        sizeof(fallStart));

                    // 核心算法计算下落后的棋盘
                    applyGravity(&game);

                    // 开始播放重力动画
                    anim.state = ANIM_FALLING;
                    anim.elapsed = 0.0f;
                    anim.duration = FALL_DURATION;

                    message = "Falling...";
                }

                // 阶段4：重力动画完成
                else if (anim.state == ANIM_FALLING)
                {
                    // 暂时直接补充随机物品
                    fillEmptyCells(&game);

                    // 判断是否触发连锁消除
                    int count = findMatches(&game, marked);

                    if (count > 0)
                    {
                        anim.state = ANIM_CLEARING;
                        anim.elapsed = 0.0f;
                        anim.duration = CLEAR_DURATION;

                        message = "Chain reaction!";
                    }
                    else
                    {
                        anim.state = ANIM_IDLE;
                        message = "Move successful!";

                        int levelResult = updateLevel(&game);

                        if (levelResult == 1)
                        {
                            message = "Level up!";
                        }
                        else if (levelResult == 2)
                        {
                            gameWon = 1;
                            message = "YOU WIN!";
                        }
                    }
                }

            }
        }
        BeginDrawing();

        ClearBackground((Color) { 20, 33, 49, 255 });

        // 绘制游戏标题
        DrawText("MATCH GAME", 40, 35, 36, RAYWHITE);



        // 绘制棋盘中的静止物品
        // 下落动画由专用绘制代码处理
        if (anim.state != ANIM_FALLING && anim.state != ANIM_REFILLING)
        {
            for (int row = 0; row < BOARD_ROWS; row++)
            {
                for (int col = 0; col < BOARD_COLS; col++)
                {
                    // 交换期间跳过两个运动中的物品
                    if ((anim.state == ANIM_FORWARD ||
                        anim.state == ANIM_BACKWARD) &&
                        ((row == anim.fromRow &&
                            col == anim.fromCol) ||
                            (row == anim.toRow &&
                                col == anim.toCol)))
                    {
                        continue;
                    }

                    int value = game.board[row][col];

                    float x = BOARD_X + col * CELL_SIZE;
                    float y = BOARD_Y + row * CELL_SIZE;

                    float opacity = 1.0f;

                    // 消除阶段逐渐降低透明度
                    if (anim.state == ANIM_CLEARING &&
                        marked[row][col] == 1)
                    {
                        opacity = 1.0f -
                            anim.elapsed / anim.duration;

                        opacity = Clamp(opacity, 0.0f, 1.0f);
                    }

                    drawTile(
                        value, x, y,
                        tileColors, opacity
                    );

                    // 匹配区域高亮
                    if (anim.state == ANIM_CLEARING &&
                        marked[row][col] == 1)
                    {
                        DrawRectangleLinesEx(
                            (Rectangle) {
                            x, y,
                                CELL_SIZE - 2,
                                CELL_SIZE - 2
                        },
                            2.0f,
                            Fade(YELLOW, opacity)
                        );
                    }
                }
            }
        }


        // 绘制正在交换的两个物品
        if (anim.state == ANIM_FORWARD || anim.state == ANIM_BACKWARD)
        {
            // 当前动画的归一化进度：0到1
            float t = anim.elapsed / anim.duration;

            if (t > 1.0f)
                t = 1.0f;

            // 回退阶段沿相反方向运动
            if (anim.state == ANIM_BACKWARD)
                t = 1.0f - t;

            // 两个格子的原始屏幕坐标
            float x1 = BOARD_X + anim.fromCol * CELL_SIZE;
            float y1 = BOARD_Y + anim.fromRow * CELL_SIZE;

            float x2 = BOARD_X + anim.toCol * CELL_SIZE;
            float y2 = BOARD_Y + anim.toRow * CELL_SIZE;

            // 线性插值计算当前位置
            float moveX1 = x1 + (x2 - x1) * t;
            float moveY1 = y1 + (y2 - y1) * t;

            float moveX2 = x2 + (x1 - x2) * t;
            float moveY2 = y2 + (y1 - y2) * t;

            // 绘制两个移动中的方块
            drawTile(
                anim.fromValue,
                moveX1, moveY1,
                tileColors,1.0f
            );

            drawTile(
                anim.toValue,
                moveX2, moveY2,
                tileColors,1.0f
            );
        }

        // 绘制重力下落动画
        if (anim.state == ANIM_FALLING)
        {
            float t = anim.elapsed / anim.duration;

            if (t > 1.0f)
                t = 1.0f;

            // 平滑插值：开始和结束时速度较慢
            t = t * t * (3.0f - 2.0f * t);

            // 每一列独立计算运动轨迹
            for (int col = 0; col < BOARD_COLS; col++)
            {
                int writeRow = BOARD_ROWS - 1;

                // 从下往上寻找所有未被消除的物品
                for (int row = BOARD_ROWS - 1; row >= 0; row--)
                {
                    int value = fallStart[row][col];

                    if (value == 0)
                    {
                        continue;
                    }

                    // 计算原始屏幕位置
                    float x = BOARD_X + col * CELL_SIZE;
                    float startY = BOARD_Y + row * CELL_SIZE;

                    // 计算重力压缩后的目标位置
                    float targetY =
                        BOARD_Y + writeRow * CELL_SIZE;

                    // 根据动画进度插值
                    float currentY =
                        startY + (targetY - startY) * t;

                    // 绘制运动中的物品
                    drawTile(
                        value,
                        x,
                        currentY,
                        tileColors,
                        1.0f
                    );

                    // 下一个物品应该落到更上方
                    writeRow--;
                }
            }
        }


        // 绘制顶部补充动画
        if (anim.state == ANIM_REFILLING)
        {
            float t = anim.elapsed / anim.duration;

            // 限制进度范围
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;

            // Smoothstep平滑缓动
            t = t * t * (3.0f - 2.0f * t);

            // 裁剪到棋盘内，防止物品出现在标题区域
            BeginScissorMode(
                BOARD_X,
                BOARD_Y,
                BOARD_COLS * CELL_SIZE,
                BOARD_ROWS * CELL_SIZE
            );

            // 每一列单独处理
            for (int col = 0; col < BOARD_COLS; col++)
            {
                // 统计该列顶部有多少个空位
                int emptyCount = 0;

                while (emptyCount < BOARD_ROWS &&
                    refillStart[emptyCount][col] == 0)
                {
                    emptyCount++;
                }

                // 绘制该列所有物品
                for (int row = 0; row < BOARD_ROWS; row++)
                {
                    int value = game.board[row][col];

                    float x = BOARD_X + col * CELL_SIZE;
                    float targetY = BOARD_Y + row * CELL_SIZE;
                    float currentY = targetY;

                    // 补充前是空位，说明这是新生成的物品
                    if (refillStart[row][col] == 0)
                    {
                        // 从棋盘上方开始运动
                        float startY =
                            BOARD_Y +
                            (row - emptyCount) * CELL_SIZE;

                        // 插值计算当前位置
                        currentY =
                            startY + (targetY - startY) * t;
                    }

                    // 原有物品保持不动，新物品逐渐下落
                    drawTile(
                        value,
                        x,
                        currentY,
                        tileColors,
                        1.0f
                    );
                }
            }

            EndScissorMode();
        }

        // 绘制选中格子的高亮边框
        if (selectedRow >= 0 && selectedCol >= 0)
        {
            int x = BOARD_X + selectedCol * CELL_SIZE;
            int y = BOARD_Y + selectedRow * CELL_SIZE;

            DrawRectangleLinesEx(
                (Rectangle) {
                (float)x,
                    (float)y,
                    CELL_SIZE - 2,
                    CELL_SIZE - 2
            },
                3.0f,
                YELLOW
            );
        }

        // 棋盘外边框
        DrawRectangleLines(
            BOARD_X - 2,
            BOARD_Y - 2,
            BOARD_COLS * CELL_SIZE + 2,
            BOARD_ROWS * CELL_SIZE + 2,
            LIGHTGRAY
        );

        // 右侧信息面板
        DrawRectangle(
            PANEL_X, BOARD_Y,
            240, 280,
            (Color) {
            37, 59, 80, 255
        }
        );

        DrawText("PLAYER", PANEL_X + 20,
            BOARD_Y + 20, 18, LIGHTGRAY);

        DrawText(game.playerName, PANEL_X + 20,
            BOARD_Y + 48, 20, WHITE);

        DrawText(
            TextFormat("LEVEL: %d", game.level),
            PANEL_X + 20, BOARD_Y + 95,
            22, WHITE
        );

        DrawText(
            TextFormat("SCORE: %d", game.score),
            PANEL_X + 20, BOARD_Y + 140,
            22, WHITE
        );

        DrawText(
            TextFormat("TIME: %ld s",
                getElapsedSeconds(&game)),
            PANEL_X + 20, BOARD_Y + 185,
            22, WHITE
        );


        // 显示本次交换的消除轮数
        DrawText(
            TextFormat("CHAIN: %d", chainCount),
            PANEL_X,
            BOARD_Y + 315,
            20,
            GOLD
        );

        
        // 显示最近一次操作的状态
        DrawText(
            message,
            PANEL_X,
            BOARD_Y + 370,
            18,
            YELLOW
        );

        // 胜利提示
        if (gameWon)
        {
            DrawRectangle(
                250, 280, 680, 160,
                (Color){10, 20, 35, 235}
            );

            DrawText(
                "YOU WIN!",
                465, 330, 48, GOLD
            );
        }
        DrawFPS(WINDOW_WIDTH - 110, 30);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
