#pragma once
#ifndef GAME_H
#define GAME_H

#include <time.h>

#define BOARD_ROWS 20          // 棋盘行数
#define BOARD_COLS 30          // 棋盘列数
#define BOARD_CELLS (BOARD_ROWS * BOARD_COLS) // 棋盘格子总数
#define ITEM_TYPES 9           // 物品种类
#define PLAYER_NAME_LENGTH 32  // 昵称的 UTF-8 字节容量
#define LEVEL_COUNT 3          // 关卡总数
#define POINTS_PER_ITEM 10     // 每个物品的固定分值
#define MAX_CHAIN_ROUNDS 100   // 防止异常随机序列无限连锁

typedef struct
{
    int board[BOARD_ROWS][BOARD_COLS]; // 1～9为物品，0为空位
    char playerName[PLAYER_NAME_LENGTH]; // 玩家昵称
    int matchLength; // 连续消除长度
    int score; // 累计分数
    int level; // 当前关卡
    time_t startTime; // 开始时间
    time_t pausedAt; // 最近暂停时间
    long pausedSeconds; // 已累计的暂停秒数
    int clockPaused; // 是否正在暂停计时
    int successfulMoves; // 已完成的有效交换数
    int totalCleared; // 已完成回合累计消除数量
    int maxChain; // 已完成回合的最大连锁轮数
} Game;

typedef struct
{
    int row; // 起始行号，从1开始
    int col; // 起始列号，从1开始
    char direction; // D或S，其他方向无需重复枚举
} LegalMove;

typedef struct
{
    int row; // 连续段起点行下标
    int col; // 连续段起点列下标
    int length; // 段长度
    int value; // 相同物品的编号
    int vertical; // 0为横向，1为纵向
} MatchSegment;

typedef struct
{
    int marked[BOARD_ROWS][BOARD_COLS]; // 待消除位置，交叉只标记一次
    MatchSegment segments[BOARD_CELLS]; // 真实匹配段
    int segmentCount; // 匹配段数
    int rawCount; // 各段长度之和，含交叉重复
    int uniqueCount; // 去重后的格子数
} MatchResult;

typedef struct
{
    int fromRow; // 移动前行；-1表示新生成
    int fromCol; // 移动前列
    int toRow; // 移动后行
    int toCol; // 移动后列
    int value; // 被移动或生成的数字
} CellChange;

typedef enum
{
    STAGE_READY, STAGE_SWAP, STAGE_MATCH, STAGE_REVERT,
    STAGE_CLEAR, STAGE_GRAVITY, STAGE_REFILL,
    STAGE_LEVEL, STAGE_SHUFFLE, STAGE_FINISH, STAGE_ERROR
} GameStage; // 一次回合的算法阶段

typedef struct
{
    GameStage stage; // 最近执行的阶段
    GameStage nextStage; // 下次推进要执行的阶段
    int active; // 是否还有阶段需要执行
    int valid; // 1为有效，0为无效，-1为异常中止
    int fromRow; // 交换起点行下标
    int fromCol; // 交换起点列下标
    int toRow; // 交换终点行下标
    int toCol; // 交换终点列下标
    int chain; // 实际发生的消除轮数
    int totalCleared; // 本回合累计消除数量
    int scoreDelta; // 本阶段得分增量
    int levelResult; // 0不变，1换关，2获胜
    int changeCount; // 本阶段移动或生成的物品数
    int before[BOARD_ROWS][BOARD_COLS]; // 本阶段执行前的棋盘
    Game original; // 完整回合开始前的快照，用于取消
    MatchResult matches; // 本轮匹配结果
    CellChange changes[BOARD_CELLS]; // 本阶段真实移动或补充记录
} RoundExecution;

/* 初始化游戏。game：输出状态；name：昵称；matchLength：3～5；返回：无。 */
void initializeGame(Game* game, const char* name, int matchLength);
/* 生成稳定且可操作的棋盘。game：当前状态；保留成绩；返回：无。 */
void initializeBoard(Game* game);
/* 扫描横纵匹配并去重。game：只读棋盘；result：输出统计；返回：去重数量。 */
int analyzeMatches(const Game* game, MatchResult* result);
/* 清除并按实际数量计分。game：状态；result：匹配；返回：实际清除数量。 */
int clearMatches(Game* game, const MatchResult* result);
/* 按列压缩并记录轨迹。game：状态；changes：输出轨迹；返回：移动数量。 */
int applyGravity(Game* game, CellChange changes[BOARD_CELLS]);
/* 填充空位并记录新数字。game：状态；changes：输出记录；返回：新增数量。 */
int fillEmptyCells(Game* game, CellChange changes[BOARD_CELLS]);
/* 在副本中查找合法交换。game：只读状态；move：输出操作；返回：是否找到。 */
int findLegalMove(const Game* game, LegalMove* move);
/* 开始回合，不立即交换。game：状态；round：初始置零的记录；row/col：1起编号；
 * direction：WASD；返回：1合法坐标，-1非法请求；匹配有效性在检测阶段判断。 */
int beginRound(const Game* game, RoundExecution* round, int row, int col, char direction);
/* 只执行一个真实阶段。game/round：当前状态与记录；返回：是否执行了阶段。 */
int advanceRound(Game* game, RoundExecution* round);
/* 取消未完成回合并恢复棋盘与成绩，保留时钟。game/round：当前状态；返回：无。 */
void cancelRound(Game* game, RoundExecution* round);
/* 正常模式包装。game：状态；row/col：1起编号；direction：WASD；
 * 返回：1有效、0无效且恢复、-1非法或异常。 */
int playMove(Game* game, int row, int col, char direction);
/* 按累计分数结算关卡。game：状态；返回：0不变、1换关、2获胜。 */
int updateLevel(Game* game);
/* 获取目标分数。game：当前状态；返回：当前关目标。 */
int getLevelTarget(const Game* game);
/* 暂停或恢复时钟。game：状态；paused：是否暂停；返回：无。 */
void setGamePaused(Game* game, int paused);
/* 获取扣除暂停后的秒数。game：只读状态；返回：累计秒数。 */
long getElapsedSeconds(const Game* game);
/* 获取中文阶段名。stage：阶段；返回：静态文本。 */
const char* getStageName(GameStage stage);

#endif
