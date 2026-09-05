/****************************************************************************************************
 * @file    cli_output.c
 * @brief   キャラクタ・文字列の出力機能
 * @details このファイルでは、キャラクタ・文字列の出力機能を定義している。
 *          APIを使用するには、writeインタフェースの登録を行う必要がある。
 *
 * @author  Masakazu Inoue
 * @date    2026/06/28          新規作成
 ****************************************************************************************************/

/****************************************************************************************************
 * Private include
 ****************************************************************************************************/
#include "../inc/cli_output.h"

#include "cli_private.h"

#include <stdio.h>
#include <string.h>

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

static int output_write(cli_private_t *priv, const char *p, uint32_t s);

/**
 * @brief キャラクタ出力
 * @param ctx 制御データ(context)のポインタ
 * @param c キャラクタ
 * @return 正常(0) / 失敗(-1)
 */
int cli_putc(cli_context_t *ctx, char c)
{
    if (ctx == NULL) return -1;

    cli_private_t *priv = get_priv(ctx);
    char buf[8] = { 0 };

    buf[0] = c;
    buf[1] = '\0';

    return output_write(priv, (const char *)buf, strlen((const char *)buf));
}

/**
 * @brief 文字列出力
 * @param ctx 制御データ(context)のポインタ
 * @param s 文字列のポインタ
 * @return 正常(0) / 失敗(-1) / CB未登録(1)
 */
int cli_puts(cli_context_t *ctx, const char *s)
{
    if (ctx == NULL) return -1;

    cli_private_t *priv = get_priv(ctx);

    size_t len = strlen(s);

    if (0 < len)
    {
        int ret = 0;

        ret = output_write(priv, s, len);
        if (ret != 0)
        {
            return -2;
        }

        ret = output_write(priv, "\r\n", 3);
        if (ret != 0)
        {
            return -2;
        }
    }

    return 1;
}

/**
 * @brief 文字列出力（内部処理用）
 * @param priv 制御データ(context)のポインタ
 * @param p 文字列のポインタ
 * @return 正常(0) / 失敗(-1) / CB未登録(1)
 */
int output_string(cli_private_t *priv, const char *p)
{
    size_t s = strlen(p);

    if (0 < s)
    {
        return output_write(priv, p, s);
    }

    return -1;
}


/********************
 * Static functions
 ********************/

/**
 * @brief writeインターフェースの実行
 * @param priv 制御データ(context)のポインタ
 * @param p 出力データのポインタ
 * @param s 出力データサイズ
 * @return 正常(0) / 失敗(-1) / CB未登録(1)
 */
static int output_write(cli_private_t *priv, const char *p, uint32_t s)
{
    cli_output_write_t output_write = priv->output_write;

    if (output_write)
    {
        return output_write((const uint8_t *)p, s);
    }

    return 1;
}