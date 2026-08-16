/****************************************************************************************************
 * @file    cli_editor.h
 * @brief   Command Line編集
 * @details 外部公開するAPI、型定義、マクロを定義する。
 *
 * @author  Masakazu Inoue
 * @date    2026/07/04          新規作成
 ****************************************************************************************************/
#ifndef __CLI_EDIOTR_H__
#define __CLI_EDIOTR_H__

/****************************************************************************************************
 * Public include
 ****************************************************************************************************/
#include "../inc/cli.h"

#include "cli_private.h"

/****************************************************************************************************
 * Public define
 ****************************************************************************************************/
/*** ASCiiコード(制御文字) ***/
#define NUL                 '\0'    /* null文字 */
#define BS                  '\b'    /* 後退 */
#define HT                  '\t'    /* 水平タブ */
#define LF                  '\n'    /* 改行 */
#define CR                  '\r'    /* 復帰 */
#define ESC                 '\e'    /* エスケープ */
#define DEL                 0x7f    /* 削除 */

/*** ASCiiコード(図形文字) ***/
#define SPC                 ' '     /* 空白文字 */

#define HISTORY_BROWSE_UNREAD       -1

/****************************************************************************************************
 * Public typedef
 ****************************************************************************************************/

/****************************************************************************************************
 * Public Variables
 ****************************************************************************************************/

/****************************************************************************************************
 * Public Functions
 ****************************************************************************************************/

 /**
 * @brief キャラクタ追加
 *        テキスト行のカーソル位置にキャラクタを追加する
 * @param priv テキストラインデータへのポインタ
 * @param c    キャラクタ
 */
void cli_textline_add_char(cli_private_t *priv, char c);

/**
 * @brief キャラクタ消去
 *        テキスト行のカーソル位置からキャラクタを消去する
 * @param priv テキストラインデータへのポインタ
 */
void cli_textline_delete_char(cli_private_t *priv);

/**
 * @brief テキスト消去
 *        テキスト行のすべてのテキストを消去する
 * @param priv テキストラインデータへのポインタ
 */
void cli_textline_delete_text(cli_private_t *priv);

/**
 * @brief テキスト保存
 *        引数textを履歴データとして保存する。
 * @param priv テキストラインデータへのポインタ
 * @param text テキストのポインタ
 */
void cli_textline_storage_text(cli_private_t *priv, const char *text);

/**
 * @brief カーソル右移動
 *        テキスト行上のカーソル位置を右に移動する
 * @param priv テキストラインデータへのポインタ
 */
void cli_textline_cursor_right(cli_private_t *priv);

/**
 * @brief カーソル左移動
 *        テキスト行上のカーソル位置を左に移動する
 * @param priv テキストラインデータへのポインタ
 */
void cli_textline_cursor_left(cli_private_t *priv);

/**
 * @brief 改行処理
 * @param priv テキストラインデータへのポインタ
 */
void cli_textline_break(cli_private_t *priv);

/********************
 * Setter functions
 ********************/

/**
 * @brief 履歴設定
 *        引数textを履歴データとして設定する。
 * @param priv テキストラインデータへのポインタ
 * @param text テキストのポインタ
 */
void cli_textline_set_history(cli_private_t *priv, const char *text);

void cli_textline_set_cursor_pos(cli_private_t *priv, uint32_t pos);


/********************
 * Getter functions
 ********************/

/**
 * @brief 履歴取得(古い履歴を遡る)
 *        履歴としてストレージしているテキストデータを、新しいデータから、古いデータの順に呼び出す。
 * @param priv テキストラインデータへのポインタ
 * @return     テキストのポインタ
 */
const char *cli_textline_get_history_prev(cli_private_t *priv);

/**
 * @brief 履歴取得(新しい履歴に進む)
 *        履歴としてストレージしているテキストデータを、古いデータから、新しいデータの順に呼び出す。
 * @param priv テキストラインデータへのポインタ
 * @return     テキストのポインタ
 */
const char *cli_textline_get_history_next(cli_private_t *priv);

uint32_t cli_textline_get_cursor_pos(cli_private_t *priv);


#endif  /* __CLI_EDIOTR_H__ */