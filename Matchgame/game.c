#define  _CRT_SECURE_NO_WARNINGS 1
#include "game.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 根据方向求相邻位置。row/col：0起下标；direction：WASD；
 * targetRow/targetCol：输出位置；返回：是否在棋盘内。 */
static int getTargetPosition(const Game* game, int row, int col, char direction,
    int* targetRow, int* targetCol)
{
    *targetRow = row;
    *targetCol = col;
    switch (toupper((unsigned char)direction))
    {
    case 'W': --*targetRow; break;
    case 'A': --*targetCol; break;
    case 'S': ++*targetRow; break;
    case 'D': ++*targetCol; break;
    default: return 0;
    }
    return *targetRow >= 0 && *targetRow < game->rows &&
        *targetCol >= 0 && *targetCol < game->cols;
}

/* 交换两个合法位置。game：棋盘；r1/c1、r2/c2：0起下标；返回：无。 */
static void swapItems(Game* game, int r1, int c1, int r2, int c2)
{
    int value = game->board[r1][c1]; // 暂存第一个物品
    game->board[r1][c1] = game->board[r2][c2];
    game->board[r2][c2] = value;
}

/* 检查当前位置的连续段。game：只读棋盘；row/col：位置；返回：是否匹配。 */
static int hasMatchAt(const Game* game, int row, int col)
{
    int value = game->board[row][col]; // 当前物品
    int horizontal = 1; // 横向长度
    int vertical = 1; // 纵向长度
    if (value == 0) return 0;
    for (int c = col - 1; c >= 0 && game->board[row][c] == value; --c) // c：左侧列
        ++horizontal;
    for (int c = col + 1; c < game->cols && game->board[row][c] == value; ++c) // c：右侧列
        ++horizontal;
    for (int r = row - 1; r >= 0 && game->board[r][col] == value; --r) // r：上侧行
        ++vertical;
    for (int r = row + 1; r < game->rows && game->board[r][col] == value; ++r) // r：下侧行
        ++vertical;
    return horizontal >= game->matchLength || vertical >= game->matchLength;
}

/* 检查初始化候选。game：棋盘；row/col：待填位置；value：候选；返回：是否匹配。 */
static int createsInitialMatch(const Game* game, int row, int col, int value)
{
    int horizontal = 1; // 已有左侧连续数量
    int vertical = 1; // 已有上方连续数量
    for (int c = col - 1; c >= 0 && game->board[row][c] == value; --c) // c：左侧列
        ++horizontal;
    for (int r = row - 1; r >= 0 && game->board[r][col] == value; --r) // r：上方行
        ++vertical;
    return horizontal >= game->matchLength || vertical >= game->matchLength;
}

/* 见 game.h：提示只在副本模拟，不改原棋盘、不调用 rand。 */
int findLegalMove(const Game* game, LegalMove* move)
{
    Game copy = *game; // 只读查找使用的局部副本
    for (int row = 0; row < game->rows; ++row) // row：交换起点行
    {
        for (int col = 0; col < game->cols; ++col) // col：交换起点列
        {
            for (int axis = 0; axis < 2; ++axis) // axis：右、下两种无重复方向
            {
                int r = row + (axis == 1); // 目标行
                int c = col + (axis == 0); // 目标列
                int valid; // 是否会形成匹配
                if (r >= game->rows || c >= game->cols) continue;
                swapItems(&copy, row, col, r, c);
                valid = hasMatchAt(&copy, row, col) || hasMatchAt(&copy, r, c);
                swapItems(&copy, row, col, r, c);
                if (valid)
                {
                    move->row = row + 1;
                    move->col = col + 1;
                    move->direction = axis == 0 ? 'D' : 'S';
                    return 1;
                }
            }
        }
    }
    return 0;
}

