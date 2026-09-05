/****************************************************************************************************
 * @file    cli_private.h
 * @brief   非公開定義
 * @details 外部公開しないAPI、型定義、マクロを定義する。
 *
 * @author  Masakazu Inoue
 * @date    2026/08/02     新規作成
 ****************************************************************************************************/
#ifndef __CLI_PRIVATE_H__
#define __CLI_PRIVATE_H__

/****************************************************************************************************
 * Public include
 ****************************************************************************************************/
#include "../inc/cli.h"

#include <stdint.h>
#include <stdbool.h>

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

/****************************************************************************************************
 * Public typedef
 ****************************************************************************************************/

typedef enum
{
    PARSE_STATE_NOMAL,
    PARSE_STATE_ESC_WAIT1,
    PARSE_STATE_ESC_WAIT2,
} parse_state_t;

typedef struct
{
    /** 現在操作中のテキストデータ(コマンドライン)*/
    uint8_t *current_line;
    uint32_t current_line_max_size;

    /** 履歴データ*/
    uint8_t *history_buffer;
    uint32_t history_depth;
    uint32_t history_write_idx;
    int32_t  history_browse_idx;
    uint32_t history_count;

    /** カーソル位置 */
    uint32_t cursor_pos;
} cli_text_t;

typedef struct
{
    int count;
    char **vector;
    uint32_t max_size;
} cli_argument_t;

typedef struct
{
    const char *prompt;

    parse_state_t state;

    /** コマンドラインとして管理するテキストデータ */
    cli_text_t text;

    /* コマンドライン引数 */
    cli_argument_t argument;

    /** コマンドのエントリーテーブル*/
    const cli_command_t *command_list;
    uint16_t list_size;

    /** writeインターフェースデータ */
    cli_output_write_t output_write;
} cli_private_t;


/****************************************************************************************************
 * Public Variables
 ****************************************************************************************************/

/****************************************************************************************************
 * Public Functions
 ****************************************************************************************************/

/**
 * @brief 文字列出力（内部処理用）
 * @param priv 制御データ(context)のポインタ
 * @param p 文字列のポインタ
 * @return 正常(0) / 失敗(-1) / CB未登録(1)
 */
int output_string(cli_private_t *priv, const char *p);

/**
 * @brief 型変換処理
 * @param ctx CLIの状態データを保持するメモリ領域
 * @return 非公開型に変換したポインタ
 */
static inline cli_context_t *get_public(cli_private_t *priv)
{
    void *vp = priv;

    return (cli_context_t *)vp;
}

/**
 * @brief 型変換処理
 * @param ctx CLIの状態データを保持するメモリ領域
 * @return 非公開型に変換したポインタ
 */
static inline cli_private_t *get_priv(cli_context_t *ctx)
{
    void *vp = ctx->opaque;

    return (cli_private_t *)vp;
}

/**
 * @brief 型変換処理(読み取り専用)
 * @param ctx CLIの状態データを保持するメモリ領域
 * @return 非公開型に変換したポインタ
 */
static inline const cli_private_t *get_priv_const(cli_context_t *ctx)
{
    const void *vp = ctx->opaque;

    return (const cli_private_t *)vp;
}

#endif  /* __CLI_PRIVATE_H__ */