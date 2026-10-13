#pragma once
#ifndef GAME_H
#define GAME_H

#include <time.h>

// 棋盘基本参数
#define BOARD_ROWS 20          // 棋盘总行数
#define BOARD_COLS 30          // 棋盘总列数
#define ITEM_TYPES 9           // 随机物品的种类数
#define PLAYER_NAME_LENGTH 32  // 玩家昵称数组容量
#define LEVEL_COUNT 3          // 游戏关卡总数

// 保存一局游戏运行期间的全部状态
typedef struct
{
    int board[BOARD_ROWS][BOARD_COLS]; // 棋盘各位置的物品编号，0表示空位
    char playerName[PLAYER_NAME_LENGTH]; // 玩家昵称
    int matchLength; // 触发消除所需的最少连续物品数
    int score; // 当前累计分数
    int level; // 当前关卡编号
    time_t startTime; // 本局游戏开始时间
} Game;

/*
 * 功能：初始化一局游戏的基本状态和棋盘
 * game：需要初始化的游戏状态
 * name：玩家昵称
 * matchLength：触发消除所需的最少连续物品数
 * 返回值：无
 */
void initializeGame(Game* game, const char* name, int matchlength);

/*
 * 功能：在控制台显示玩家信息、游戏状态和棋盘
 * game：需要显示的游戏状态，只读
 * 返回值：无
 */
void displayGame(const Game* game);

/*
 * 功能：生成不存在初始消除的随机棋盘
 * game：需要初始化棋盘的游戏状态
 * 返回值：无
 */
void initializeBoard(Game* game);

/*
 * 功能：验证并执行一次交换，成功时处理全部消除和连锁
 * game：当前游戏状态
 * row：玩家输入的行号，从1开始
 * col：玩家输入的列号，从1开始
 * direction：交换方向，使用W、A、S或D
 * 返回值：1表示成功，0表示无效交换，-1表示非法操作
 */
int playMove(Game* game, int row, int col, char direction);

/*
 * 功能：处理棋盘中的全部消除、下落、补充和连锁
 * game：当前游戏状态
 * 返回值：本次处理累计消除的格子数量
 */
int processMatches(Game* game);

/*
 * 功能：获取本局游戏的累计用时
 * game：当前游戏状态，只读
 * 返回值：从游戏开始到当前经过的秒数
 */

 /*
  * 只尝试交换，不执行消除结算
  * 返回：1有效，0无效并恢复，-1非法
  */
int trySwapOnly(Game* game, int row, int col, char direction);

// 查找所有可消除格子，返回数量
int findMatches(
    const Game* game,
    int marked[BOARD_ROWS][BOARD_COLS]);

// 清除标记的格子，并计算分数
void clearMatches(
    Game* game,
    const int marked[BOARD_ROWS][BOARD_COLS]);

// 将剩余数字向下压缩
void applyGravity(Game* game);

// 填充所有空白位置
void fillEmptyCells(Game* game);

long getElapsedSeconds(const Game* game);

/*
 * 功能：根据累计分数检查并更新关卡
 * game：当前游戏状态
 * 返回值：0表示关卡不变，1表示升级，2表示整局获胜
 */
int updateLevel(Game* game);
#endif
