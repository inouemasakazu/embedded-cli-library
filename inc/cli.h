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
#include <stddef.h>

/****************************************************************************************************
 * Public define
 ****************************************************************************************************/

/****************************************************************************************************
 * Public typedef
 ****************************************************************************************************/

/**
 * @brief CLIコマンドハンドラ型
 * @param argc コマンドライン引数の数
 * @param argv コマンドライン引数を格納する文字列配列
 * @return 処理結果
 */
typedef int (*cli_command_handler_t)(int argc, char **argv);

/**
 * @brief CLIの出力用writeインターフェース
 * @param p 出力データのポインタ
 * @param s 出力データサイズ
 * @return 処理結果
 */
typedef int (*cli_output_write_t)(const uint8_t *p, uint32_t s);


/**
 * @brief インスタンス生成用構造体
 *        本データは公開APIを介してのみ、アクセスが可能。
 *        操作を行った場合は動作の保証ができない。
 */
typedef struct
{
    uint32_t opaque[32];
} cli_context_t;


/**
 * @brief CLI利用時の設定データ
 */
typedef struct
{
    /** プロンプトに表示する文字列*/
    const char *prompt;

    /** コマンドラインに割り当てるバッファサイズ(byte)*/
    uint32_t max_line_size;

    /** コマンドライン引数のvector数*/
    uint32_t depth_argv;

    /** コマンドラインの履歴件数*/
    uint32_t depth_history;

    /** 出力用writeインターフェース*/
    cli_output_write_t output_write;
} cli_config_t;


/**
 * @brief コマンドのデータ
 */
typedef struct
{
    const char *name;
    cli_command_handler_t handler;
} cli_command_t;


/****************************************************************************************************
 * Public Variables
 ****************************************************************************************************/

/****************************************************************************************************
 * Public Functions
 ****************************************************************************************************/

/**
 * @brief CLI初期化
 * @param ctx 制御データ(context)のポインタ
 * @param cfg 設定データのポインタ
 * @param workspace workspaceメモリ
 * @param size workspaceのサイズ
 * @return 処理結果
 */
int cli_init(cli_context_t *ctx, const cli_config_t *cfg, void *workspace, size_t size);

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
 * @param ctx 制御データ(context)のポインタ
 * @param c   キャラクタ
 * @return 処理結果
 */
int cli_input_char(cli_context_t *ctx, char c);


/********************
 * Command functions
 ********************/

/**
 * @brief コマンド登録
 *        エントリーテーブルのポインタとテーブルの要素数を登録する。
 * @param ctx  制御データ(context)のポインタ
 * @param list エントリーテーブルのポインタ
 * @param size テーブルの要素数
 * @return 処理結果
 */
int cli_command_register(cli_context_t *ctx, const cli_command_t *list, uint16_t size);

/**
 * @brief コマンド登録解除
 * @param ctx  制御データ(context)のポインタ
 * @return 処理結果
 */
int cli_command_unregister(cli_context_t *ctx);


#endif  /* __CLI_H__ */