/****************************************************************************************************
 * @file    cli.c
 * @brief   CLI(Command Line Interface)ライブラリ
 * @details このファイルではCLIライブラリ用のモジュールを定義。
 *
 * @author  Masakazu Inoue
 * @date    2026/05/24          新規作成
 ****************************************************************************************************
 * @note
 * 利用手順を以下に示す。
 * 1. cli_init関数でCLIコンテキストの初期化を行う。
 * 2. cli_set_write_func関数で書き込み関数を設定する。
 * 3. cli_cmd_register関数でコマンド登録を行う。
 * 4. cli_printf関数+cli_show_prompt関数で起動時メッセージ+プロンプトを表示する(任意表示)。
 * 5. コマンド実行を行う場合はcli_input_char関数にコマンド入力文字を渡す(\r or \nの入力でコマンドを実行)。
 * 6. 必要に応じてcli_printf関数でコマンド実行結果を出力する。
 * 
 * @attention
 * 入力文字はASCIIコードを使用する。使用可能な文字は以下の通り。
 *  CLI_LINE_EDITTOR   : コマンドラインエディタで使用する文字(例: 'a', 'b', 'c', ...)
 *  CLI_ENTER_CHAR     : コマンド実行を示す文字(例: '\n')
 *  CLI_BACKSPACE_CHAR : コマンドラインエディタで使用する文字(例: '\b')
 *  CLI_ESCAPE_CHAR    : ESCシーンスは未対応(例: '\e')
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
static int cli_workspace_memory_layout(cli_private_t *priv, const cli_config_t *cfg, void *workspace, size_t size);

static void cli_char_proc(cli_private_t *priv, char c);
static void cli_esc_proc(cli_private_t *priv, char c);

static void cli_event_send(cli_private_t *priv, uint16_t event, uint32_t parm);
static void cli_event_handler(cli_private_t *priv, uint16_t event, uint32_t parm);

static void cli_enter(cli_private_t *priv);
static void cli_tokenizer(cli_private_t *priv);

static void cli_command_execute(cli_private_t *priv);
static const cli_command_t *cli_command_find(cli_private_t *priv, const char *name);


static void cli_set_prompt(cli_private_t *priv, const char *prompt);
static void cli_set_output_write(cli_private_t *priv, cli_output_write_t output_write);
static void cli_set_command_list(cli_private_t *priv, const cli_command_t *list);
static void cli_set_command_list_size(cli_private_t *priv, uint16_t size);

static const cli_command_t *cli_get_command_list(cli_private_t *priv);
static uint16_t cli_get_command_list_size(cli_private_t *priv);



/**
 * @brief CLI初期化
 * @param ctx CLIの状態データを保持するメモリ領域
 * @param cfg 設定データ
 * @param workspace workspaceメモリ
 * @param size workspaceのサイズ
 * @return 処理結果
 */
int cli_init(cli_context_t *ctx, const cli_config_t *cfg, void *workspace, size_t size)
{
    if ((ctx == NULL) || (cfg == NULL) || (workspace == NULL))
    {
        return -1;
    }

    cli_private_t *priv = get_priv(ctx);

    int success = 0;

    /* メモリ0クリア */
    memset(priv, 0, sizeof(*priv));

    success = cli_workspace_memory_layout(priv, cfg, workspace, size);
    if (success == 0)
    {
        /* 設定データを反映 */
        cli_set_prompt(priv, cfg->prompt);
        cli_set_output_write(priv, cfg->output_write);
    }

    return success;
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

    priv->text.current_line          = workspace;
    priv->text.current_line_max_size = cfg->max_line_size;

    offset = cfg->max_line_size;

    priv->text.history_buffer        = (workspace + offset);
    priv->text.history_depth         = cfg->depth_history;

    offset = offset + (cfg->max_line_size * cfg->depth_history);

    priv->argument.vector = (char **)(workspace + offset);
    priv->argument.max_size = (cfg->depth_argv * sizeof(char *));

    offset = offset + priv->argument.max_size;

    return 0;
}

/**
 * @brief CLI開始
 *        CLIの開始を通知するメッセージと、プロンプト('>')の表示を行う。
 * @param message 開始を通知するメッセージ(通知が必要ない場合はNULLを設定する)
 * @return 処理結果
 */
int cli_begin(cli_context_t *ctx, const char *message)
{
    int success = 0;

    if (ctx == NULL)
    {
        success = -1;
    }
    else
    {
        cli_private_t *priv = get_priv(ctx);

        if ((message != NULL) || (*message != '\0'))
        {
            /* 開始を通知するメッセージを表示 */
            output_string(priv, "\r\n");
            output_string(priv, message);
        }

        /* プロンプトを表示 */
        output_string(priv, "\r\n");
        output_string(priv, priv->prompt);
    }

    return success;
}

/**
 * @brief CLIキャラクタ入力処理
 *        入力するキャラクタデータの管理・コマンド実行の判断を行う。
 * @param c 入力するキャラクタデータ
 * @return 処理結果
 */
int cli_input_char(cli_context_t *ctx, char c)
{
    if (ctx == NULL)
    {
        return -1;
    }

    cli_private_t *priv = get_priv(ctx);

    switch (priv->state)
    {
    case PARSE_STATE_NOMAL:
        if (ESC == c)
        {
            priv->state = PARSE_STATE_ESC_WAIT1;
        }
        else
        {
            cli_char_proc(priv, c);
        }
        break;

    case PARSE_STATE_ESC_WAIT1:
        if ('[' == c)
        {
            priv->state = PARSE_STATE_ESC_WAIT2;
        }
        else
        {
            priv->state = PARSE_STATE_NOMAL;
        }
        break;

    case PARSE_STATE_ESC_WAIT2:
        cli_esc_proc(priv, c);

        priv->state = 0;
        break;

    default:
        priv->state = PARSE_STATE_NOMAL;
        break;
    }

    return 0;
}

