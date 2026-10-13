#define  _CRT_SECURE_NO_WARNINGS 1
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/*
 * 功能：初始化一局游戏的基本状态和棋盘
 * game：需要初始化的游戏状态
 * name：玩家昵称
 * matchLength：触发消除所需的最少连续物品数
 * 返回值：无
 */
void initializeGame(Game *game, const char* name, int matchLength)
{
	//snprintf() 可以限制写入长度，因此更安全。此处用于保存昵称
	snprintf(game->playerName, PLAYER_NAME_LENGTH, "%s", name);
	//初始化游戏基本参数
	game->matchLength = matchLength;
	game->score = 0;
	game->level = 1;
	//初始化全0数组
    initializeBoard(game);
    game->startTime = time(NULL);
}


/*
 * 功能：检查候选数字是否会产生初始消除
 * game：当前游戏状态
 * row：候选位置所在行
 * col：候选位置所在列
 * value：准备填入的数字
 * 返回值：1 表示会产生消除，0 表示不会
 */
static int createsInitialMatch(
    const Game* game, int row, int col, int value)
{
    int horizontal = 1; // 横向连续数量
    int vertical = 1;   // 纵向连续数量

    // offset：候选位置与左侧待检查位置之间的列距离
    // 检查左侧连续相同数字
    for (int offset = 1; offset < game->matchLength; offset++)
    {
        if (col - offset >= 0 &&
            game->board[row][col - offset] == value)
        {
            horizontal++;
        }
        else
        {
            break;
        }
    }

    // offset：候选位置与上方待检查位置之间的行距离
    // 检查上方连续相同数字
    for (int offset = 1; offset < game->matchLength; offset++)
    {
        if (row - offset >= 0 &&
            game->board[row - offset][col] == value)
        {
            vertical++;
        }
        else
        {
            break;
        }
    }

    return horizontal >= game->matchLength || vertical >= game->matchLength;
}


/*
 * 功能：生成无初始消除的随机棋盘
 * game：需要初始化棋盘的游戏状态
 * 返回值：无
 */
void initializeBoard(Game* game)
{
    // row：当前生成物品的行下标
    // 遍历每一行
    for (int row = 0; row < BOARD_ROWS; row++)
    {
        // col：当前生成物品的列下标
        // 遍历当前行的每一列
        for (int col = 0; col < BOARD_COLS; col++)
        {
            int value; // 候选随机数字

            // 不断尝试，直到不会产生消除
            do
            {
                value = rand() % ITEM_TYPES + 1;
            } while (createsInitialMatch(game, row, col, value));

            // 保存合法数字
            game->board[row][col] = value;
        }
    }
}

/*
 * 功能：在控制台显示玩家信息、游戏状态和棋盘
 * game：需要显示的游戏状态，只读
 * 返回值：无
 */
void displayGame(const Game* game)
{
	printf("\n===== Match Game =====\n");
	printf("Player: %s\n", game->playerName);
	printf("Level : %d\n", game->level);
	printf("Score : %d\n", game->score);
    printf("Time  : %ld seconds\n", getElapsedSeconds(game));
	printf("Match : %d\n", game->matchLength);


    // 打印列编号
    printf("\n    ");

    // col：当前显示的列下标
    for (int col = 0; col < BOARD_COLS; col++)
    {
        printf("%2d", col + 1);
    }

    printf("\n");

    // row：当前显示的行下标
    // 打印棋盘
    for (int row = 0; row < BOARD_ROWS; row++)
    {
        // 打印行编号
        printf("%2d |", row + 1);

        // col：当前行中正在显示的列下标
        // 打印当前行的全部数字
        for (int col = 0; col < BOARD_COLS; col++)
        {
            printf("%2d", game->board[row][col]);
        }

        printf("\n");
    }
}
/*
     * 功能：根据方向计算交换目标位置
     * row, col：当前位置（数组下标）
     * direction：移动方向
     * targetRow, targetCol：输出目标位置
     * 返回值：1合法，0非法
     */