void initializeBoard(Game* game)
{
    LegalMove move; // 用于确认棋盘可操作
    for (int attempt = 0; attempt < 32; ++attempt) // attempt：有限生成次数
    {
        for (int row = 0; row < game->rows; ++row) // row：生成行
        {
            for (int col = 0; col < game->cols; ++col) // col：生成列
            {
                int value; // 不产生初始消除的候选
                do { value = rand() % ITEM_TYPES + 1; }
                while (createsInitialMatch(game, row, col, value));
                game->board[row][col] = value;
            }
        }
        if (findLegalMove(game, &move)) return;
    }

    // k较大时随机棋盘可能无解。周期底板无连续段，植入一次向下交换的保证图案。
    for (int row = 0; row < game->rows; ++row) // row：周期底板行
        for (int col = 0; col < game->cols; ++col) // col：周期底板列
            game->board[row][col] = (row + col) % ITEM_TYPES + 1;
    game->board[0][0] = 2;
    game->board[1][0] = 1;
    for (int col = 1; col < game->matchLength; ++col) // col：长度为k-1的稳定段
        game->board[0][col] = 1;
}

void initializeGame(Game* game, const char* name, int matchLength)
{
    initializeGameWithSize(game, name, matchLength, BOARD_SIZE_COUNT);
}

const char* getBoardSizeName(int boardSize)
{
    const char* names[BOARD_SIZE_COUNT] = { "小型", "中型", "大型" }; // 三种规模名称
    int index = boardSize >= 1 && boardSize <= BOARD_SIZE_COUNT ? boardSize - 1 : 2; // 合法档位
    return names[index];
}

void initializeGameWithSize(Game* game, const char* name, int matchLength, int boardSize)
{
    char copiedName[PLAYER_NAME_LENGTH] = { 0 }; // 允许重开时传入原状态中的昵称
    snprintf(copiedName, sizeof(copiedName), "%s", name);
    memset(game, 0, sizeof(*game));
    memcpy(game->playerName, copiedName, sizeof(game->playerName));
    game->matchLength = matchLength >= 3 && matchLength <= 5 ? matchLength : 3;
    game->boardSize = boardSize >= 1 && boardSize <= BOARD_SIZE_COUNT ? boardSize : BOARD_SIZE_COUNT;
    game->rows = game->boardSize == 1 ? 10 : game->boardSize == 2 ? 15 : 20;
    game->cols = game->boardSize == 1 ? 10 : game->boardSize == 2 ? 20 : 30;
    game->level = 1;
    initializeBoard(game);
    game->startTime = time(NULL);
}

/* 记录真实连续段。result：输出统计；row/col/length/value：段信息；
 * vertical：方向；返回：无。 */
static void addSegment(MatchResult* result, int row, int col,
    int length, int value, int vertical)
{
    MatchSegment* segment = &result->segments[result->segmentCount++]; // 新段记录
    segment->row = row;
    segment->col = col;
    segment->length = length;
    segment->value = value;
    segment->vertical = vertical;
    result->rawCount += length;
    for (int offset = 0; offset < length; ++offset) // offset：段内偏移
        result->marked[row + (vertical ? offset : 0)][col + (vertical ? 0 : offset)] = 1;
}

int analyzeMatches(const Game* game, MatchResult* result)
{
    memset(result, 0, sizeof(*result));
    // 两种方向使用同一连续段扫描；每次end停在段后一格，避免漏掉长度大于k的段。
    for (int vertical = 0; vertical <= 1; ++vertical) // vertical：当前扫描方向
    {
        int lineCount = vertical ? game->cols : game->rows; // 需要扫描的线数
        int lineLength = vertical ? game->rows : game->cols; // 单条线长度
        for (int line = 0; line < lineCount; ++line) // line：行或列编号
        {
            int start = 0; // 当前连续段起点
            while (start < lineLength)
            {
                int row = vertical ? start : line; // 起点行
                int col = vertical ? line : start; // 起点列
                int value = game->board[row][col]; // 连续段物品
                int end = start + 1; // 连续段后一格
                while (end < lineLength &&
                    game->board[vertical ? end : line][vertical ? line : end] == value)
                    ++end;
                if (value != 0 && end - start >= game->matchLength)
                    addSegment(result, row, col, end - start, value, vertical);
                start = end;
            }
        }
    }
    for (int row = 0; row < game->rows; ++row) // row：去重统计行
        for (int col = 0; col < game->cols; ++col) // col：去重统计列
            result->uniqueCount += result->marked[row][col];
    return result->uniqueCount;
}

