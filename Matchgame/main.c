#define  _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "game.h"

/*
 * 功能：读取玩家设置并运行游戏主循环
 * 参数：无
 * 返回值：0表示正常结束，1表示初始输入无效
 */
int main(void)
{
    Game game; // 当前游戏状态

    char name[PLAYER_NAME_LENGTH]; // 玩家昵称
    int k = 0; // 连续消除长度

    char input[80]; // 玩家输入的一整行内容
    char command; // 用于判断退出命令
    char direction; // 玩家交换方向
    char extra; // 用于检测输入末尾是否有多余字符

    int row; // 玩家输入的行号
    int col; // 玩家输入的列号
    int result; // 本次操作返回值
    int ch; // 用于清理输入缓冲区
    int levelResult;  // 本回合的关卡更新结果
    int hasWon = 0;   // 是否已经取得最终胜利

    // 初始化随机数种子
    srand((unsigned int)time(NULL));

    // 获取玩家昵称
    printf("Enter your nickname: ");
    if (scanf("%31s", name) != 1)
    {
        return 1;
    }

    // 清除读取昵称后残留的当前行内容
    while ((ch = getchar()) != '\n' && ch != EOF)
    {
    }

    // 循环读取消除长度，直到玩家输入合法值
    while (1)
    {
        printf("Choose match length (3/4/5): ");

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            return 1;
        }

        // 输入行过长时，清除本行剩余内容
        if (strchr(input, '\n') == NULL)
        {
            while ((ch = getchar()) != '\n' && ch != EOF)
            {
            }

            printf("Input is too long!\n");
            continue;
        }

        // 输入必须仅含一个整数，并且整数只能是3、4或5
        if (sscanf(input, " %d %c", &k, &extra) == 1 &&
            k >= 3 && k <= 5)
        {
            break;
        }

        printf("Invalid match length! Please enter 3, 4, or 5.\n");
    }

    // 初始化游戏
    initializeGame(&game, name, k);

    // 游戏主循环
    while (1)
    {
        // 显示当前棋盘与分数
        displayGame(&game);

        // 等待玩家操作
        printf("\nEnter move (row col W/A/S/D), or Q to quit: ");

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            break;
        }

        // 检查玩家是否选择退出
        if (sscanf(input, " %c", &command) == 1 &&
            (command == 'Q' || command == 'q'))
        {
            break;
        }

        // 解析玩家操作
        if (sscanf(input, "%d %d %c",
            &row, &col, &direction) != 3)
        {
            printf("Input format error!\n");
            continue;
        }

        // 执行交换与消除
        result = playMove(&game, row, col, direction);

        // 显示操作结果
        if (result == 1)
        {
            printf("Move successful!\n");
        }
        else if (result == 0)
        {
            printf("No match! Move cancelled.\n");
        }
        else
        {
            printf("Invalid move!\n");
        }

        // 只有有效交换才可能获得分数
        if (result == 1)
        {
            // 检查分数是否达到关卡目标
            levelResult = updateLevel(&game);

            if (levelResult == 1)
            {
                printf("\nCongratulations!\n");
                printf("Welcome to Level %d!\n",
                    game.level);
            }
            else if (levelResult == 2)
            {
                hasWon = 1;

                printf("\n====================\n");
                printf("   YOU WIN!\n");
                printf("====================\n");

                break;
            }
        }

    }

    // 游戏结束后显示最终结果
    printf("\n===== Game Finished =====\n");

    if (hasWon)
    {
        printf("Result: Victory!\n");
    }
    else
    {
        printf("Result: Player Quit.\n");
    }

    printf("Player: %s\n", game.playerName);
    printf("Final Level: %d\n", game.level);
    printf("Final Score: %d\n", game.score);
    printf("Total Time: %ld seconds\n",getElapsedSeconds(&game));


    return 0;
}
