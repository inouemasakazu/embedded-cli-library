/****************************************************************************************************
 * @file    cli.h
 * @brief   CLI(Command Line Interface)ライブラリ
 * @details 外部公開するAPI、型定義、マクロを定義する。
 *
 * @author  Masakazu Inoue
 * @date    2026/05/24     新規作成
 ****************************************************************************************************/
#ifndef __CLI_H__
#define __CLI_H__

/****************************************************************************************************
 * Public include
 ****************************************************************************************************/
#include "stdint.h"

#include <stdbool.h>

/****************************************************************************************************
 * Public define
 ****************************************************************************************************/
#define CLI_LINE_SIZE      128      /* 128byte buf */
#define CLI_ARGV_SIZE        8      /* 8 args */

#define CLI_CMD_NAME_SIZE   32      /* 32byte cmd name */
#define CLI_CMD_SIZE        10      /* 10 command */

/****************************************************************************************************
 * Public typedef
 ****************************************************************************************************/
typedef int (*cli_func_t)(int argc, char **argv);

/**
 * @brief CLI用標準出力のコールバック型
 * @param p 出力データのポインタ
 * @param s 出力データサイズ
 * @return 処理結果
 */
typedef int (*stdout_cb_t)(const char *p, uint16_t s);

typedef struct
{
    char name[CLI_CMD_NAME_SIZE];
    cli_func_t func;
    bool is_used;
} cli_cmd_t;

typedef struct
{
    int argc;
    char *argv[CLI_ARGV_SIZE];
} cli_line_args_t;

typedef struct
{
    char buf[CLI_LINE_SIZE];
    unsigned short size;
} cli_line_t;

typedef struct
{
    const char *prompt;

    cli_line_t current_line;
    cli_line_args_t args;

    /* cli cmd entry func */
    cli_cmd_t cmd[CLI_CMD_SIZE];

    /* cli write callback */
    char write_buf[CLI_LINE_SIZE * 2];    /* 書き込み用バッファ */

    stdout_cb_t stdout_cb;
} cli_context_t;

/****************************************************************************************************
 * Public Variables
 ****************************************************************************************************/

/****************************************************************************************************
 * Public Functions
 ****************************************************************************************************/

/**
 * @brief CLI初期化
 * @param ctx CLIの状態データを保持するメモリ領域
 * @return 処理結果
 */
int cli_init(cli_context_t *ctx);

/**
 * @brief CLI開始
 *        CLIの開始を通知するメッセージと、プロンプト('>')の表示を行う。
 * @param message 開始を通知するメッセージ(必要ない場合はNULLを設定)
 * @return 処理結果
 */
int cli_begin(cli_context_t *ctx, const char *message);

/**
 * @brief プロンプト設定
 * @param prompt プロンプトとして表示する文字列を示すメモリ領域
 * @return 処理結果
 */
int cli_set_prompt(cli_context_t *ctx, const char *prompt);

int cli_cmd_register(cli_context_t *ctx, const char *name, cli_func_t func);
int cli_cmd_unregister(cli_context_t *ctx, const char *name);

/**
 * @brief CLI入力処理
 *        CLIに入力する文字の整形と解釈を行う。
 * @param c CLIに入力する文字
 * @return 処理結果
 */
int cli_input_char(cli_context_t *ctx, char c);


/*** cli_stdio ***/

/**
 * @brief CLI用標準出力のコールバック設定
 *        CLIモジュール内で使用する出力処理のコールバックを設定する。
 * @return 処理結果
 */
int cli_set_stdout_cb(cli_context_t *ctx, stdout_cb_t stdout_cb);


#endif  /* __CLI_H__ */