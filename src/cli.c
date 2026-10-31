/****************************************************************************************************
 * @file    cli.c
 * @brief   CLI(Command Line Interface)ライブラリ
 * @details このファイルではCLIライブラリ用のモジュールを定義。
 *
 * @author  Masakazu Inoue
 * @date    2026/05/24          新規作成
 ****************************************************************************************************/

/****************************************************************************************************
 * Private include
 ****************************************************************************************************/
#include "../inc/cli.h"
#include "../inc/cli_output.h"

#include "cli_private.h"

#include "cli_text.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/****************************************************************************************************
 * Private define
 ****************************************************************************************************/
#ifdef CLI_DEBUG
    #define CLI_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            while (1); \
        } \
    } while (0)
#else
    #define CLI_ASSERT(expr) ((void)0U)
#endif /* CLI_DEBUG */

/****************************************************************************************************
 * Private typedef
 ****************************************************************************************************/

/****************************************************************************************************
 * Private Variables
 ****************************************************************************************************/

/****************************************************************************************************
 * Private Functions
 ****************************************************************************************************/

static int cli_set_config(cli_private_t *priv, const cli_config_t *cfg, void *workspace, size_t size);
static int cli_workspace_memory_layout(cli_private_t *priv, const cli_config_t *cfg, void *workspace, size_t size);

static void cli_input_analysis( cli_private_t *priv, char c );

static void cli_emit_event( cli_private_t *priv, bool is_control, char command );

static void cli_enter(cli_private_t *priv);
static void cli_tokenizer(cli_private_t *priv);


static void cli_command_execute(cli_private_t *priv);
static const cli_command_t *cli_command_find(cli_private_t *priv, const char *name);

static cli_impl_code_t cli_fifo_push(cli_private_t * priv, uint8_t data);
static cli_impl_code_t cli_fifo_pull(cli_private_t * priv, uint8_t * data);
static uint16_t cli_fifo_is_items(cli_private_t * priv);


static void cli_set_prompt(cli_private_t *priv, const char *prompt);
static void cli_set_output_write(cli_private_t *priv, cli_output_write_t output_write);
static void cli_set_command_list(cli_private_t *priv, const cli_command_t *list);
static void cli_set_command_list_size(cli_private_t *priv, uint16_t size);

static const cli_command_t *cli_get_command_list(cli_private_t *priv);
static uint16_t cli_get_command_list_size(cli_private_t *priv);


/**
 * @brief CLI初期化
 * @param ctx       制御データ(context)のポインタ
 * @param cfg       設定データのポインタ
 * @param workspace workspaceのポインタ
 * @param size      workspaceのメモリサイズ
 * @return 処理結果
 */
cli_code_t cli_init(cli_context_t *ctx, const cli_config_t *cfg, void *workspace, size_t size)
{
    if (ctx == NULL) return CLI_CODE_INVALID_INSTANCE;

    cli_private_t *priv = get_priv(ctx);

    /* メモリ0クリア */
    memset(priv, 0, sizeof(*priv));

    /* 設定データを展開 */
    if (cli_set_config(priv, cfg, workspace, size) != 0)
    {
        return -2;
    }

    return CLI_CODE_OK;
}

/**
 * @brief 初期化解除
 * @param ctx       制御データ(context)のポインタ
 * @return 処理結果
 */
cli_code_t cli_deinit(cli_context_t *ctx)
{
    if (ctx == NULL) return CLI_CODE_INVALID_INSTANCE;

    cli_private_t *priv = get_priv(ctx);

    /* メモリ0クリア */
    memset(priv, 0, sizeof(*priv));

    return CLI_CODE_OK;
}

/**
 * @brief メイン処理
 * @param ctx       制御データ(context)のポインタ
 * @return 処理結果
 */
cli_code_t cli_process(cli_context_t *ctx)
{
    if (ctx == NULL) return CLI_CODE_INVALID_INSTANCE;

    cli_private_t *priv = get_priv(ctx);

    while ( cli_fifo_is_items( priv ) )
    {
        uint8_t c;

        if ( CLI_IMPL_CODE_OK == cli_fifo_pull( priv, &c ) )
        {
            cli_input_analysis( priv, ((char)c) );
        }
    }

    return CLI_CODE_OK;
}

/**
 * @brief 文字データ送信
 * @param ctx       制御データ(context)のポインタ
 * @param c         入力文字（キャラクタ）
 * @return 処理結果
 */
cli_code_t cli_send_char(cli_context_t *ctx, char c)
{
    if (ctx == NULL) return CLI_CODE_INVALID_INSTANCE;

    cli_private_t *priv = get_priv(ctx);

    if ( CLI_IMPL_CODE_FIFO_FULL == cli_fifo_push(priv, (uint8_t)c) )
    {
        /* pushしたデータをprocessが処理しきれていないためBUSYを返す */
        return CLI_CODE_SEND_CHAR_BUSY;
    }

    return CLI_CODE_OK;
}