int clearMatches(Game* game, const MatchResult* result)
{
    int count = 0; // 实际清除的非空格数量
    for (int row = 0; row < game->rows; ++row) // row：待清除行
    {
        for (int col = 0; col < game->cols; ++col) // col：待清除列
        {
            if (result->marked[row][col] && game->board[row][col] != 0)
            {
                game->board[row][col] = 0;
                ++count;
            }
        }
    }
    game->score += count * POINTS_PER_ITEM;
    return count;
}

int applyGravity(Game* game, CellChange changes[BOARD_CELLS])
{
    int count = 0; // 实际移动数量
    for (int col = 0; col < game->cols; ++col) // col：当前压缩列
    {
        int writeRow = game->rows - 1; // 下一个写入位置
        for (int row = game->rows - 1; row >= 0; --row) // row：从底部读取的行
        {
            int value = game->board[row][col]; // 当前物品
            if (value == 0) continue;
            if (row != writeRow)
                changes[count++] = (CellChange){ row, col, writeRow, col, value };
            game->board[writeRow--][col] = value;
        }
        // 底部压缩后只清空剩余顶部位置，不影响未消除物品的相对顺序。
        for (int row = writeRow; row >= 0; --row) // row：顶部空位
            game->board[row][col] = 0;
    }
    return count;
}

int fillEmptyCells(Game* game, CellChange changes[BOARD_CELLS])
{
    int count = 0; // 新生成的物品数
    for (int row = 0; row < game->rows; ++row) // row：补充行
    {
        for (int col = 0; col < game->cols; ++col) // col：补充列
        {
            if (game->board[row][col] == 0)
            {
                int value = rand() % ITEM_TYPES + 1; // 只在实际补充阶段生成一次
                game->board[row][col] = value;
                changes[count++] = (CellChange){ -1, col, row, col, value };
            }
        }
    }
    return count;
}

int getLevelTarget(const Game* game)
{
    const int targets[LEVEL_COUNT] = { 300, 700, 1200 }; // 累计分数阈值
    int index = game->level - 1; // 当前关下标
    return targets[index >= 0 && index < LEVEL_COUNT ? index : LEVEL_COUNT - 1];
}

int updateLevel(Game* game)
{
    int oldLevel = game->level; // 更新前关卡
    if (game->score >= 1200) { game->level = 3; return 2; }
    game->level = game->score >= 700 ? 3 : game->score >= 300 ? 2 : 1;
    if (game->level != oldLevel) { initializeBoard(game); return 1; }
    return 0;
}

void setGamePaused(Game* game, int paused)
{
    time_t now = time(NULL); // 当前墙钟时间
    if (paused && !game->clockPaused)
    {
        game->pausedAt = now;
        game->clockPaused = 1;
    }
    else if (!paused && game->clockPaused)
    {
        long duration = (long)difftime(now, game->pausedAt); // 本次暂停秒数
        if (duration > 0) game->pausedSeconds += duration;
        game->clockPaused = 0;
    }
}

long getElapsedSeconds(const Game* game)
{
    time_t end = game->clockPaused ? game->pausedAt : time(NULL); // 计时终点
    long elapsed = (long)difftime(end, game->startTime) - game->pausedSeconds; // 扣除暂停
    return elapsed > 0 ? elapsed : 0;
}

int beginRound(const Game* game, RoundExecution* round, int row, int col, char direction)
{
    int targetRow; // 相邻目标行
    int targetCol; // 相邻目标列
    if (round->active || game->score >= 1200 || row < 1 || row > game->rows ||
        col < 1 || col > game->cols ||
        !getTargetPosition(game, row - 1, col - 1, direction, &targetRow, &targetCol))
        return -1;
    memset(round, 0, sizeof(*round));
    round->original = *game;
    round->fromRow = row - 1;
    round->fromCol = col - 1;
    round->toRow = targetRow;
    round->toCol = targetCol;
    round->stage = STAGE_READY;
    round->nextStage = STAGE_SWAP;
    round->active = 1;
    return 1;
}

