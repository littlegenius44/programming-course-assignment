#define  _CRT_SECURE_NO_WARNINGS 1
#include "game.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#endif

/* 跳过空白。text：输入文本；返回：首个非空白位置。 */
static const char* skipSpace(const char* text)
{
    while (isspace((unsigned char)*text)) ++text;
    return text;
}

/* 严格读取完整行。text：缓冲区；capacity：容量；
 * 返回：1正常，0输入结束，-1超长且已丢弃剩余内容。 */
static int readLine(char* text, int capacity)
{
    if (fgets(text, capacity, stdin) == NULL) return 0;
    if (strchr(text, '\n') == NULL)
    {
        int ch = getchar(); // 用于清理超长行的字符
        if (ch != EOF)
        {
            while (ch != '\n' && ch != EOF) ch = getchar();
            return -1;
        }
    }
    text[strcspn(text, "\r\n")] = '\0';
    return 1;
}

/* 解析范围内整数。text：输入并更新游标；min/max：范围；value：输出；
 * 返回：是否为完整的整数词项，拒绝溢出及3abc。 */
static int parseInteger(const char** text, int min, int max, int* value)
{
    char* end; // strtol停止位置
    long number; // 转换结果，用于检查int范围
    const char* begin = skipSpace(*text); // 当前词项起点
    errno = 0;
    number = strtol(begin, &end, 10);
    if (begin == end || errno == ERANGE || number < min || number > max ||
        (*end != '\0' && !isspace((unsigned char)*end))) return 0;
    *value = (int)number;
    *text = end;
    return 1;
}

/* 判断单字母命令。text：输入；command：期望字母；返回：是否完全匹配。 */
static int isCommand(const char* text, char command)
{
    text = skipSpace(text);
    return toupper((unsigned char)*text) == command && *skipSpace(text + 1) == '\0';
}

/* 读取指定范围的单整数。prompt：提示；min/max：范围；value：输出；
 * 返回：0表示输入结束，1表示成功。 */
static int readNumber(const char* prompt, int min, int max, int* value)
{
    char input[128]; // 一行设置输入
    for (;;)
    {
        const char* cursor; // 整数解析游标
        int status; // 完整行读取结果
        printf("%s", prompt);
        fflush(stdout);
        status = readLine(input, sizeof(input));
        if (status == 0) return 0;
        cursor = input;
        if (status == 1 && parseInteger(&cursor, min, max, value) && !*skipSpace(cursor))
            return 1;
        printf("输入无效，请只输入 %d～%d 的整数。\n", min, max);
    }
}

/* 读取非空UTF-8昵称。name：输出缓冲；返回：是否成功。 */
static int readName(char name[PLAYER_NAME_LENGTH])
{
    char input[128]; // 原始昵称输入
    for (;;)
    {
        const char* begin; // 去除首部空白后的昵称
        size_t length; // 昵称字节数
        int status; // 行读取结果
        printf("请输入昵称（最多 %d 个UTF-8字节）：", PLAYER_NAME_LENGTH - 1);
        fflush(stdout);
        status = readLine(input, sizeof(input));
        if (!status) return 0;
        begin = skipSpace(input);
        length = strlen(begin);
        while (length && isspace((unsigned char)begin[length - 1])) --length;
        if (status == 1 && length > 0 && length < PLAYER_NAME_LENGTH)
        {
            memcpy(name, begin, length);
            name[length] = '\0';
            return 1;
        }
        puts("昵称不能为空或过长，请重新输入。");
    }
}