/********************
 * static functions
 ********************/

/**
 * @brief 設定データ展開
 * @param priv      制御データ(context)のポインタ
 * @param cfg       設定データのポインタ
 * @param workspace workspaceのポインタ
 * @param size      workspaceのメモリサイズ
 * @return 処理結果
 */
static int cli_set_config(cli_private_t *priv, const cli_config_t *cfg, void *workspace, size_t size)
{
    if (cfg == NULL) return -1;
    if (workspace == NULL) return -1;

    if (cli_workspace_memory_layout(priv, cfg, workspace, size) == 0)
    {
        /* プロンプト設定 */
        cli_set_prompt(priv, cfg->prompt);

        /* writeインタフェース設定 */
        cli_set_output_write(priv, cfg->output_write);
    }
    else
    {
        return -2;
    }

    return CLI_CODE_OK;
}

/**
 * @brief ワークスペースのメモリレイアウト
 */
int cli_workspace_memory_layout(cli_private_t *priv, const cli_config_t *cfg, void *workspace, size_t size)
{
    uint32_t total = 0;

    /* 設定データからCLIが使用するワーキングメモリーの必要byte数を算出 */
    total = cfg->max_line_size;
    total = total + (cfg->depth_argv * sizeof(char *));
    total = total + (cfg->max_line_size * cfg->depth_history);

    if (0 == size)
    {
        /* workspaceのサイズがない */
        return -1;
    }
    else if (total > size)
    {
        /* workspaceのサイズが不足 */
        return -2;
    }
    else
    {
        /* DO NOTHING */
    }

    uint32_t offset = 0;

    memset(workspace, 0x00, size);

    priv->fifo                       = workspace;
    priv->fifo_size                  = cfg->max_fifo_size;

    offset = cfg->max_fifo_size;

    priv->text.current_line          = workspace;
    priv->text.current_line_max_size = cfg->max_line_size;

    offset = offset + cfg->max_line_size;

    priv->text.history_buffer        = (workspace + offset);
    priv->text.history_depth         = cfg->depth_history;

    offset = offset + (cfg->max_line_size * cfg->depth_history);

    priv->argument.vector = (char **)(workspace + offset);
    priv->argument.max_size = (cfg->depth_argv * sizeof(char *));

    offset = offset + priv->argument.max_size;

    return CLI_CODE_OK;
}

/**
 * @brief 入力解析
 *        キャラクタ・制御文字・エスケープシーケンスの解析を行い、解析結果に応じたイベントを発行する
 */
static void cli_input_analysis( cli_private_t *priv, char c )
{
    switch ( priv->parse_state )
    {
    case CLI_PARSE_NOMAL:
        if ( (0 < c) && (c <= 31) )
        {
            if ( '\e' == c )
            {
                /* エスケープコード（ESC）を確認 */
                /* ANSI識別文字の受信待ちに遷移 */
                priv->parse_state = CLI_PARSE_ESC;
            }
            else
            {
                /* 制御文字 */
                cli_emit_event( priv, 1, c );
            }
        }
        else
        {
            /* キャラクタ */
            cli_emit_event( priv, 0, c );
        }
        break;

    case CLI_PARSE_ESC:
        if ( '[' == c )
        {
            /* ANSI識別文字を確認 */
            /* シーケンス識別文字の受信待ちに遷移 */
            priv->parse_state = CLI_PARSE_CSI;
        }
        else
        {
            /* 不正なシーケンスは無視して通常状態に戻す */
            priv->parse_state = CLI_PARSE_NOMAL;
        }

        break;

    case CLI_PARSE_CSI:
        cli_emit_event( priv, 1, c );

        priv->parse_state = CLI_PARSE_NOMAL;
        break;

    default:
        break;
    }
}

/**
 * @brief イベント発行
 *        引数のcommandに応じたイベントを発行する
 */