static void cli_char_proc(cli_private_t *priv, char c)
{
    if ((SPC <= c) && (c <= 0x7e))
    {
        cli_event_send(priv, 1, c);
    }
    else if (BS == c)
    {
        cli_event_send(priv, 2, '\0');
    }
    else if ((LF == c) || (CR == c))
    {
        cli_event_send(priv, 3, '\0');
    }
    else
    {
        ;
    }
}

static void cli_esc_proc(cli_private_t *priv, char c)
{
    if ('A' == c)
    {
        cli_event_send(priv, 4, '\0');
    }
    else if ('B' == c)
    {
        cli_event_send(priv, 5, '\0');
    }
    else if ('C' == c)
    {
        cli_event_send(priv, 6, '\0');
    }
    else if ('D' == c)
    {
        cli_event_send(priv, 7, '\0');
    }
    else
    {
        /* DO NOTHING */
    }
}

static void cli_event_send(cli_private_t *priv, uint16_t event, uint32_t parm)
{
    if (event != 0)
    {
        cli_event_handler(priv, event, parm);
    }
}

static void cli_event_handler(cli_private_t *priv, uint16_t event, uint32_t parm)
{
    if (event != 0)
    {
        if (event == 1)
        {
            char c = (char)parm;
            /* CHARACTER */
            cli_text_add_char(priv, c);
        }
        else if (event == 2)
        {
            /* BACKSPACE */
            cli_text_delete_char(priv);
        }
        else if (event == 3)
        {
            cli_enter(priv);
        }
        else if (event ==4)
        {
            const char *text = cli_text_get_history_prev(priv);
            if (text)
            {
                /* 現在のテキストは消去 */
                cli_text_delete_text(priv);

                /* 履歴から取得したテキストを、現在行のテキストとして追加する */
                cli_text_add_text(priv, text);
            }
        }
        else if (event == 5)
        {
            const char *text = cli_text_get_history_next(priv);
            if (text)
            {
                /* 現在のテキストは消去 */
                cli_text_delete_text(priv);

                /* 履歴から取得したテキストを、現在行のテキストとして追加する */
                cli_text_add_text(priv, text);
            }
        }
        else if (event == 6)
        {
            /* カーソル右移動 */
            cli_text_cursor_right(priv);
        }
        else if (event == 7)
        {
            /* カーソル左移動 */
            cli_text_cursor_left(priv);
        }

        size_t p_len = strlen(priv->prompt);
        size_t c_len = cli_text_get_cursor_pos(priv);

        char buf[8];
        uint16_t pos = p_len + c_len;

        char *text = (char *)cli_text_get_current_line(priv);

        buf[0] = '\r';
        buf[1] = '\e';
        buf[2] = '[';

        if (pos > 99)
        {
            buf[3] = ((pos / 100) % 10) + '0';
            buf[4] = ((pos /  10) % 10) + '0';
            buf[5] = ((pos /   1) % 10) + '0';
            buf[6] = 'C';
            buf[7] = '\0';
        }
        else if (pos > 9)
        {
            buf[3] = ((pos /  10) % 10) + '0';
            buf[4] = ((pos /   1) % 10) + '0';
            buf[5] = 'C';
            buf[6] = '\0';
        }
        else
        {
            buf[3] = ((pos /   1) % 10) + '0';
            buf[4] = 'C';
            buf[5] = '\0';
        }

        output_string(priv, "\033[2K\r");                   /* テキスト行を行ごと削除 */
        output_string(priv, priv->prompt);                  /* プロンプト */
        output_string(priv, text);                          /* テキスト */
        output_string(priv, buf);                           /* カーソル位置 */
    }
}

static void cli_enter(cli_private_t *priv)
{
    char *text = (char *)cli_text_get_current_line(priv);
    size_t text_len = strlen((const char *)text);

    if (0 < text_len)
    {
        /* 現在のテキストを履歴用バッファに保存 */
        cli_text_storage_text(priv, (const char *)text);

        /* テキストをトークン化 */
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
 * @brief コマンドラインをスペース区切りでトークンに分割
 * @param ctx CLIの状態データを保持するメモリ領域
 * @return 引数の数 (0以上)
 * @note 引数の最大数はCLI_ARGV_SIZEで定義されている値まで。引数の最大数を超える場合は切り詰める。
 *       引数の最大数に達していない場合は、argvの最後をNULLにする。
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
int cli_command_register(cli_context_t *ctx, const cli_command_t *list, uint16_t size)
{
    if (ctx == NULL) return -1;
    if (list == NULL) return -1;
    if (size == 0) return -1;

    cli_private_t *priv = get_priv(ctx);

    cli_set_command_list(priv, list);
    cli_set_command_list_size(priv, size);

    return 0;
}

/**
 * @brief コマンド登録解除
 * @param ctx  制御データ(context)のポインタ
 * @return 処理結果
 */
int cli_command_unregister(cli_context_t *ctx)
{
    if (ctx == NULL) return -1;

    cli_private_t *priv = get_priv(ctx);

    cli_set_command_list(priv, NULL);
    cli_set_command_list_size(priv, 0);

    return 0;
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