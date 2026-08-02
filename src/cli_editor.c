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

    if (priv->cmd_line.escape.sequence)
    {
        /* ESCシーケンスキーの入力 */
        cli_escape_input(priv, c);
    }
    else if (ESC == c)
    {
        /* ESCシーケンスの入力 */
        priv->cmd_line.escape.sequence = true;
        priv->cmd_line.escape.buf[0] = '\0';
        priv->cmd_line.escape.size   = 0;
    }
    else
    {
        /* キャラクタの入力 */
        if (priv->cmd_line.current.size <= priv->cmd_line.cursor.y)
        {
            /* 入力データサイズがカーソル位置より小さい */
            if ((SPC <= c) && (c <= 0x7e))
            {
                /* 図形文字(空白含む) */
                if (priv->cmd_line.current.size < (CLI_LINE_SIZE - 1))
                {
                    priv->cmd_line.current.buf[priv->cmd_line.current.size    ] = c;
                    priv->cmd_line.current.buf[priv->cmd_line.current.size + 1] = '\0';         /* バッファ終端にNULL文字を挿入 */
                    priv->cmd_line.current.size++;

                    priv->cmd_line.cursor.y++;
                }

                cli_printf(ctx, "%c", c);        /* バッファフローしていてもエコーバックは行う */
            }
            else if (BS == c)
            {
                /* バックスペース */
                if (0 < priv->cmd_line.current.size)
                {
                    priv->cmd_line.current.size--;
                    priv->cmd_line.current.buf[priv->cmd_line.current.size] = '\0';

                    priv->cmd_line.cursor.y--;

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
                memccpy(&temp[0], &priv->cmd_line.current.buf[priv->cmd_line.cursor.y], '\0', 128);

                /* 現在のカーソル位置に文字を挿入 */
                priv->cmd_line.current.buf[priv->cmd_line.cursor.y    ] = c;
                priv->cmd_line.current.buf[priv->cmd_line.cursor.y + 1] = '\0';
                priv->cmd_line.cursor.y++;

                /* 一時bufに退避した文字列を最新のカーソル位置に挿入 */
                memccpy(&priv->cmd_line.current.buf[priv->cmd_line.cursor.y], &temp[0], '\0', 128);

                /* データサイズ更新 */
                priv->cmd_line.current.size++;

                /* プロンプトの文字列サイズを取得 */
                size = (uint8_t)strlen(priv->prompt);

                /* カーソルのある行を1行ごと再描画 */
                cli_printf(ctx, "\033[2K");                                 /* カーソルが存在する１行を消去 */
                cli_printf(ctx, "\r");                                      /* 復帰(左寄せ) */
                cli_printf(ctx, ">%s", &priv->cmd_line.current.buf[0]);      /* 1行ごと再描画 */
                cli_printf(ctx, "\r");                                      /* 復帰(左寄せ) */
                cli_printf(ctx, "\e[%dC", priv->cmd_line.cursor.y + size);   /* 現在のカーソル位置に移動 */
            }
            else if (BS == c)
            {
                /* バックスペース */
                if ((0 < priv->cmd_line.current.size) && (0 < priv->cmd_line.cursor.y))
                {
                    /* 現在のカーソル位置から後ろの文字列を一時bufに退避 */
                    memccpy(&temp[0], &priv->cmd_line.current.buf[priv->cmd_line.cursor.y], '\0', 128);

                    /* 現在のカーソル位置に存在する文字を削除 */
                    priv->cmd_line.cursor.y--;
                    priv->cmd_line.current.buf[priv->cmd_line.cursor.y] = '\0';

                    /* 一時bufに退避した文字列を最新のカーソル位置に挿入 */
                    memccpy(&priv->cmd_line.current.buf[priv->cmd_line.cursor.y], &temp[0], '\0', 128);

                    /* データサイズ更新 */
                    priv->cmd_line.current.size--;

                    /* プロンプトの文字列サイズを取得 */
                    size = (uint8_t)strlen(priv->prompt);

                    /* カーソルのある行を1行ごと再描画 */
                    cli_printf(ctx, "\033[2K");                                 /* カーソルが存在する１行を消去 */
                    cli_printf(ctx, "\r");                                      /* 復帰(左寄せ) */
                    cli_printf(ctx, ">%s", &priv->cmd_line.current.buf[0]);      /* 1行ごと再描画 */
                    cli_printf(ctx, "\r");                                      /* 復帰(左寄せ) */
                    cli_printf(ctx, "\e[%dC", priv->cmd_line.cursor.y + size);   /* 現在のカーソル位置に移動 */
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
    priv->cmd_line.current.buf[0] = '\0';
    priv->cmd_line.current.size   = 0;

    /* カーソル位置(上下左右)の初期化 */
    priv->cmd_line.cursor.y = 0;
    priv->cmd_line.cursor.x = 0;

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
    priv->cmd_line.escape.buf[priv->cmd_line.escape.size    ] = c;
    priv->cmd_line.escape.buf[priv->cmd_line.escape.size + 1] = '\0';
    priv->cmd_line.escape.size++;

    /* ESCシーケンスの種別ごとに分岐 */
    if (('[' == priv->cmd_line.escape.buf[0]) && (2 <= priv->cmd_line.escape.size))
    {
        switch (priv->cmd_line.escape.buf[1])
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
        priv->cmd_line.escape.sequence = false;
    }
    else if ('[' != priv->cmd_line.escape.buf[0])
    {
        /* ESC入力としての解釈不能なためフラグはoffにする */
        priv->cmd_line.escape.sequence = false;
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

    if (priv->cmd_line.cursor.y < priv->cmd_line.current.size)
    {
        /* カーソル位置更新(右に移動するよう描画) */
        cli_printf(ctx, "\e[C");

        /* カーソル位置更新 */
        priv->cmd_line.cursor.y++;
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

    if (0 < priv->cmd_line.cursor.y)
    {
        /* カーソル位置更新(左に移動するよう描画) */
        cli_printf(ctx, "\e[D");

        /* カーソル位置更新 */
        priv->cmd_line.cursor.y--;
    }
    else
    {
        /* DO NOTHING */
    }
}