/****************************************************************************************************
 * @file    cli_editor.c
 * @brief   Command Line編集
 * @details このファイルではCLIが保持するCommand Lineデータの編集処理に関するモジュールを定義。
 *
 * @author  Masakazu Inoue
 * @date    2026/07/04          新規作成
 ****************************************************************************************************/

/****************************************************************************************************
 * Private include
 ****************************************************************************************************/
#include "cli_editor.h"

#include "../inc/cli_io.h"

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
static void cli_textline_add_char(cli_private_t *priv, char c);
static void cli_textline_delete_char(cli_private_t *priv);
static void cli_textline_delete_text(cli_private_t *priv);

static void cli_textline_cursor_right(cli_private_t *priv);
static void cli_textline_cursor_left(cli_private_t *priv);

static void cli_escape_input(cli_private_t *priv, char c);

static void cli_textline_set_cursor_pos(cli_private_t *priv, uint32_t pos);

static uint32_t cli_textline_get_cursor_pos(cli_private_t *priv);

/**
 * @brief Command Lineの編集
 *        入力された文字データに応じてCommand Line用bufの編集処理を行う。
 * @param ctx CLIの状態データを保持するメモリ領域
 * @param c 入力文字(制御データ含む)
 */
void cli_editor(cli_private_t *priv, char c)
{
    if (priv->escape.sequence)
    {
        /* ESCシーケンスキーの入力 */
        cli_escape_input(priv, c);
    }
    else if (ESC == c)
    {
        /* ESCシーケンスの入力 */
        priv->escape.sequence = true;
        priv->escape.buf[0] = '\0';
        priv->escape.size   = 0;
    }
    else
    {
        if ((SPC <= c) && (c <= 0x7e))
        {
            /* CHARACTER */
            cli_textline_add_char(priv, c);
        }
        else if (BS == c)
        {
            /* BACKSPACE */
            cli_textline_delete_char(priv);
        }
        else
        {
            ;
        }
    }

    if (true != priv->escape.sequence)
    {
        cli_context_t *ctx = get_public(priv);
        size_t p_len = strlen(priv->prompt);

        /* text line draw refresh. */
        cli_printf(ctx, "\033[2K");                                 /* テキスト行を行ごと削除 */
        cli_printf(ctx, "\r");                                      /* 復帰(左寄せ) */
        cli_printf(ctx, "%s", priv->prompt);                        /* プロンプト表示 */
        cli_printf(ctx, "%s", priv->text.line);                     /* テキスト行表示 */
        cli_printf(ctx, "\r");                                      /* 復帰(左寄せ) */
        cli_printf(ctx, "\e[%dC", priv->text.cursor + p_len);       /* 現在のカーソル位置に移動 */
    }
}

/**
 * @brief キャラクタ追加
 *        テキスト行のカーソル位置にキャラクタを追加する
 * @param priv テキストラインデータへのポインタ
 * @param c    キャラクタ
 */
static void cli_textline_add_char(cli_private_t *priv, char c)
{
    uint8_t char_buf[2] = { 0 };

    char_buf[0] = (uint8_t)c;
    char_buf[1] = 0;

    const char *in_text = (const char *)char_buf;

    size_t line_len = strlen((const char *)priv->text.line);
    size_t in_len   = strlen(in_text);
    size_t new_len  = line_len + in_len;

    uint32_t pos = cli_textline_get_cursor_pos(priv);

    if (new_len <= (priv->text.max_size - 1))
    {
        /* 現在のカーソル位置にキャラクタを追加するため、カーソル位置より後ろのデータをずらす */
        for (size_t i = new_len; i >= (pos + in_len) ; i--)
        {
            priv->text.line[i] = priv->text.line[i - in_len];
        }

        /* add character */
        priv->text.line[pos] = in_text[0];

        cli_textline_set_cursor_pos(priv, (pos + 1));
    }
    else
    {
        /* buffer full */
    }
}

/**
 * @brief キャラクタ消去
 *        テキスト行のカーソル位置からキャラクタを消去する
 * @param priv テキストラインデータへのポインタ
 */
