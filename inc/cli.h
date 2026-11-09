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

/* 不透明型で定義するデータ構造のメモリサイズ */
#define CLI_CONTEXT_OPAQUE_BYTES        (128)
#define CLI_CONTEXT_OPAQUE_WORDS        (CLI_CONTEXT_OPAQUE_BYTES / sizeof(uint32_t))

/****************************************************************************************************
 * Public typedef
 ****************************************************************************************************/

/**
 * @brief インスタンス定義用の不透明型構造体
 * 
 * @note
 * 4バイト（uint32_t）アライメントを強制するための配列定義です。
 * このデータ構造に対しての直接操作は禁止しています。
 * データ構造に対する操作はAPIを介してのみ許可しています。APIを使用しない場合の動作は保証できません。
 */
typedef struct
{
    uint32_t opaque[CLI_CONTEXT_OPAQUE_WORDS];
} cli_context_t;


/**
 * @brief CLIのエラーコード
 */
typedef enum
{
    CLI_OK = 0,

    CLI_ERROR_INVALID_INSTANCE,
    CLI_ERROR_INVALID_ARGUMENT,

    CLI_ERROR_CONFIG_PARM,
    CLI_ERROR_MEMORY_SIZE,

    CLI_ERROR_SEND_CHAR_BUSY,

    CLI_ERROR_OUTPUT_WRITE_FAILED,
    CLI_ERROR_OUTPUT_NOT_REGISTERED,

    CLI_CODE_MAX
} cli_code_t;

/**
 * @brief コマンドのエラーコード
 */
typedef enum
{
    CLI_COMMAND_COMPLETE,
    CLI_COMMAND_RUNNING,
    CLI_COMMAND_ERROR,

    CLI_COMMAND_MAX
} cli_command_code_t;

/**
 * @brief コマンド定義用データ
 *        ユーザ定義コマンドは本データ構造を使用して定義します。
 * @note
 * 各データの用途を記載します。
 * - name        : コマンドの名称
 * - handler     : コマンド実行要求を通知するコマンドハンドラ
 * - callback    : コマンド停止要求を通知するコールバック
 * - description : コマンドの説明・概要
 * 
 * コマンドを利用するには、本データのnameとhandlerの登録が必要です。
 * callbackは強制終了等の割り込みシグナルに応じてコマンドを停止させたい場合に使用します。
 * ワンショットで処理が完了する場合、callbackの登録は必ずしも必要ではありません。
 */
typedef struct
{
    const char * name;
    cli_command_code_t (* handler)(int argc, char **argv);
    cli_command_code_t (* callback)(void);

    const char * description;
} cli_command_t;


/**
 * @brief writeインターフェースのエラーコード
 */
typedef enum
{
    CLI_OUTPUT_OK = 0,
    CLI_OUTPUT_ERROR,
    CLI_OUTPUT_MAX
} cli_output_code_t;

/**
 * @brief writeインターフェース用構造体定義
 */
typedef struct
{
    cli_output_code_t (* write)( const uint8_t * p, uint32_t s );
} cli_output_t;


/**
 * @brief 設定パラメータ構造体
 */
typedef struct
{
    /** プロンプトに表示する文字列*/
    const char * prompt;

    /** 入力可能な文字データのバッファサイズ(byte) */
    uint16_t max_fifo_size;

    /** コマンドラインに割り当てるバッファサイズ(byte)*/
    uint32_t max_line_size;

    /** コマンドライン引数のvector数*/
    uint32_t depth_argv;

    /** コマンドラインの履歴件数*/
    uint32_t depth_history;
} cli_config_t;

/****************************************************************************************************
 * Public Variables
 ****************************************************************************************************/

/****************************************************************************************************
 * Public Functions
 ****************************************************************************************************/

/**
 * @brief CLI初期化
 * @param ctx       制御データ(context)のポインタ
 * @param cfg       設定データのポインタ
 * @param workspace workspaceのポインタ
 * @param size      workspaceのメモリサイズ
 * @return 処理結果
 */
cli_code_t cli_init(cli_context_t * ctx, const cli_config_t * cfg, void * workspace, size_t size);

/**
 * @brief 初期化解除
 * @param ctx       制御データ(context)のポインタ
 * @return 処理結果
 */
cli_code_t cli_deinit(cli_context_t *ctx);

/**
 * @brief メイン処理
 * @param ctx       制御データ(context)のポインタ
 * @return 処理結果
 */
cli_code_t cli_process(cli_context_t *ctx);

/**
 * @brief 文字データ送信
 * @param ctx       制御データ(context)のポインタ
 * @param c         入力文字（キャラクタ）
 * @return 処理結果
 */
cli_code_t cli_send_char(cli_context_t *ctx, char c);

/**
 * @brief workspaceの必要サイズ取得
 *        指定された設定パラメータからworkspaceに必要なメモリサイズを算出します。
 * @param cfg 設定パラメータのポインタ
 * @return 必要サイズ
 */
size_t cli_get_workspace_size(const cli_config_t * cfg);


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
cli_code_t cli_command_register(cli_context_t *ctx, const cli_command_t *list, uint16_t size);

/**
 * @brief コマンド登録解除
 * @param ctx  制御データ(context)のポインタ
 * @return 処理結果
 */
cli_code_t cli_command_unregister(cli_context_t *ctx);


/**
 * @brief writeインターフェース登録
 * @param ctx  制御データ(context)のポインタ
 * @param output writeインターフェースのポインタ
 * @return 処理結果
 */
cli_code_t cli_output_register(cli_context_t * ctx, const cli_output_t * output);

#endif  /* __CLI_H__ */