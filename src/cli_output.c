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

static cli_impl_code_t output_write( cli_private_t * priv, const char * p, uint32_t s );

/**
 * @brief キャラクタ出力
 * @param ctx 制御データ(context)のポインタ
 * @param c キャラクタ
 * @return cli_code_t
 */
cli_code_t cli_putc( cli_context_t * ctx, char c )
{
    if (ctx == NULL) return CLI_ERROR_INVALID_INSTANCE;

    cli_private_t *priv = get_priv(ctx);

    if ( output_write( priv, (const char *)&c, 1 ) != CLI_IMPL_OK )
    {
        return CLI_ERROR_OUTPUT_WRITE_FAILED;
    }

    return CLI_OK;
}

/**
 * @brief 文字列出力
 * @param ctx 制御データ(context)のポインタ
 * @param p 文字列のポインタ
 * @return cli_code_t
 */
cli_code_t cli_puts( cli_context_t * ctx, const char * p )
{
    if (ctx == NULL) return CLI_ERROR_INVALID_INSTANCE;

    cli_private_t *priv = get_priv(ctx);

    if ( '\0' == *p )
    {
        if ( output_write( priv, p, strlen(p) ) != CLI_IMPL_OK )
        {
            return CLI_ERROR_OUTPUT_WRITE_FAILED;
        }
    }

    if ( output_write(priv, "\r\n", 2) != CLI_IMPL_OK )
    {
        return CLI_ERROR_OUTPUT_WRITE_FAILED;
    }

    return CLI_OK;
}

/**
 * @brief 文字列出力（内部処理用）
 * @param priv 制御データ(context)のポインタ
 * @param p 文字列のポインタ
 */
void output_string( cli_private_t * priv, const char * p )
{
    if ( output_write( priv, p, strlen(p) ) != CLI_IMPL_OK )
    {
        /* 内部処理用のためエラーハンドリングはしない         */
        /* 今後、CLI用のprintf()が実装される予定              */
        /* その際に、本関数からprintf()に置換される可能性あり */
    }
}


/********************
 * Static functions
 ********************/

/**
 * @brief writeインターフェースの実行
 * @param priv 制御データ(context)のポインタ
 * @param p 出力データのポインタ
 * @param s 出力データサイズ
 * @return cli_impl_code_t
 */
static cli_impl_code_t output_write( cli_private_t * priv, const char * p, uint32_t s )
{
    cli_output_t * output = (cli_output_t *)priv->output;

    if ( output->write )
    {
        if ( output->write( (const uint8_t *)p, s ) == CLI_OUTPUT_ERROR )
        {
            return CLI_IMPL_ERROR_OUTPUT_WRITE_FAILED;
        }
    }
    else
    {
        return CLI_IMPL_ERROR_OUTPUT_NOT_REGISTERED;
    }

    return CLI_IMPL_OK;
}