/* 显示当前棋盘。game：只读状态；matches：可为NULL的匹配标记；返回：无。 */
static void displayGame(const Game* game, const MatchResult* matches)
{
#ifdef _WIN32
    // 交互控制台清屏；重定向测试日志时保留每个阶段输出。
    if (_isatty(_fileno(stdin)) && _isatty(_fileno(stdout))) system("cls");
#endif
    printf("\n========== 消消乐 · 二维数组算法演示 ==========\n");
    printf("昵称：%s  棋盘：%s %d×%d（列×行）  k=%d  第%d关  分数：%d/%d  用时：%ld秒\n",
        game->playerName, getBoardSizeName(game->boardSize), game->cols, game->rows,
        game->matchLength, game->level,
        game->score, getLevelTarget(game), getElapsedSeconds(game));
    printf("    ");
    for (int col = 0; col < game->cols; ++col) // col：当前棋盘列编号
        printf("%3d", col + 1);
    putchar('\n');
    for (int row = 0; row < game->rows; ++row) // row：当前棋盘行
    {
        printf("%2d |", row + 1);
        for (int col = 0; col < game->cols; ++col) // col：当前棋盘列
        {
            int value = game->board[row][col]; // 当前位置物品
            if (matches && matches->marked[row][col]) printf("[%d]", value);
            else if (value == 0) printf(" . ");
            else printf(" %d ", value);
        }
        putchar('\n');
    }
    puts("数字=物品；[数字]=检测到的匹配；.=已经消除的空位。");
}

/* 显示一个真实算法阶段。game：状态；round：阶段结果；返回：无。 */
static void displayStage(const Game* game, const RoundExecution* round)
{
    displayGame(game, round->stage == STAGE_MATCH ? &round->matches : NULL);
    printf("\n【阶段：%s】 已发生消除轮数：%d\n", getStageName(round->stage), round->chain);
    switch (round->stage)
    {
    case STAGE_SWAP:
        printf("交换 (%d,%d) 的 %d 与 (%d,%d) 的 %d。\n",
            round->fromRow + 1, round->fromCol + 1, round->before[round->fromRow][round->fromCol],
            round->toRow + 1, round->toCol + 1, round->before[round->toRow][round->toCol]);
        puts("只交换物品，还没有消除或计分。");
        break;
    case STAGE_MATCH:
        printf("横纵匹配段：%d；段长度合计：%d；交叉重复：%d；实际待消除：%d。\n",
            round->matches.segmentCount, round->matches.rawCount,
            round->matches.rawCount - round->matches.uniqueCount, round->matches.uniqueCount);
        for (int i = 0; i < round->matches.segmentCount && i < 8; ++i) // i：展示匹配段
        {
            const MatchSegment* s = &round->matches.segments[i]; // 当前连续段
            printf("  %s：(%d,%d)～(%d,%d)，数字%d，长度%d。\n",
                s->vertical ? "纵向" : "横向", s->row + 1, s->col + 1,
                s->row + 1 + (s->vertical ? s->length - 1 : 0),
                s->col + 1 + (s->vertical ? 0 : s->length - 1), s->value, s->length);
        }
        if (round->matches.segmentCount > 8) puts("  其余匹配位置已在棋盘中标出。");
        if (round->matches.uniqueCount == 0)
            puts(round->chain ? "没有新匹配，连锁结束。" : "没有形成连续匹配，下一阶段恢复交换。");
        else puts("检测只作标记；下一阶段才清除这些位置。");
        break;
    case STAGE_REVERT:
        puts("交换无法形成消除，棋盘已恢复，分数不变。");
        break;
    case STAGE_CLEAR:
        printf("第%d轮消除，增量：%d分；本回合累计消除：%d格。\n",
            round->chain, round->scoreDelta, round->totalCleared);
        puts("被标记的位置变为0，界面使用点号显示空位。交叉只计一次。");
        break;
    case STAGE_GRAVITY:
    case STAGE_REFILL:
        printf("%s物品数量：%d。\n",
            round->stage == STAGE_GRAVITY ? "实际移动" : "新生成", round->changeCount);
        for (int i = 0; i < round->changeCount && i < 8; ++i) // i：展示移动或补充记录
        {
            const CellChange* c = &round->changes[i]; // 当前真实变化
            if (c->fromRow < 0) printf("  新数字%d → (%d,%d)\n", c->value, c->toRow + 1, c->toCol + 1);
            else printf("  数字%d：(%d,%d) → (%d,%d)\n",
                c->value, c->fromRow + 1, c->fromCol + 1, c->toRow + 1, c->toCol + 1);
        }
        if (round->changeCount > 8) printf("  另有%d个变化，均已反映在棋盘中。\n", round->changeCount - 8);
        puts(round->stage == STAGE_GRAVITY ?
            "按列从底部读取，保持剩余物品顺序；顶部暂时保留空位。" :
            "只填充空位；下一阶段重新扫描，判断是否产生连锁。");
        break;
    case STAGE_LEVEL:
        puts(round->levelResult == 2 ? "累计达到1200分，完成三关。" :
            round->levelResult == 1 ? "达到关卡目标，新关卡棋盘已生成，分数保留。" :
            "本回合结束，累计分数尚未达到下一关目标。");
        break;
    case STAGE_SHUFFLE:
        puts(round->changeCount ? "没有有效交换，已重新生成稳定且可操作的棋盘，成绩保留。" :
            "棋盘有可用操作，或本局已获胜，无需重新生成。");
        break;
    case STAGE_ERROR:
        puts("异常连锁超过上限，本回合已恢复，不保留部分得分。");
        break;
    default:
        printf("本回合：%d轮消除，%d格，合计%d分。\n",
            round->chain, round->totalCleared, round->totalCleared * POINTS_PER_ITEM);
        break;
    }
}