static void cli_emit_event( cli_private_t *priv, bool is_control, char command )
{
    if ( is_control )
    {
        /* エスケープシーケンス・制御文字に対応するイベントを発行 */
        if ( ('A' == command) || ('B' == command) || ('C' == command) || ('D' == command) )
        {
            priv->event.type    = CLI_EVENT_CURSOR_MOVE;
            priv->event.parm[0] = command;
            priv->event.parm[1] = '\r';
            priv->event.parm[2] = '\n';
            priv->event.parm[3] = '\0';

            output_string(priv, "[DEBUG] CLI_EVENT_CURSOR_MOVE : ");
            output_string(priv, (const char *)priv->event.parm);
        }
        else if ( (LF == command) || (CR == command) )
        {
            priv->event.type    = CLI_EVENT_ENTER;
            priv->event.parm[0] = '\0';

            output_string(priv, "[DEBUG] CLI_EVENT_ENTER\r\n");
        }
        else if ( BS == command )
        {
            priv->event.type    = CLI_EVENT_BACKSPACE;
            priv->event.parm[0] = '\0';

            output_string(priv, "[DEBUG] CLI_EVENT_BACKSPACE\r\n");
        }
        else if ( ETX == command )
        {
            priv->event.type   = CLI_EVENT_INPUT_INTERRUPT;

            output_string(priv, "[DEBUG] CLI_EVENT_INPUT_INTERRUPT\r\n");
        }
        else
        {
            /* 該当するイベントがない */
            priv->event.type    = CLI_EVENT_NONE;
            priv->event.parm[0] = '\0';

            output_string(priv, "[DEBUG] CLI_EVENT_NONE\r\n");
        }
    }
    else
    {
        /* テキストデータのイベントを発行 */
        priv->event.type    = CLI_EVENT_TEXT;
        priv->event.parm[0] = command;
        priv->event.parm[1] = '\r';
        priv->event.parm[2] = '\n';
        priv->event.parm[3] = '\0';

        output_string(priv, "[DEBUG] CLI_EVENT_TEXT : ");
        output_string(priv, (const char *)priv->event.parm);
    }
}


/**
 * @brief ENTER入力時処理
 *        現在行用のバッファが保持するテキストデータをコマンドラインとして確定し、コマンドラインに以下の処理を行う。
 *        - 現在のテキストデータを履歴データとして保存する。
 *        - コマンドラインのトークン化を行う。
 *        - コマンドハンドラの実行。
 * @param priv 制御データ(context)のポインタ
 * @return None
 */
static void cli_enter(cli_private_t *priv)
{
    char *text = (char *)cli_text_get_current_line(priv);
    size_t text_len = strlen((const char *)text);

    if (0 < text_len)
    {
        /* 現在のテキストデータを履歴データとして保存 */
        cli_text_storage_text(priv, (const char *)text);

        /* コマンドラインをトークン化 */
        cli_tokenizer(priv);

        if (0 < priv->argument.count)
        {
            /* コマンド実行 */
            cli_command_execute(priv);
        }

        /* テキストをすべて消去 */
        cli_text_delete_text(priv);
    }

    output_string(priv, "\r\n");
}

/**
 * @brief トークン作成
 *        入力されたコマンドラインからトークンの作成を行う。コマンドラインはスペース区切りで分割(トークン化)する。
 * @param priv 制御データ(context)のポインタ
 * @return None
 * @note
 * コマンドライン引数はコマンドハンドラの実行時に使用するデータとして作成する。
 *
 * 作成するコマンドライン引数は以下の通り。
 * - argc コマンドライン引数の数
 * - argv コマンドライン引数を格納する文字列配列
 *
 * いずれも制御データ(context)内に保持するデータとして扱うが、データの生存期間はコマンドハンドラの実行～終了までとする。
 * 以降はデータの使用を想定していないため、データが保持する値は保証しない。
 */
static void cli_tokenizer(cli_private_t *priv)
{
    char *token = (char *)cli_text_get_current_line(priv);
    uint32_t max_count = 0;

    priv->argument.count = 0;
    memset(priv->argument.vector, 0x00, priv->argument.max_size);

    max_count = (priv->argument.max_size / sizeof(char *));

    /* コマンドラインをスペース区切りでトークンに分割 */
    while ((*token != '\0') && (priv->argument.count < max_count))
    {
        while (*token == ' ') token++;

        priv->argument.vector[priv->argument.count++] = token;

        while ((*token != ' ') && (*token != '\0'))
        {
            token++;
        }

        if (*token == '\0')
        {
            break;
        }

        *token = '\0';
        token++;
    }

    if (priv->argument.count < max_count)
    {
        /* 引数の最大数に達していない場合は、argvの最後をNULLにする */
        priv->argument.vector[priv->argument.count] = NULL;
    }
}


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
cli_code_t cli_command_register(cli_context_t *ctx, const cli_command_t *list, uint16_t size)
{
    if (ctx == NULL) return CLI_CODE_INVALID_INSTANCE;
    if ((list == NULL) || (size == 0)) return CLI_CODE_INVALID_ARGUMENT;

    cli_private_t *priv = get_priv(ctx);

    cli_set_command_list(priv, list);
    cli_set_command_list_size(priv, size);

    return CLI_CODE_OK;
}

/**
 * @brief コマンド登録解除
 * @param ctx  制御データ(context)のポインタ
 * @return 処理結果
 */
cli_code_t cli_command_unregister(cli_context_t *ctx)
{
    if (ctx == NULL) return CLI_CODE_INVALID_INSTANCE;

    cli_private_t *priv = get_priv(ctx);

    cli_set_command_list(priv, NULL);
    cli_set_command_list_size(priv, 0);

    return CLI_CODE_OK;
}

