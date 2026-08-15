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
    uint8_t *line;
    uint32_t max_size;

    uint32_t cursor;
} cli_text_t;

typedef struct
{
    bool is_valid;

    uint8_t *buffer;
    uint32_t max_size;

    uint32_t head;
    uint32_t tail;

    uint32_t count;
} cli_history_t;

typedef struct
{
    uint8_t *buf;
    uint32_t max_size;
} cli_output_t;

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

    cli_text_t text;
    cli_history_t history;

    cli_output_t output;

    /* コマンドライン引数 */
    cli_argument_t argument;
    cli_command_t *command_list;
    uint16_t list_size;

    /* cli write structure */
    io_write_cb_t io_write_cb;
} cli_private_t;

/****************************************************************************************************
 * Public Variables
 ****************************************************************************************************/

/****************************************************************************************************
 * Public Functions
 ****************************************************************************************************/

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