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

#define STX                 0x02    /* テキスト開始 */
#define ETX                 0x03    /* テキスト終了 */
#define BS                  '\b'    /* 後退 */
#define HT                  '\t'    /* 水平タブ */
#define LF                  '\n'    /* 改行 */
#define CR                  '\r'    /* 復帰 */
#define ESC                 '\e'    /* エスケープ */
#define DEL                 0x7f    /* 削除 */

/*** ASCiiコード(図形文字) ***/
#define SPC                 ' '     /* 空白文字 */


/****************************************************************************************************
 * Public MACROS
 ****************************************************************************************************/

#define CONCAT_SYM_SUB(a, b)    a ## b
#define CONCAT_SYM(a, b)        CONCAT_SYM_SUB(a, b)


#define CLI_ASSERT_HANDLER      while (1);

#define CLI_STATIC_ASSERT(expr, name)  \
    typedef char CONCAT_SYM(static_assert_##name##_, __LINE__)[(expr) ? 1 : -1]

#define CLI_ASSERT(expr)                    \
    do {                                    \
        if (!(expr)) {                      \
            CLI_ASSERT_HANDLER              \
        }                                   \
    } while (0)


/****************************************************************************************************
 * Public typedef
 ****************************************************************************************************/

typedef enum
{
    CLI_IMPL_OK = 0,

    /** FIFO操作時のエラーコード */
    CLI_IMPL_ERROR_FIFO_FULL,
    CLI_IMPL_ERROR_FIFO_EMPTY,

    CLI_IMPL_ERROR_OUTPUT_WRITE_FAILED,
    CLI_IMPL_ERROR_OUTPUT_NOT_REGISTERED,

    CLI_IMPL_MAX
} cli_impl_code_t;

/**
 * @brief CLIのシステム状態
 */
typedef enum {
    CLI_SYSTEM_STATE_IDLE = 0,
    CLI_SYSTEM_STATE_RUNNING,
    CLI_SYSTEM_STATE_MAX
} cli_system_t;

typedef enum
{
    CLI_PARSE_NOMAL = 0,
    CLI_PARSE_ESC,
    CLI_PARSE_CSI,
    CLI_PARSE_MAX
} cli_parse_t;

typedef enum
{
    CLI_EVENT_NONE = 0,
    CLI_EVENT_TEXT,
    CLI_EVENT_CURSOR_MOVE,
    CLI_EVENT_BACKSPACE,
    CLI_EVENT_ENTER,
    CLI_EVENT_INPUT_INTERRUPT,

    CLI_EVENT_MAX
} cli_event_type_t;

typedef struct
{
    cli_event_type_t type;
    char parm[4];
} cli_event_t;

typedef enum
{
    CLI_RENDER_NONE = 0,
    CLI_RENDER_STARTUP,
    CLI_RENDER_LINE_BREAK,
    CLI_RENDER_COMMANDLINE
} cli_render_required_t;

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

    cli_system_t system;
    cli_parse_t parse_state;

    cli_event_t event;

    /** fifo Data Structure */
    uint8_t *fifo;
    uint16_t fifo_size;
    uint16_t fifo_head;
    uint16_t fifo_tail;

    /** コマンドラインとして管理するテキストデータ */
    cli_text_t text;

    cli_render_required_t render_required;

    /* コマンドライン引数 */
    cli_argument_t argument;

    /** コマンドのエントリーテーブル*/
    const cli_command_t *command_list;
    uint16_t list_size;

    const cli_command_t * active_command;

    const cli_output_t * output;
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
void output_string(cli_private_t *priv, const char *p);

/**
 * @brief 型変換処理
 * @param priv 制御データ(context)のポインタ
 * @return 公開型に変換したポインタ
 */
static inline cli_context_t *get_public(cli_private_t *priv)
{
    void *vp = priv;

    return (cli_context_t *)vp;
}

/**
 * @brief 型変換処理
 * @param ctx 制御データ(context)のポインタ
 * @return 非公開型に変換したポインタ
 */
static inline cli_private_t *get_priv(cli_context_t *ctx)
{
    void *vp = ctx->opaque;

    return (cli_private_t *)vp;
}

/**
 * @brief 型変換処理(読み取り専用)
 * @param ctx 制御データ(context)のポインタ
 * @return 非公開型に変換したポインタ
 */
static inline const cli_private_t *get_priv_const(cli_context_t *ctx)
{
    const void *vp = ctx->opaque;

    return (const cli_private_t *)vp;
}

#endif  /* __CLI_PRIVATE_H__ */