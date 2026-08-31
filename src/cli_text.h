/****************************************************************************************************
 * @file    cli_text.h
 * @brief   テキストデータの操作機能
 * @details 外部公開するAPI、型定義、マクロを定義する。
 *
 * @author  Masakazu Inoue
 * @date    2026/07/04          新規作成
 ****************************************************************************************************/
#ifndef __CLI_TEXT_H__
#define __CLI_TEXT_H__

/****************************************************************************************************
 * Public include
 ****************************************************************************************************/
#include "cli_private.h"

/****************************************************************************************************
 * Public define
 ****************************************************************************************************/

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
 * @param priv 制御データ(context)のポインタ
 * @param c    キャラクタ
 */
void cli_text_add_char(cli_private_t *priv, char c);

/**
 * @brief テキスト追加
 *        テキスト行のカーソル位置にテキストを追加する
 * @param priv 制御データ(context)のポインタ
 * @param text テキストのポインタ
 */
void cli_text_add_text(cli_private_t *priv, const char *text);

/**
 * @brief キャラクタ消去
 *        テキスト行のカーソル位置からキャラクタを消去する
 * @param priv 制御データ(context)のポインタ
 */
void cli_text_delete_char(cli_private_t *priv);

/**
 * @brief テキスト消去
 *        テキスト行のすべてのテキストを消去する
 * @param priv 制御データ(context)のポインタ
 */
void cli_text_delete_text(cli_private_t *priv);

/**
 * @brief テキスト保存
 *        履歴データとして、引数textの保存を行う。
 * @param priv 制御データ(context)のポインタ
 * @param text テキストのポインタ
 */
void cli_text_storage_text(cli_private_t *priv, const char *text);


/********************
 * Setter functions
 ********************/

/**
 * @brief 履歴設定
 *        履歴用の循環バッファに引数textの設定処理を行う。
 * @param priv 制御データ(context)のポインタ
 * @param text テキストのポインタ
 */
void cli_text_set_history(cli_private_t *priv, const char *text);

/**
 * @brief カーソル位置の設定
 * @param priv 制御データ(context)のポインタ
 * @param pos  設定するカーソル位置
 */
void cli_text_set_cursor_pos(cli_private_t *priv, uint32_t pos);


/********************
 * Getter functions
 ********************/

/**
 * @brief 現在行のポインタを取得
 * @param priv 制御データ(context)のポインタ
 * @return     テキストのポインタ
 */
uint8_t *cli_text_get_current_line(cli_private_t *priv);

/**
 * @brief 現在行の最大バッファサイズを取得
 * @param priv 制御データ(context)のポインタ
 * @return     最大バッファサイズ(byte)
 */
uint32_t cli_text_get_current_line_max_size(cli_private_t *priv);

/**
 * @brief 履歴取得(古い履歴を遡る)
 *        履歴としてストレージしているテキストデータを、新しいデータから、古いデータの順に呼び出す。
 * @param priv 制御データ(context)のポインタ
 * @return     テキストのポインタ
 */
const char *cli_text_get_history_prev(cli_private_t *priv);

/**
 * @brief 履歴取得(新しい履歴に進む)
 *        履歴としてストレージしているテキストデータを、古いデータから、新しいデータの順に呼び出す。
 * @param priv 制御データ(context)のポインタ
 * @return     テキストのポインタ
 */
const char *cli_text_get_history_next(cli_private_t *priv);

/**
 * @brief カーソル位置の取得
 * @param priv 制御データ(context)のポインタ
 * @return 現在のカーソル位置
 */
uint32_t cli_text_get_cursor_pos(cli_private_t *priv);


/********************
 * Other functions
 ********************/

/**
 * @brief カーソル右移動
 *        テキスト行上のカーソル位置を右に移動する
 * @param priv 制御データ(context)のポインタ
 */
void cli_text_cursor_right(cli_private_t *priv);

/**
 * @brief カーソル左移動
 *        テキスト行上のカーソル位置を左に移動する
 * @param priv 制御データ(context)のポインタ
 */
void cli_text_cursor_left(cli_private_t *priv);

/**
 * @brief 改行処理
 * @param priv 制御データ(context)のポインタ
 */
void cli_text_break(cli_private_t *priv);


#endif  /* __CLI_TEXT_H__ */