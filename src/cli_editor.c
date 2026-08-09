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

static void cli_textline_add_history(cli_private_t *priv, const char *text);
static const char *cli_textline_history_pull(cli_private_t *priv, uint32_t point);

static void cli_textline_history_prev(cli_private_t *priv);
static void cli_textline_history_next(cli_private_t *priv);



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
    uint16_t event = 0;

    switch (priv->status)
    {
    case 0:
        if (ESC == c)
        {
            priv->status = 1;
        }
        else if ((SPC <= c) && (c <= 0x7e))
        {
            event = 1;
        }
        else if (BS == c)
        {
            event = 2;
        }
        else
        {
            ;
        }
        break;

    case 1:
        if ('[' == c)
        {
            priv->status = 2;
        }
        else
        {
            priv->status = 0;
        }
        break;

    case 2:
        if ('A' == c)
        {
            event = 5;
        }
        else if ('B' == c)
        {
            event = 6;
        }
        else if ('C' == c)
        {
            event = 3;
        }
        else if ('D' == c)
        {
            event = 4;
        }
        else
        {
            event = 0;
        }

        priv->status = 0;
        break;
    default:
        break;
    }

    if (event != 0)
    {
        if (event == 1)
        {
            /* CHARACTER */
            cli_textline_add_char(priv, c);
        }
        else if (event == 2)
        {
            /* BACKSPACE */
            cli_textline_delete_char(priv);
        }
        else if (event == 3)
        {
            /* カーソル右移動 */
            cli_textline_cursor_right(priv);
        }
        else if (event == 4)
        {
            /* カーソル左移動 */
            cli_textline_cursor_left(priv);
        }
        else if (event == 5)
        {
            cli_textline_history_prev(priv);
        }
        else if (event == 6)
        {
            cli_textline_history_next(priv);
        }
    
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
 * @brief 履歴追加
 *        引数のtextを履歴として履歴用の循環バッファに保存する。
 * @param priv テキストラインデータへのポインタ
 * @param text 保存するテキストデータのポインタ
 */
static void cli_textline_add_history(cli_private_t *priv, const char *text)
{
    size_t text_len = strlen(text);

    if (0 < text_len)
    {
        char *p = (char *)priv->history.buffer;
        uint32_t max = sizeof(priv->history.max_size / priv->text.max_size);

        uint32_t next = (priv->history.head + 1) % max;
        uint32_t offset = 0;

        offset = priv->history.max_size / max;
        offset = offset * priv->history.head;

        /* 現在のテキストデータ(終端文字含む)を履歴用バッファにコピー */
        for (size_t i = 0; i < (text_len + 1); i++)
        {
            *(p + (offset + i)) = *(text + i);
        }

        priv->history.head = next;
        priv->history.tail = 0;

        if (max > priv->history.count)
        {
            priv->history.count++;
        }
    }
    else
    {
        /* There is no text. */
    }
}

/**
 * @brief 履歴取得
 *        履歴用の循環バッファから、指定ポイントのデータを取得する
 *        循環バッファではあるが読み出し位置の更新は行わない
 * @param priv テキストラインデータへのポインタ
 * @param point 循環バッファの指定ポイント
 */
static const char *cli_textline_history_pull(cli_private_t *priv, uint32_t point)
{
    const char *text;
    uint32_t max = sizeof(priv->history.max_size / priv->text.max_size);

    if (max > point)
    {
        uint32_t offset = 0;

        offset = priv->history.max_size / max;
        offset = offset * point;

        text = (const char *)(priv->history.buffer + offset);
    }
    else
    {
        text = NULL;
    }

    return text;
}

/**
 * @brief 履歴呼び出し(古い履歴を遡る)
 *        履歴としてストレージしているデータを、新しいデータから、古いデータの順に呼び出す。
 *        呼び出したデータは、テキスト行にコピーする。
 * @param priv テキストラインデータへのポインタ
 */
static void cli_textline_history_prev(cli_private_t *priv)
{
    /* 履歴にデータあり？ */
    if ((0 < priv->history.count) && (priv->history.tail < priv->history.count))
    {
        uint32_t point = 0;
        uint32_t max = sizeof(priv->history.max_size / priv->text.max_size);

        point = (priv->history.head + (max - 1)) % max;
        point = (point + (max - priv->history.tail)) % max;

        /* 現在のテキスト行は消去 */
        cli_textline_delete_text(priv);

        const char *history = cli_textline_history_pull(priv, point);
        char *p = (char *)priv->text.line;

        size_t len = strlen(history);

        for (size_t i = 0; i < len; i++)
        {
            *(p + i) = *(history + i);
        }

        cli_textline_set_cursor_pos(priv, len);
        priv->history.tail++;
    }
    else
    {
        /* 履歴がない、または履歴の上限に到達 */
    }
}

/**
 * @brief 履歴呼び出し(新しい履歴に進む)
 *        履歴としてストレージしているデータを、古いデータから、新しいデータの順に呼び出す。
 *        呼び出したデータは、テキスト行にコピーする。
 * @param priv テキストラインデータへのポインタ
 */
static void cli_textline_history_next(cli_private_t *priv)
{
    /* 履歴にデータあり？ */
    if ((0 < priv->history.count) && (1 < priv->history.tail))
    {
        uint32_t point = 0;
        uint32_t max = sizeof(priv->history.max_size / priv->text.max_size);

        point = (priv->history.head + (max + 1)) % max;
        point = (point + (max - priv->history.tail)) % max;

        /* 現在のテキスト行は消去 */
        cli_textline_delete_text(priv);

        const char *history = cli_textline_history_pull(priv, point);
        char *p = (char *)priv->text.line;

        size_t len = strlen(history);

        for (size_t i = 0; i < len; i++)
        {
            *(p + i) = *(history + i);
        }

        cli_textline_set_cursor_pos(priv, len);
        priv->history.tail--;
    }
    else if ((0 < priv->history.count) && (0 < priv->history.tail))
    {
    	cli_textline_delete_text(priv);
    	priv->history.tail--;
    }
    else
    {
        /* 履歴がない、または履歴の下限に到達 */
    }
}

/**
 * @brief 改行処理
 * @param priv テキストラインデータへのポインタ
 */
void cli_textline_break(cli_private_t *priv)
{
    size_t text_len = strlen((const char *)priv->text.line);

    if (0 < text_len)
    {
        /* テキストを履歴として保存 */
        cli_textline_add_history(priv, (const char *)priv->text.line);

        /* テキストデータをすべて消去 */
        cli_textline_delete_text(priv);
    }

    cli_context_t *ctx = get_public(priv);

    cli_printf(ctx, "\r\n");
    cli_printf(ctx, "%s", priv->prompt);
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