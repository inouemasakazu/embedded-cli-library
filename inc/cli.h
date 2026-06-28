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
#include <stdint.h>
#include <stdbool.h>

/****************************************************************************************************
 * Public define
 ****************************************************************************************************/
#define CLI_LINE_SIZE      128      /* 128byte buf */

#define CLI_CMD_NAME_SIZE   32      /* 32byte cmd name */

#define CLI_CMD_ENTRY_MAX   10
#define CLI_CMD_ARGS_MAX     8

/****************************************************************************************************
 * Public typedef
 ****************************************************************************************************/

/**
 * @brief CLIコマンドハンドラ型
 * @param argc コマンド引数の数
 * @param argv コマンド引数を格納する文字列配列
 * @return 処理結果
 */
typedef int (*cmd_handler_t)(int argc, char **argv);

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
    cmd_handler_t handler;
    bool is_used;
} cli_cmd_entry_t;

typedef struct
{
    int argc;
    char *argv[CLI_CMD_ARGS_MAX];
} cli_cmd_args_t;

typedef struct
{
    char buf[CLI_LINE_SIZE];
    unsigned short size;
} cli_line_t;

typedef struct
{
    const char *prompt;

    cli_line_t current_line;

    /* cli cmd structure */
    cli_cmd_entry_t cmd[CLI_CMD_ENTRY_MAX];
    cli_cmd_args_t args;

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
 * @brief CLIキャラクタ入力処理
 *        入力するキャラクタデータの管理・コマンド実行の判断を行う。
 * @param c 入力するキャラクタデータ
 * @return 処理結果
 */
int cli_input_char(cli_context_t *ctx, char c);

/**
 * @brief コマンド登録
 * @param name    登録コマンドの名称
 * @param handler コマンド実行時に呼び出されるハンドラ
 * @return 処理結果
 */
int cli_cmd_register(cli_context_t *ctx, const char *name, cmd_handler_t handler);

/**
 * @brief コマンド登録解除
 * @param name 登録解除するコマンドの名称
 * @return 処理結果
 */
int cli_cmd_unregister(cli_context_t *ctx, const char *name);


/********************
 * Setter functions
 ********************/

/**
 * @brief プロンプト設定
 * @param prompt プロンプトとして表示する文字列を示すメモリ領域
 * @return 処理結果
 */
int cli_set_prompt(cli_context_t *ctx, const char *prompt);

/**
 * @brief CLI用標準出力のコールバック設定
 *        CLIモジュール内で使用する出力処理のコールバックを設定する。
 * @return 処理結果
 */
int cli_set_stdout_cb(cli_context_t *ctx, stdout_cb_t stdout_cb);


#endif  /* __CLI_H__ */