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

static void cli_textline_history_push(cli_private_t *priv, const char *text);
static const char *cli_textline_history_pull(cli_private_t *priv, uint32_t index);


/**
 * @brief キャラクタ追加
 *        テキスト行のカーソル位置にキャラクタを追加する
 * @param priv テキストラインデータへのポインタ
 * @param c    キャラクタ
 */
void cli_textline_add_char(cli_private_t *priv, char c)
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
void cli_textline_delete_char(cli_private_t *priv)
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
void cli_textline_delete_text(cli_private_t *priv)
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
 * @brief テキスト保存
 *        引数textを履歴として保存する。
 * @param priv テキストラインデータへのポインタ
 * @param text テキストのポインタ
 */
void cli_textline_storage_text(cli_private_t *priv, const char *text)
{
    size_t text_len = strlen(text);

    if (0 < text_len)
    {
        uint32_t msx_line_size = priv->text.max_size;
        uint32_t max_history_count = priv->history.max_size / msx_line_size;

        if (0 < priv->history.count)
        {
            uint32_t prev_idex = 0;
            const char *prev_history = NULL;

            prev_idex = (priv->history.write_idx + (max_history_count - 1)) % max_history_count;
            prev_history = cli_textline_history_pull(priv, prev_idex);

            /* 既に履歴データが存在するため、前回データと今回データの比較を行う。 */
            if (strcmp(prev_history, text) != 0)
            {
                /* 今回データを履歴データとして設定 */
                cli_textline_set_history(priv, text);
            }
            else
            {
                /* 前回データと同一である場合は保存しない */
            }
        }
        else
        {
            /* 初回の履歴データを設定 */
            cli_textline_set_history(priv, text);
        }

        priv->history.browse_idx = HISTORY_BROWSE_UNREAD;
    }
}

/**
 * @brief カーソル右移動
 *        テキスト行上のカーソル位置を右に移動する
 * @param priv テキストラインデータへのポインタ
 */
void cli_textline_cursor_right(cli_private_t *priv)
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
void cli_textline_cursor_left(cli_private_t *priv)
{
    uint32_t cp = cli_textline_get_cursor_pos(priv);

    if (0 < priv->text.cursor)
    {
        cli_textline_set_cursor_pos(priv, (cp - 1));
    }
}

/**
 * @brief 履歴データの書き込み
 *        履歴用の循環バッファに引数textの書き込みを行う。
 * @param priv テキストラインデータへのポインタ
 * @param text 書き込みデータのポインタ
 */
static void cli_textline_history_push(cli_private_t *priv, const char *text)
{
    size_t text_len = strlen(text);

    if (0 < text_len)
    {
        uint32_t msx_line_size = priv->text.max_size;
        uint32_t max_history_count = priv->history.max_size / msx_line_size;

        uint32_t offset = msx_line_size * priv->history.write_idx;

        char *p = (char *)priv->history.buffer + offset;

        /* 現在のテキストデータ(終端文字含む)を履歴用バッファにコピー */
        for (size_t i = 0; i < (text_len + 1); i++)
        {
            *(p + i) = *(text + i);
        }

        priv->history.write_idx = (priv->history.write_idx + 1) % max_history_count;
    }
    else
    {
        /* There is no text. */
    }
}

/**
 * @brief 履歴データの読み込み
 *        履歴用の循環バッファから、指定ポイントのデータを取得する
 *        循環バッファではあるが読み出し位置の更新は行わない
 * @param priv テキストラインデータへのポインタ
 * @param point 循環バッファの指定ポイント
 */
static const char *cli_textline_history_pull(cli_private_t *priv, uint32_t index)
{
    const char *text;

    uint32_t msx_line_size = priv->text.max_size;
    uint32_t max_history_count = priv->history.max_size / msx_line_size;

    if (max_history_count > index)
    {
        uint32_t offset = msx_line_size * index;

        text = (const char *)(priv->history.buffer + offset);
    }
    else
    {
        text = NULL;
    }

    return text;
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
        /* テキストデータをすべて消去 */
        cli_textline_delete_text(priv);
    }
}


/********************
 * Setter functions
 ********************/

/**
 * @brief 履歴設定
 *        引数textを履歴データとして設定する。
 * @param priv テキストラインデータへのポインタ
 * @param text テキストのポインタ
 */
void cli_textline_set_history(cli_private_t *priv, const char *text)
{
    size_t text_len = strlen(text);

    if (0 < text_len)
    {
        uint32_t max_history_count = priv->history.max_size / priv->text.max_size;

        cli_textline_history_push(priv, text);

        if (max_history_count > priv->history.count)
        {
            /* 履歴データの件数を更新 */
            priv->history.count++;
        }
    }
}

void cli_textline_set_cursor_pos(cli_private_t *priv, uint32_t pos)
{
    priv->text.cursor = pos;
}

/********************
 * Getter functions
 ********************/

/**
 * @brief 履歴取得(古い履歴に遡る)
 *        履歴としてストレージしているテキストデータを、新しいデータから、古いデータの順に呼び出す。
 * @param priv テキストラインデータへのポインタ
 * @return     テキストのポインタ
 */
const char *cli_textline_get_history_prev(cli_private_t *priv)
{
    if (0 == priv->history.count) return NULL;

    uint32_t max_history_count = priv->history.max_size / priv->text.max_size;

    if (HISTORY_BROWSE_UNREAD == priv->history.browse_idx)
    {
        /* 履歴の閲覧を開始 */
        /* 現在の書き込み位置から閲覧位置(index)の開始位置を算出 */
        priv->history.browse_idx = (priv->history.write_idx + (max_history_count - 1)) % max_history_count;
    }
    else
    {
        uint32_t oldest_idx = (priv->history.write_idx + (max_history_count - priv->history.count)) % max_history_count;
        if ((uint32_t)priv->history.browse_idx == oldest_idx)
        {
            /* 既に一番古い履歴に達しているため、閲覧位置(index)の更新はしない */
        }
        else
        {
            priv->history.browse_idx = (priv->history.browse_idx + (max_history_count - 1)) % max_history_count;
        }
    }

    return cli_textline_history_pull(priv, priv->history.browse_idx);
}

/**
 * @brief 履歴取得(新しい履歴に進む)
 *        履歴としてストレージしているテキストデータを、古いデータから、新しいデータの順に呼び出す。
 * @param priv テキストラインデータへのポインタ
 * @return     テキストのポインタ
 */
const char *cli_textline_get_history_next(cli_private_t *priv)
{
    if (HISTORY_BROWSE_UNREAD == priv->history.browse_idx) return NULL;

    const char *text = NULL;

    uint32_t max_history_count = priv->history.max_size / priv->text.max_size;
    uint32_t latest_idx = (priv->history.write_idx + (max_history_count - 1)) % max_history_count;

    if ((uint32_t)priv->history.browse_idx == latest_idx)
    {
        /* 履歴の閲覧を終了 */
        /* 閲覧位置(index)を未閲覧に設定 */
        priv->history.browse_idx = HISTORY_BROWSE_UNREAD;

        text = "";
    }
    else
    {
        priv->history.browse_idx = (priv->history.browse_idx + 1) % max_history_count;

        text = cli_textline_history_pull(priv, priv->history.browse_idx);
    }

    return text;
}

uint32_t cli_textline_get_cursor_pos(cli_private_t *priv)
{
    return priv->text.cursor;
}