static int getTargetPosition(int row, int col, char direction, int* targetRow, int* targetCol)
{
    // 初始目标位置等于当前位置
    *targetRow = row;
    *targetCol = col;

    // 根据方向修改目标坐标
    switch (direction)
    {
    case 'W':
        (*targetRow)--;
        break;

    case 'S':
        (*targetRow)++;
        break;

    case 'A':
        (*targetCol)--;
        break;

    case 'D':
        (*targetCol)++;
        break;

    default:
        return 0;
    }

    // 检查目标位置是否越界
    if (*targetRow < 0 || *targetRow >= BOARD_ROWS ||
        *targetCol < 0 || *targetCol >= BOARD_COLS)
    {
        return 0;
    }

    return 1;
}

/*
 * 功能：交换棋盘上两个位置的数字
 * game：当前游戏状态
 * row1, col1：第一个位置
 * row2, col2：第二个位置
 * 返回值：无
 */
static void swapItems(Game* game,int row1, int col1,int row2, int col2)
{
    int temp; // 临时存放一个数字
    temp = game->board[row1][col1];
    game->board[row1][col1] = game->board[row2][col2];
    game->board[row2][col2] = temp;
}

/*
 * 功能：检查指定位置是否形成连续消除
 * game：当前游戏状态
 * row, col：需要检查的位置（数组下标）
 * 返回值：1表示形成消除，0表示没有
 */
//static int hasMatchAt(
//    const Game* game, int row, int col)
//{
//    int value = game->board[row][col]; // 当前数字
//    int horizontal = 1; // 横向连续数量
//    int vertical = 1;   // 纵向连续数量
//
//    // c：当前向左检查的列下标
//    // 向左检查
//    for (int c = col - 1; c >= 0; c--)
//    {
//        if (game->board[row][c] == value)
//            horizontal++;
//    }
//    // c：当前向右检查的列下标
//    // 向右检查
//    for (int c = col + 1; c < BOARD_COLS; c++)
//    {
//        if (game->board[row][c] == value)
//            horizontal++;
//    }
//
//    // r：当前向上检查的行下标
//    // 向上检查
//    for (int r = row - 1; r >= 0; r--)
//    {
//        if (game->board[r][col] == value)
//            vertical++;
//    }
//
//    // r：当前向下检查的行下标
//    // 向下检查
//    for (int r = row + 1; r < BOARD_ROWS; r++)
//    {
//        if (game->board[r][col] == value)
//            vertical++;
//    }
//    // 检查横向或纵向是否达到消除长度
//    return horizontal >= game->matchLength || vertical >= game->matchLength;
//}
/*
 * 功能：检查指定位置是否形成连续消除
 * game：当前游戏状态
 * row, col：需要检查的位置（数组下标）
 * 返回值：1表示形成消除，0表示没有
 */
static int hasMatchAt(
    const Game* game, int row, int col)
{
    int value = game->board[row][col]; // 当前数字
    int horizontal = 1; // 横向连续数量
    int vertical = 1;   // 纵向连续数量

    // c：当前向左检查的列下标
    for (int c = col - 1; c >= 0; c--)
    {
        if (game->board[row][c] == value)
        {
            horizontal++;
        }
        else
        {
            break;
        }
    }

    // c：当前向右检查的列下标
    for (int c = col + 1; c < BOARD_COLS; c++)
    {
        if (game->board[row][c] == value)
        {
            horizontal++;
        }
        else
        {
            break;
        }
    }

    // r：当前向上检查的行下标
    for (int r = row - 1; r >= 0; r--)
    {
        if (game->board[r][col] == value)
        {
            vertical++;
        }
        else
        {
            break;
        }
    }

    // r：当前向下检查的行下标
    for (int r = row + 1; r < BOARD_ROWS; r++)
    {
        if (game->board[r][col] == value)
        {
            vertical++;
        }
        else
        {
            break;
        }
    }

    return horizontal >= game->matchLength ||
        vertical >= game->matchLength;
}

/*
 * 功能：执行完整的一次玩家操作
 * 保留给原来的控制台版本使用
 */
int playMove(
    Game* game, int row, int col, char direction)
{
    int result = trySwapOnly(game, row, col, direction);

    if (result == 1)
    {
        // 控制台版直接完成全部消除与连锁
        processMatches(game);
    }

    return result;
}

