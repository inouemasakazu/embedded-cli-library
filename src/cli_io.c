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

#include "cli_private.h"

#include <stdio.h>

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
 * @param ctx 制御データ(context)のポインタ
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
        cli_private_t *priv = get_priv(ctx);

        if (priv->io_write_cb)
        {
            /* 書き込みCB実行 */
            success = priv->io_write_cb(p, s);
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
 * @brief 書式付き文字列の出力
 *        標準ライブラリのprintfと同様の機能を有する。
 * @param ctx 制御データ(context)のポインタ
 * @param format 出力するときの書式を含む文字列
 * @param ...    出力する値のリスト
 * @return 出力した文字数、または負の値(エラー時)
 */
int cli_printf(cli_context_t *ctx, const char * format, ...)
{
    if (ctx == NULL) return -1;

    int success = -1;
    va_list arg;

    va_start(arg, format);
    success = cli_vprintf(ctx, format, arg);
    va_end(arg);

    return success;
}

/**
 * @brief 可変個引数リストを書式付で文字列に出力
 * @param ctx 制御データ(context)のポインタ
 * @param format 出力するときの書式を含む文字列
 * @param arg    引数並びへのポインタ
 * @return 出力した文字数、または負の値(エラー時)
 */
int cli_vprintf(cli_context_t *ctx, const char *format, va_list arg)
{
    if (ctx == NULL) return -1;

    cli_private_t *priv = get_priv(ctx);

    int success = -1;
    int s = 0;
    int n = priv->output.max_size;      /* 最大文字数(終端文字(\0)を含む) */

    s = vsnprintf((char *)priv->output.buf, n, format, arg);

    if (0 < s)
    {
        if (n <= s)
        {
            /* bufサイズを超過しているので切り詰める */
            s = (n - 1);
            priv->output.buf[s] = '\0';
        }

        /* 出力処理実行 */
        success = cli_io_write(ctx, (const char *)priv->output.buf, ((uint16_t)s));
        if (success == 0)
        {
            /* 処理結果として出力した文字数(byte)を返す */
            success = s;
        }
    }
    else
    {
        /* 表現形式エラー */
        success = -1;
    }

    return success;
}

/**
 * @brief CLI用文字の標準出力
 *        1byte単位でデータを出力する
 * @param ctx 制御データ(context)のポインタ
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
        cli_private_t *priv = get_priv(ctx);

        /* データ整形 */
        priv->output.buf[0] = c;
        priv->output.buf[1] = '\0';

        /* 出力処理実行 */
        success = cli_io_write(ctx, (const char *)priv->output.buf, 1);
    }

    return success;
}