static void cli_textline_delete_char(cli_private_t *priv)
{
    size_t line_len = strlen((const char *)priv->text.line);
    size_t new_len  = 0;

    uint32_t pos = cli_textline_get_cursor_pos(priv);

    if ((0 < line_len) && (0 < pos))
    {
        new_len = line_len - 1;

        /* 現在のカーソル位置にキャラクタを追加するため、カーソル位置より後ろのデータをずらす */
        for (size_t i = pos; i <= new_len; i++)
        {
            priv->text.line[i] = priv->text.line[i + 1];
        }

        priv->text.line[new_len] = '\0';

        cli_textline_set_cursor_pos(priv, (pos - 1));
    }
    else
    {
        /* buffer empty */
    }
}

/**
 * @brief テキスト消去
 *        テキスト行のすべてのテキストを消去する
 * @param priv テキストラインデータへのポインタ
 */
static void cli_textline_delete_text(cli_private_t *priv)
{
    size_t text_len = strlen((const char *)priv->text.line);
    char *p = (char *)priv->text.line;

    for (size_t i = 0; i < text_len; i++)
    {
        *(p + i) = '\0';
    }

    cli_textline_set_cursor_pos(priv, 0);
}

/**
 * @brief カーソル右移動
 *        テキスト行上のカーソル位置を右に移動する
 * @param priv テキストラインデータへのポインタ
 */
static void cli_textline_cursor_right(cli_private_t *priv)
{
    uint32_t cp = cli_textline_get_cursor_pos(priv);

    size_t text_len = strlen((const char *)priv->text.line);

    if (priv->text.cursor < text_len)
    {
        cli_textline_set_cursor_pos(priv, (cp + 1));
    }
}

/**
 * @brief カーソル左移動
 *        テキスト行上のカーソル位置を左に移動する
 * @param priv テキストラインデータへのポインタ
 */
static void cli_textline_cursor_left(cli_private_t *priv)
{
    uint32_t cp = cli_textline_get_cursor_pos(priv);

    if (0 < priv->text.cursor)
    {
        cli_textline_set_cursor_pos(priv, (cp - 1));
    }
}

/**
 * @brief Command Line改行
 * @param ctx CLIの状態データを保持するメモリ領域
 */
void cli_editor_new_line(cli_private_t *priv)
{
    cli_textline_delete_text(priv);

    cli_context_t *ctx = get_public(priv);

    cli_printf(ctx, "\r\n");
    cli_printf(ctx, "%s", priv->prompt);
}

/**
 * @brief ESCシーケンス入力処理
 */
static void cli_escape_input(cli_private_t *priv, char c)
{
    /* '\e'以降の構文作成 */
    priv->escape.buf[priv->escape.size    ] = c;
    priv->escape.buf[priv->escape.size + 1] = '\0';
    priv->escape.size++;

    /* ESCシーケンスの種別ごとに分岐 */
    if (('[' == priv->escape.buf[0]) && (2 <= priv->escape.size))
    {
        switch (priv->escape.buf[1])
        {
        case 'C':
            /* カーソル右移動 */
            cli_textline_cursor_right(priv);
            break;

        case 'D':
            /* カーソル左移動 */
            cli_textline_cursor_left(priv);
            break;

        default:
            /* DO NOTHING */
            break;
        }

        /* ESCの分岐結果に関わらずフラグはoffにする */
        priv->escape.sequence = false;
    }
    else if ('[' != priv->escape.buf[0])
    {
        /* ESC入力としての解釈不能なためフラグはoffにする */
        priv->escape.sequence = false;
    }
    else
    {
        /* DO NOTHING */
    }
}

/********************
 * Setter functions
 ********************/

static void cli_textline_set_cursor_pos(cli_private_t *priv, uint32_t pos)
{
    priv->text.cursor = pos;
}

/********************
 * Getter functions
 ********************/

static uint32_t cli_textline_get_cursor_pos(cli_private_t *priv)
{
    return priv->text.cursor;
}