/* 等待下一阶段或确认结束。game：用于暂停计时；finished：回合是否结束；
 * 返回：1回车、0退出或EOF，已完成回合不会回滚。 */
static int waitForNextStage(Game* game, int finished)
{
    char input[128]; // 单步命令
    setGamePaused(game, 1);
    for (;;)
    {
        int status; // 本次读取结果
        puts(finished ? "本回合已结束，按回车返回操作；输入 Q 结束本局。" :
            "按回车执行下一阶段；输入 Q 结束本局（未完成回合会恢复）。");
        fflush(stdout);
        status = readLine(input, sizeof(input));
        if (status == 0 || (status == 1 && isCommand(input, 'Q'))) return 0;
        if (status == 1 && !*skipSpace(input))
        {
            if (game->score < 1200) setGamePaused(game, 0);
            return 1;
        }
        puts("演示期间只接受回车或单独的 Q。");
    }
}

/* 展示最终统计。game：已暂停状态；返回：无。 */
static void displayResult(const Game* game)
{
    printf("\n========== 本局结束 ==========\n");
    printf("结果：%s\n昵称：%s\n关卡：%d\n分数：%d\n用时：%ld秒\n",
        game->score >= 1200 ? "三关完成" : "主动退出或输入结束",
        game->playerName, game->level, game->score, getElapsedSeconds(game));
    printf("有效交换：%d次；累计消除：%d格；最大连锁：%d轮。\n",
        game->successfulMoves, game->totalCleared, game->maxChain);
    printf("棋盘：%s %d×%d（列×行）；k=%d。\n",
        getBoardSizeName(game->boardSize), game->cols, game->rows, game->matchLength);
}