/**
 * @brief コマンドハンドラ実行
 *        コマンドライン引数に一致するコマンドをエントリーテーブルから取得し、ハンドラを実行する。
 * @param priv 制御データ(context)のポインタ
 * @return None
 */
static void cli_command_execute(cli_private_t *priv)
{
    int argc    = priv->argument.count;
    char **argv = priv->argument.vector;

    if ((argc != 0) || (argv != NULL))
    {
        const cli_command_t *command = cli_command_find(priv, argv[0]);

        if (command)
        {
            /* ハンドラ実行 */
            if (command->handler(argc, argv) != 0)
            {
                output_string(priv, "\r\nError: command execution failed\r\n");
            }
        }
        else
        {
            output_string(priv, "\r\nError: '");
            output_string(priv, priv->argument.vector[0]);
            output_string(priv, "' command not found\r\n");
        }
    }
}

/**
 * @brief コマンド探索
 *        登録済みコマンドの登録名称と、引数nameに一致するコマンドの探索を行う。
 * @param priv 制御データ(context)のポインタ
 * @param name 探索対象の名称(登録名)を示すポインタ
 * @return コマンドのポインタ、またはNULLポインタ
 */
static const cli_command_t *cli_command_find(cli_private_t *priv, const char *name)
{
    if (name == NULL) return NULL;

    const cli_command_t *list = cli_get_command_list(priv);
    uint16_t list_size        = cli_get_command_list_size(priv);

    if (list != NULL)
    {
        for (uint16_t i = 0; i < list_size; i++)
        {
            if ((strcmp(list->name, name) == 0) && (list->handler))
            {
                /* リスト内にコマンドあり */
                return (cli_command_t *)list;
            }
            else
            {
                /* 名称不一致、ハンドラ未登録は一致コマンドなしと判断する */
            }

            list++;
        }
    }

    return NULL;
}


/********************
 * fifo functions
 ********************/

/**
 * データをFIFOにpushする
 */
static cli_impl_code_t cli_fifo_push(cli_private_t * priv, uint8_t data)
{
    uint16_t next = (priv->fifo_head + 1) % priv->fifo_size;

    if (next == priv->fifo_tail)
    {
        /* 空きなし */
        return CLI_IMPL_CODE_FIFO_FULL;
    }

    uint8_t *p = priv->fifo;
    uint16_t offset = priv->fifo_head;

    *(p + offset) = data;

    priv->fifo_head = next;

    return CLI_IMPL_CODE_OK;
}

/**
 * データをFIFOからpullする
 */
static cli_impl_code_t cli_fifo_pull(cli_private_t * priv, uint8_t * data)
{
    if (priv->fifo_head == priv->fifo_tail)
    {
        /* データなし */
        return CLI_IMPL_CODE_FIFO_EMPTY;
    }

    uint8_t *p = priv->fifo;
    uint16_t offset = priv->fifo_tail;

    *data = *(p + offset);

    priv->fifo_tail = (priv->fifo_tail + 1) % priv->fifo_size;

    return CLI_IMPL_CODE_OK;
}

/**
 * FIFO内の有効データサイズを取得
 */
static uint16_t cli_fifo_is_items(cli_private_t * priv)
{
    return ((priv->fifo_head + priv->fifo_size) - priv->fifo_tail) % priv->fifo_size;
}


/********************
 * Setter functions
 ********************/

/**
 * @brief プロンプト設定
 * @param prompt プロンプトとして表示する文字列を示すメモリ領域
 */
static void cli_set_prompt(cli_private_t *priv, const char *prompt)
{
    if (prompt)
    {
        priv->prompt = prompt;
    }
    else
    {
        /* デフォルトのプロンプト('>')を設定 */
        priv->prompt = ">";
    }
}

/**
 * @brief 出力用writeインターフェース登録
 *        CLIが使用するwriteインターフェースの登録を行う。
 */
static void cli_set_output_write(cli_private_t *priv, cli_output_write_t output_write)
{
    if (output_write)
    {
        priv->output_write = output_write;
    }
}

/**
 * @brief コマンドを設定
 */
static void cli_set_command_list(cli_private_t *priv, const cli_command_t *list)
{
    priv->command_list = list;
}

/**
 * @brief コマンドの要素数を設定
 */
static void cli_set_command_list_size(cli_private_t *priv, uint16_t size)
{
    priv->list_size = size;
}


/********************
 * Getter functions
 ********************/

/**
 * @brief コマンドを取得
 */
static const cli_command_t *cli_get_command_list(cli_private_t *priv)
{
    return (cli_command_t *)priv->command_list;
}

/**
 * @brief コマンドの要素数を取得
 */
static uint16_t cli_get_command_list_size(cli_private_t *priv)
{
    return priv->list_size;
}