void cancelRound(Game* game, RoundExecution* round)
{
    if (round->active)
    {
        Game restored = round->original; // 回合前状态
        restored.startTime = game->startTime;
        restored.pausedAt = game->pausedAt;
        restored.pausedSeconds = game->pausedSeconds;
        restored.clockPaused = game->clockPaused;
        *game = restored;
        round->active = 0;
    }
}

int advanceRound(Game* game, RoundExecution* round)
{
    if (!round->active) return 0;
    memcpy(round->before, game->board, sizeof(round->before));
    round->stage = round->nextStage;
    round->scoreDelta = 0;
    round->changeCount = 0;
    switch (round->stage)
    {
    case STAGE_SWAP:
        swapItems(game, round->fromRow, round->fromCol, round->toRow, round->toCol);
        round->nextStage = STAGE_MATCH;
        break;
    case STAGE_MATCH:
        analyzeMatches(game, &round->matches);
        if (round->matches.uniqueCount == 0)
            round->nextStage = round->chain == 0 ? STAGE_REVERT : STAGE_LEVEL;
        else if (round->chain >= MAX_CHAIN_ROUNDS)
            round->nextStage = STAGE_ERROR;
        else
            round->nextStage = STAGE_CLEAR;
        break;
    case STAGE_REVERT:
        swapItems(game, round->fromRow, round->fromCol, round->toRow, round->toCol);
        round->valid = 0;
        round->nextStage = STAGE_FINISH;
        break;
    case STAGE_CLEAR:
        round->totalCleared += clearMatches(game, &round->matches);
        round->scoreDelta = round->matches.uniqueCount * POINTS_PER_ITEM;
        ++round->chain;
        round->valid = 1;
        round->nextStage = STAGE_GRAVITY;
        break;
    case STAGE_GRAVITY:
        round->changeCount = applyGravity(game, round->changes);
        round->nextStage = STAGE_REFILL;
        break;
    case STAGE_REFILL:
        round->changeCount = fillEmptyCells(game, round->changes);
        round->nextStage = STAGE_MATCH;
        break;
    case STAGE_LEVEL:
        round->levelResult = updateLevel(game);
        round->nextStage = STAGE_SHUFFLE;
        break;
    case STAGE_SHUFFLE:
    {
        LegalMove move; // 用于检测稳定棋盘是否有操作
        if (round->levelResult != 2 && !findLegalMove(game, &move))
        {
            initializeBoard(game);
            round->changeCount = game->rows * game->cols;
        }
        round->nextStage = STAGE_FINISH;
        break;
    }
    case STAGE_FINISH:
        if (round->valid)
        {
            ++game->successfulMoves;
            game->totalCleared += round->totalCleared;
            if (round->chain > game->maxChain) game->maxChain = round->chain;
        }
        round->active = 0;
        if (round->levelResult == 2) setGamePaused(game, 1);
        break;
    case STAGE_ERROR:
        cancelRound(game, round);
        round->valid = -1;
        break;
    default:
        cancelRound(game, round);
        round->valid = -1;
        round->stage = STAGE_ERROR;
        break;
    }
    return 1;
}

int playMove(Game* game, int row, int col, char direction)
{
    RoundExecution round = { 0 }; // 正常模式也使用相同阶段
    if (beginRound(game, &round, row, col, direction) != 1) return -1;
    while (round.active) advanceRound(game, &round);
    return round.valid;
}

const char* getStageName(GameStage stage)
{
    switch (stage)
    {
    case STAGE_SWAP: return "交换";
    case STAGE_MATCH: return "检测匹配";
    case STAGE_REVERT: return "恢复交换";
    case STAGE_CLEAR: return "消除与计分";
    case STAGE_GRAVITY: return "重力下落";
    case STAGE_REFILL: return "随机补充";
    case STAGE_LEVEL: return "关卡结算";
    case STAGE_SHUFFLE: return "可用操作检测";
    case STAGE_FINISH: return "回合结束";
    case STAGE_ERROR: return "异常中止";
    default: return "等待操作";
    }
}