/* 主入口。参数：无；返回：0正常退出。 */
int main(void)
{
    Game game; // 当前完整状态
    RoundExecution round = { 0 }; // 两种模式共用阶段执行器
    char name[PLAYER_NAME_LENGTH]; // 可在重开时复用的昵称
    char input[128]; // 命令输入
    char feedback[256] = ""; // 清屏后保留上一条提示或回合结果
    int k; // 消除长度
    int boardSize; // 棋盘大小，与消除长度独立选择
    int mode; // 1正常、2演示
    int running = 1; // 是否继续运行程序
    int needsSettings = 1; // 是否需要填写设置
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    srand((unsigned int)time(NULL));
    while (running)
    {
        if (needsSettings)
        {
            puts("\n消消乐课程作业：正常运行 / 分阶段算法演示");
            if (!readName(name) ||
                !readNumber("请选择棋盘（1小型10×10 / 2中型20×15 / 3大型30×20，列×行）：",
                    1, BOARD_SIZE_COUNT, &boardSize) ||
                !readNumber("请选择消除长度 k（3/4/5）：", 3, 5, &k) ||
                !readNumber("请选择运行模式（1正常 / 2演示）：", 1, 2, &mode)) break;
            needsSettings = 0;
        }
        initializeGameWithSize(&game, name, k, boardSize);
        memset(&round, 0, sizeof(round));
        feedback[0] = '\0';
        for (;;)
        {
            const char* cursor; // 操作解析游标
            int row; // 输入的行号
            int col; // 输入的列号
            int status; // 完整行读取结果
            char direction; // 交换方向
            displayGame(&game, NULL);
            if (feedback[0]) puts(feedback);
            printf("模式：%s。输入 行 列 WASD；H提示；R重开；Q结束本局。\n",
                mode == 2 ? "演示（回车下一阶段）" : "正常");
            fflush(stdout);
            status = readLine(input, sizeof(input));
            if (status == 0) { running = 0; break; }
            if (status < 0)
            { snprintf(feedback, sizeof(feedback), "输入过长，已丢弃本行。"); continue; }
            if (isCommand(input, 'Q')) break;
            if (isCommand(input, 'R'))
            {
                initializeGameWithSize(&game, name, k, boardSize);
                snprintf(feedback, sizeof(feedback), "已按原设置重新开始。");
                continue;
            }
            if (isCommand(input, 'H'))
            {
                LegalMove move; // 提示的合法操作
                if (findLegalMove(&game, &move))
                    snprintf(feedback, sizeof(feedback),
                        "提示：%d %d %c（只提示，不交换、不计分）。",
                        move.row, move.col, move.direction);
                else
                {
                    initializeBoard(&game);
                    snprintf(feedback, sizeof(feedback),
                        "无可用操作，已自动重新生成棋盘，成绩保留。");
                }
                continue;
            }
            cursor = input;
            if (!parseInteger(&cursor, 1, game.rows, &row) ||
                !parseInteger(&cursor, 1, game.cols, &col))
            {
                snprintf(feedback, sizeof(feedback),
                    "格式或坐标错误，行范围1～%d，列范围1～%d；例如 3 5 D。", game.rows, game.cols);
                continue;
            }
            cursor = skipSpace(cursor);
            direction = *cursor;
            if (!direction || *skipSpace(cursor + 1) ||
                beginRound(&game, &round, row, col, direction) != 1)
            { snprintf(feedback, sizeof(feedback), "方向错误或目标越界，棋盘未改变。"); continue; }

            while (round.active)
            {
                advanceRound(&game, &round);
                if (mode == 2)
                {
                    setGamePaused(&game, 1);
                    displayStage(&game, &round);
                    if (!waitForNextStage(&game, !round.active))
                    {
                        cancelRound(&game, &round);
                        goto round_exit;
                    }
                }
            }
            snprintf(feedback, sizeof(feedback), "回合结果：%s；%d轮，消除%d格，得%d分。",
                round.valid > 0 ? "有效交换" : round.valid == 0 ? "无效交换已恢复" : "异常已恢复",
                round.chain, round.totalCleared, round.totalCleared * POINTS_PER_ITEM);
            if (game.score >= 1200) { puts(feedback); break; }
        }
round_exit:
        setGamePaused(&game, 1);
        displayResult(&game);
        if (!running) break;
        puts("R 使用原设置重开；N 修改设置；Q 退出程序。");
        fflush(stdout);
        while ((running = readLine(input, sizeof(input))) != 0)
        {
            if (running < 0) { puts("输入过长，请重新输入。"); continue; }
            if (isCommand(input, 'Q')) { running = 0; break; }
            if (isCommand(input, 'R')) break;
            if (isCommand(input, 'N')) { needsSettings = 1; break; }
            puts("请输入单独的 R、N 或 Q。");
        }
    }
    return 0;
}