/*
 * 功能：查找棋盘中全部可消除的位置
 * game：当前游戏状态
 * marked：输出标记数组，1表示需要消除
 * 返回值：需要消除的格子总数
 * 说明：每轮需要扫描完整棋盘，因为下落和随机补充可能在任意位置产生连锁
 */
int findMatches(
    const Game* game,
    int marked[BOARD_ROWS][BOARD_COLS])
{
    int count = 0; // 需要消除的格子数量

    // row：初始化标记数组时的行下标
    // 初始化标记数组，全部设为0
    for (int row = 0; row < BOARD_ROWS; row++)
    {
        // col：初始化标记数组时的列下标
        for (int col = 0; col < BOARD_COLS; col++)
        {
            marked[row][col] = 0;
        }
    }

    // row：当前执行横向扫描的行下标
    // 第一轮：横向扫描
    for (int row = 0; row < BOARD_ROWS; row++)
    {
        int start = 0; // 当前连续段起点

        while (start < BOARD_COLS)
        {
            int end = start + 1; // 连续段终点的后一列

            // 向右查找连续相同数字
            while (end < BOARD_COLS &&
                game->board[row][end] ==
                game->board[row][start])
            {
                end++;
            }

            // 判断当前连续段是否满足消除条件
            if (game->board[row][start] != 0 &&
                end - start >= game->matchLength)
            {
                // col：当前连续段内需要标记的列下标
                for (int col = start; col < end; col++)
                {
                    marked[row][col] = 1;
                }
            }

            // 继续查找下一组连续数字
            start = end;
        }
    }

    // col：当前执行纵向扫描的列下标
    // 第二轮：纵向扫描
    for (int col = 0; col < BOARD_COLS; col++)
    {
        int start = 0; // 当前连续段起点

        while (start < BOARD_ROWS)
        {
            int end = start + 1; // 连续段终点的下一行

            // 向下查找连续相同数字
            while (end < BOARD_ROWS &&
                game->board[end][col] ==
                game->board[start][col])
            {
                end++;
            }

            if (game->board[start][col] != 0 &&
                end - start >= game->matchLength)
            {
                // row：当前连续段内需要标记的行下标
                for (int row = start; row < end; row++)
                {
                    marked[row][col] = 1;
                }
            }

            start = end;
        }
    }

    // row：统计已标记位置时的行下标
    // 统计标记的格子总数
    for (int row = 0; row < BOARD_ROWS; row++)
    {
        // col：统计已标记位置时的列下标
        for (int col = 0; col < BOARD_COLS; col++)
        {
            if (marked[row][col] == 1)
            {
                count++;
            }
        }
    }
    return count;
}



/*
 * 功能：清除被标记的物品，并累计分数
 * game：当前游戏状态
 * marked：待消除位置，1表示需要消除
 */
void clearMatches(
    Game *game,
    const int marked[BOARD_ROWS][BOARD_COLS])
{
    int count = 0; // 实际清除的格子数量

    for (int row = 0; row < BOARD_ROWS; row++)
    {
        for (int col = 0; col < BOARD_COLS; col++)
        {
            if (marked[row][col] == 1 &&
                game->board[row][col] != 0)
            {
                game->board[row][col] = 0;
                count++;
            }
        }
    }

    // 每消除一个物品获得10分
    game->score += count * 10;
}


/*
 * 功能：模拟重力，使所有非零物品向下压缩
 * game：当前游戏状态
 * 返回值：无
 */
void applyGravity(Game* game)
{
    // col：当前执行重力处理的列下标
    // 每一列独立处理
    for (int col = 0; col < BOARD_COLS; col++)
    {
        // 从该列最底部开始填充
        int writeRow = BOARD_ROWS - 1;

        // row：从下向上读取物品时的行下标
        // 从下往上寻找非零物品
        for (int row = BOARD_ROWS - 1; row >= 0; row--)
        {
            if (game->board[row][col] != 0)
            {
                // 将非零物品移动到目标位置
                game->board[writeRow][col] =
                    game->board[row][col];

                writeRow--;
            }
        }

        // row：当前需要清空的顶部行下标
        // 将剩余的顶部位置全部设为0
        for (int row = writeRow; row >= 0; row--)
        {
            game->board[row][col] = 0;
        }
    }
}

