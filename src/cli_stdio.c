/****************************************************************************************************
 * @file    cli_stdio.c
 * @brief   CLI標準入出力
 * @details このファイルではCLIにて使用する標準入出力用モジュールを定義。
 *
 * @author  Masakazu Inoue
 * @date    2026/06/28          新規作成
 ****************************************************************************************************/

/****************************************************************************************************
 * Private include
 ****************************************************************************************************/
#include "../inc/cli_stdio.h"

#include <stdio.h>
#include <stdarg.h>

/****************************************************************************************************
 * Private define
 ****************************************************************************************************/

/****************************************************************************************************
 * Private typedef
 ****************************************************************************************************/

/****************************************************************************************************
 * Private Variables
 ****************************************************************************************************/

/****************************************************************************************************
 * Private Functions
 ****************************************************************************************************/

/**
 * @brief CLI用標準出力実行
 *        CLIに使用するコールバックの呼び出しを行う。
 * @param p 出力データのポインタ
 * @param s 出力データサイズ
 * @return 正常(0) / 失敗(-1) / CB未登録(1)
 */
int cli_stdout(cli_context_t *ctx, const char *p, uint16_t s)
{
    int success = 0;

    if ((ctx == NULL) || (p == NULL))
    {
        success = -1;
    }
    else
    {
        if (ctx->stdout_cb)
        {
            /* CLI用標準出力実行 */
            success = ctx->stdout_cb(p, s);
        }
        else
        {
            /* CB未登録 */
            success = 1;
        }
    }

    return success;
}

/**
 * @brief CLI用書式付き文字列の標準出力
 *        printfと同様のフォーマットで書式化文字列の出力を行う
 * @param  format 出力するときの書式を含む文字列
 * @param  ...    出力する値のリスト
 * @return 出力byte数 / 失敗(-1)
 */
int cli_printf(cli_context_t *ctx, const char * format, ...)
{
    int success = -1;
    va_list arg;

    if (ctx == NULL)
    {
        success = -1;
    }
    else
    {
        int s = 0;
        int n = sizeof(ctx->write_buf);     /* 最大文字数(終端文字(\0)を含む) */

        va_start(arg, format);
        s = vsnprintf(ctx->write_buf, n, format, arg);
        va_end(arg);

        if (0 < s)
        {
            if (n <= s)
            {
                /* 出力用bufサイズをoverしているので切り詰める */
                s = (n - 1);
                ctx->write_buf[s] = '\0';
            }

            /* 出力処理実行 */
            success = cli_stdout(ctx, ctx->write_buf, ((uint16_t)s));
            if (success == 0)
            {
                /* 処理結果として出力byte数を返す */
                success = s;
            }
        }
        else
        {
            /* 表現形式エラー */
            success = -1;
        }
    }

    return success;
}

/**
 * @brief CLI用文字の標準出力
 *        1byte単位でデータを出力する
 * @param c 出力する文字
 * @return 正常(0) / 失敗(-1)
 */
int cli_putc(cli_context_t *ctx, char c)
{
    int success = 0;

    if (ctx == NULL)
    {
        success = -1;
    }
    else
    {
        /* データ整形 */
        ctx->write_buf[0] = c;
        ctx->write_buf[1] = '\0';

        /* 出力処理実行 */
        success = cli_stdout(ctx, ctx->write_buf, 1);
    }

    return success;
}