/****************************************************************************************************
 * @file    cli_io.c
 * @brief   CLI入出力機能
 * @details このファイルではCLIライブラリが使用する入出力用モジュールを定義。
 *
 * @author  Masakazu Inoue
 * @date    2026/06/28          新規作成
 ****************************************************************************************************/

/****************************************************************************************************
 * Private include
 ****************************************************************************************************/
#include "../inc/cli_io.h"

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
 * @brief CLI用データ書き込みCB処理の実行
 * @param p 出力データのポインタ
 * @param s 出力データサイズ
 * @return 正常(0) / 失敗(-1) / CB未登録(1)
 */
int cli_io_write(cli_context_t *ctx, const char *p, uint16_t s)
{
    int success = 0;

    if ((ctx == NULL) || (p == NULL))
    {
        success = -1;
    }
    else
    {
        if (ctx->io_write.cb)
        {
            /* 書き込みCB実行 */
            success = ctx->io_write.cb(p, s);
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
        int n = sizeof(ctx->io_write.buf);     /* 最大文字数(終端文字(\0)を含む) */

        va_start(arg, format);
        s = vsnprintf(ctx->io_write.buf, n, format, arg);
        va_end(arg);

        if (0 < s)
        {
            if (n <= s)
            {
                /* 出力用bufサイズをoverしているので切り詰める */
                s = (n - 1);
                ctx->io_write.buf[s] = '\0';
            }

            /* 出力処理実行 */
            success = cli_io_write(ctx, ctx->io_write.buf, ((uint16_t)s));
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
        ctx->io_write.buf[0] = c;
        ctx->io_write.buf[1] = '\0';

        /* 出力処理実行 */
        success = cli_io_write(ctx, ctx->io_write.buf, 1);
    }

    return success;
}