/*
 * 功能：随机生成新物品，填充全部空白
 * game：当前游戏状态
 * 返回值：无
 */
void fillEmptyCells(Game* game)
{
    // row：当前检查的行下标
    for (int row = 0; row < BOARD_ROWS; row++)
    {
        // col：当前检查的列下标
        for (int col = 0; col < BOARD_COLS; col++)
        {
            if (game->board[row][col] == 0)
            {
                game->board[row][col] =
                    rand() % ITEM_TYPES + 1;
            }
        }
    }
}

/*
 * 功能：处理当前棋盘中的全部消除与连锁
 * game：当前游戏状态
 * 返回值：本次操作累计消除的格子数量
 */
int processMatches(Game* game)
{
    // 本轮消除位置的标记数组
    int marked[BOARD_ROWS][BOARD_COLS];

    int count;           // 当前一轮消除的格子数量
    int totalCleared = 0; // 本次操作的累计消除数量
    int chain = 0;       // 连锁消除轮数

    while (1)
    {
        // 寻找本轮所有需要消除的位置
        count = findMatches(game, marked);

        // 没有消除项，棋盘进入稳定状态
        if (count == 0)
        {
            break;
        }

        chain++;

        // 统一清除所有标记的物品
        clearMatches(game, marked);

        // 每个被消除的物品获得10分
        totalCleared += count;

        // 让剩余物品下落
        applyGravity(game);

        // 随机补充顶部空白
        fillEmptyCells(game);

        // 临时输出，方便观察连锁过程
        printf("Chain %d: cleared %d items (+%d)\n",
            chain, count, count * 10);
    }

    return totalCleared;
}

/*
 * 功能：获取当前游戏累计用时
 * game：当前游戏状态（只读）
 * 返回值：累计用时，单位秒
 */
long getElapsedSeconds(const Game* game)
{
    time_t now;   // 当前时间
    double elapsed; // 已经过的秒数

    now = time(NULL);

    // 计算当前时间与游戏开始时间的差值
    elapsed = difftime(now, game->startTime);

    if (elapsed < 0)
    {
        return 0;
    }

    return (long)elapsed;
}

/*
 * 功能：根据累计分数更新关卡
 * game：当前游戏状态
 * 返回值：
 *   0：关卡没有变化
 *   1：进入下一关
 *   2：整局游戏胜利
 */
int updateLevel(Game* game)
{
    int oldLevel; // 更新前的关卡编号

    oldLevel = game->level;

    // 第一关：达到300分进入第二关
    if (game->level == 1 && game->score >= 300)
    {
        game->level = 2;
    }

    // 第二关：达到700分进入第三关
    if (game->level == 2 && game->score >= 700)
    {
        game->level = 3;
    }

    // 第三关：达到1200分获得最终胜利
    if (game->level == 3 && game->score >= 1200)
    {
        return 2;
    }

    // 如果关卡发生变化，则生成新棋盘
    if (game->level != oldLevel)
    {
        initializeBoard(game);
        return 1;
    }

    return 0;
}

/*
 * 功能：尝试交换，不执行消除结算
 * row、col：玩家坐标，从1开始
 * direction：W、A、S、D
 * 返回：1有效、0无效、-1非法
 */
int trySwapOnly(
    Game* game, int row, int col, char direction)
{
    int targetRow; // 目标行
    int targetCol; // 目标列

    // 转换成从0开始的数组下标
    row--;
    col--;

    // 检查起始位置
    if (row < 0 || row >= BOARD_ROWS ||
        col < 0 || col >= BOARD_COLS)
    {
        return -1;
    }

    direction = (char)toupper((unsigned char)direction);

    // 检查交换方向和目标位置
    if (!getTargetPosition(
        row, col, direction, &targetRow, &targetCol))
    {
        return -1;
    }

    // 先尝试交换
    swapItems(game, row, col, targetRow, targetCol);

    // 有效交换：保留棋盘，等待后续消除
    if (hasMatchAt(game, row, col) ||
        hasMatchAt(game, targetRow, targetCol))
    {
        return 1;
    }

    // 无效交换：恢复原来的棋盘
    swapItems(game, row, col, targetRow, targetCol);

    return 0;
}
