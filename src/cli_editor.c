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

static void cli_escape_input(cli_private_t *priv, char c);

static void cli_cursor_move_right(cli_private_t *priv);
static void cli_cursor_move_left(cli_private_t *priv);

/**
 * @brief Command Lineの編集
 *        入力された文字データに応じてCommand Line用bufの編集処理を行う。
 * @param ctx CLIの状態データを保持するメモリ領域
 * @param c 入力文字(制御データ含む)
 */
void cli_editor(cli_private_t *priv, char c)
{
    cli_context_t *ctx = get_public(priv);

    char temp[128] = {'\0'};
    uint8_t size = 0;

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
        /* キャラクタの入力 */
        if (priv->text.size <= priv->cursor)
        {
            /* 入力データサイズがカーソル位置より小さい */
            if ((SPC <= c) && (c <= 0x7e))
            {
                /* 図形文字(空白含む) */
                if (priv->text.size < (priv->text.max_size - 1))
                {
                    priv->text.line[priv->text.size    ] = c;
                    priv->text.line[priv->text.size + 1] = '\0';         /* バッファ終端にNULL文字を挿入 */
                    priv->text.size++;

                    priv->cursor++;
                }

                cli_printf(ctx, "%c", c);        /* バッファフローしていてもエコーバックは行う */
            }
            else if (BS == c)
            {
                /* バックスペース */
                if (0 < priv->text.size)
                {
                    priv->text.size--;
                    priv->text.line[priv->text.size] = '\0';

                    priv->cursor--;

                    cli_printf(ctx, "%c", BS );
                    cli_printf(ctx, "%c", SPC);
                    cli_printf(ctx, "%c", BS );
                }
            }
            else
            {
                /* 図形文字以外は受付しない */
            }
        }
        else
        {
            /* 入力データサイズがカーソル位置より大きい */
            if ((SPC <= c) && (c <= 0x7e))
            {
                /* 現在のカーソル位置から後ろの文字列を一時bufに退避 */
                memccpy(&temp[0], &priv->text.line[priv->cursor], '\0', priv->text.size);

                /* 現在のカーソル位置に文字を挿入 */
                priv->text.line[priv->cursor    ] = c;
                priv->text.line[priv->cursor + 1] = '\0';
                priv->cursor++;

                /* 一時bufに退避した文字列を最新のカーソル位置に挿入 */
                memccpy(&priv->text.line[priv->cursor], &temp[0], '\0', priv->text.size);

                /* データサイズ更新 */
                priv->text.size++;

                /* プロンプトの文字列サイズを取得 */
                size = (uint8_t)strlen(priv->prompt);

                /* カーソルのある行を1行ごと再描画 */
                cli_printf(ctx, "\033[2K");                                 /* カーソルが存在する１行を消去 */
                cli_printf(ctx, "\r");                                      /* 復帰(左寄せ) */
                cli_printf(ctx, ">%s", &priv->text.line[0]);      /* 1行ごと再描画 */
                cli_printf(ctx, "\r");                                      /* 復帰(左寄せ) */
                cli_printf(ctx, "\e[%dC", priv->cursor + size);   /* 現在のカーソル位置に移動 */
            }
            else if (BS == c)
            {
                /* バックスペース */
                if ((0 < priv->text.size) && (0 < priv->cursor))
                {
                    /* 現在のカーソル位置から後ろの文字列を一時bufに退避 */
                    memccpy(&temp[0], &priv->text.line[priv->cursor], '\0', priv->text.size);

                    /* 現在のカーソル位置に存在する文字を削除 */
                    priv->cursor--;
                    priv->text.line[priv->cursor] = '\0';

                    /* 一時bufに退避した文字列を最新のカーソル位置に挿入 */
                    memccpy(&priv->text.line[priv->cursor], &temp[0], '\0', priv->text.size);

                    /* データサイズ更新 */
                    priv->text.size--;

                    /* プロンプトの文字列サイズを取得 */
                    size = (uint8_t)strlen(priv->prompt);

                    /* カーソルのある行を1行ごと再描画 */
                    cli_printf(ctx, "\033[2K");                                 /* カーソルが存在する１行を消去 */
                    cli_printf(ctx, "\r");                                      /* 復帰(左寄せ) */
                    cli_printf(ctx, ">%s", &priv->text.line[0]);      /* 1行ごと再描画 */
                    cli_printf(ctx, "\r");                                      /* 復帰(左寄せ) */
                    cli_printf(ctx, "\e[%dC", priv->cursor + size);   /* 現在のカーソル位置に移動 */
                }
            }
            else
            {
                /* 図形文字以外は受付しない */
            }
        }
    }
}


/**
 * @brief Command Line改行
 * @param ctx CLIの状態データを保持するメモリ領域
 */
void cli_editor_new_line(cli_private_t *priv)
{
    /* 現在行用buf・サイズの初期化 */
    priv->text.line[0] = '\0';
    priv->text.size   = 0;

    /* カーソル位置初期化 */
    priv->cursor = 0;

    cli_context_t *ctx = get_public(priv);

    /* 改行と改行後のプロンプトを描画 */
    cli_printf(ctx, "\r\n%s", priv->prompt);
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
            cli_cursor_move_right(priv);
            break;

        case 'D':
            /* カーソル左移動 */
            cli_cursor_move_left(priv);
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

/**
 * @brief カーソルの右移動
 */
static void cli_cursor_move_right(cli_private_t *priv)
{
    cli_context_t *ctx = get_public(priv);

    if (priv->cursor < priv->text.size)
    {
        /* カーソル位置更新(右に移動するよう描画) */
        cli_printf(ctx, "\e[C");

        /* カーソル位置更新 */
        priv->cursor++;
    }
    else
    {
        /* DO NOTHING */
    }
}

/**
 * @brief カーソルの左移動
 */
static void cli_cursor_move_left(cli_private_t *priv)
{
    cli_context_t *ctx = get_public(priv);

    if (0 < priv->cursor)
    {
        /* カーソル位置更新(左に移動するよう描画) */
        cli_printf(ctx, "\e[D");

        /* カーソル位置更新 */
        priv->cursor--;
    }
    else
    {
        /* DO NOTHING */
    }
}