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
#include "../inc/cli_io.h"

#include "cli_private.h"

#include "cli_editor.h"

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
static int cli_command_execute(cli_private_t *priv);
static void cli_tokenizer(cli_private_t *priv);


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
        cli_set_prompt(ctx, cfg->prompt);
        cli_set_stdout_cb(ctx, cfg->io_write_cb);

        if (cfg->command_list != NULL)
        {
            priv->command_list = cfg->command_list;
            priv->list_size = cfg->list_size;
        }
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
    total = total + cfg->max_output_size;
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

    priv->text.line = workspace;
    priv->text.max_size = cfg->max_line_size;

    offset = cfg->max_line_size;

    priv->output.buf = (workspace + offset);
    priv->output.max_size = cfg->max_output_size;

    offset = offset + cfg->max_output_size;

    priv->argument.vector = (char **)(workspace + offset);
    priv->argument.max_size = (cfg->depth_argv * sizeof(char *));

    offset = offset + priv->argument.max_size;

    priv->history.buffer = (workspace + offset);
    priv->history.max_size = (priv->text.max_size * cfg->depth_history);

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
            cli_printf(ctx, "\r\n%s", message);
        }

        /* プロンプトを表示 */
        cli_printf(ctx, "\r\n%s", priv->prompt);
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
    cli_context_t *ctx = get_public(priv);

    if (event != 0)
    {
        if (event == 1)
        {
            char c = (char)parm;
            /* CHARACTER */
            cli_textline_add_char(priv, c);
        }
        else if (event == 2)
        {
            /* BACKSPACE */
            cli_textline_delete_char(priv);
        }
        else if (event == 3)
        {
            size_t text_len = strlen((const char *)priv->text.line);

            if (0 < text_len)
            {
                /* 現在のテキストを履歴用バッファに保存 */
                cli_textline_add_history(priv, (const char *)priv->text.line);
            }

            cli_enter(priv);
        }
        else if (event ==4)
        {
            const char *text = cli_textline_get_history_prev(priv);
            if (text)
            {
                /* 現在のテキスト行は消去 */
                cli_textline_delete_text(priv);

                char *p = (char *)priv->text.line;

                size_t len = strlen(text);

                for (size_t i = 0; i < (len + 1); i++)
                {
                    *(p + i) = *(text + i);
                }

                cli_textline_set_cursor_pos(priv, len);
            }
        }
        else if (event == 5)
        {
            const char *text = cli_textline_get_history_next(priv);
            if (text)
            {
                /* 現在のテキスト行は消去 */
                cli_textline_delete_text(priv);

                char *p = (char *)priv->text.line;

                size_t len = strlen(text);

                for (size_t i = 0; i < (len + 1); i++)
                {
                    *(p + i) = *(text + i);
                }

                cli_textline_set_cursor_pos(priv, len);
            }
        }
        else if (event == 6)
        {
            /* カーソル右移動 */
            cli_textline_cursor_right(priv);
        }
        else if (event == 7)
        {
            /* カーソル左移動 */
            cli_textline_cursor_left(priv);
        }

        size_t p_len = strlen(priv->prompt);

#if 0
        /* text line draw refresh. */
        cli_printf(ctx, "\033[2K");                                 /* テキスト行を行ごと削除 */
        cli_printf(ctx, "\r");                                      /* 復帰(左寄せ) */
        cli_printf(ctx, "%s", priv->prompt);                        /* プロンプト表示 */
        cli_printf(ctx, "%s", priv->text.line);                     /* テキスト行表示 */
        cli_printf(ctx, "\r");                                      /* 復帰(左寄せ) */
        cli_printf(ctx, "\e[%dC", priv->text.cursor + p_len);       /* 現在のカーソル位置に移動 */
#else
        cli_printf(ctx, "\033[2K\r%s", priv->prompt);                                 /* テキスト行を行ごと削除 */
        cli_printf(ctx, "%s", priv->text.line);                     /* テキスト行表示 */
        cli_printf(ctx, "\r\e[%dC", priv->text.cursor + p_len);       /* 現在のカーソル位置に移動 */
#endif
    }
}

static void cli_enter(cli_private_t *priv)
{
    size_t text_len = strlen((const char *)priv->text.line);

    cli_context_t *ctx = get_public(priv);

    if (0 < text_len)
    {
        /* テキストをトークン化 */
        cli_tokenizer(priv);

        if (0 < priv->argument.count)
        {
            /* コマンド実行 */
            int success = cli_command_execute(priv);
            if (success == 1)
            {
                /* コマンドがない */
                cli_printf(ctx, "\r\nError: '%s' command not found\r\n", priv->argument.vector[0]);
            }
            else if (success == -1)
            {
                /* コマンドの実行エラー */
                cli_printf(ctx, "\r\nError: command execution failed\r\n");
            }
            else
            {
                /* DO NOTHING */
            }
        }
    }

    /* 改行 */
    cli_textline_break(priv);
    cli_printf(ctx, "\r\n");
}

/**
 * @brief コマンド実行
 *        コマンドライン引数が保持するコマンド名と、コマンドリストに一致するコマンド名の探索を行う。
 *        一致するコマンド名が存在する場合はコマンド実行の要求ありのため、handlerを起動する。
 * @param priv 
 * @return 実行結果
 */

static int cli_command_execute(cli_private_t *priv)
{
    const cli_command_t *list = priv->command_list;

    int argc    = priv->argument.count;
    char **argv = priv->argument.vector;

    if ((list == NULL) || (argv == NULL)) return -1;
    if (argc == 0) return -2;

    for (size_t i = 0; i < priv->list_size; i++)
    {
        if (strcmp(list[i].name, argv[0]) == 0)
        {
            if (list[i].handler)
            {
                return list[i].handler(argc, argv);
            }
        }
    }

    return 1;
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
    char *token = (char *)priv->text.line;
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


/**
 * @brief プロンプト設定
 * @param prompt プロンプトとして表示する文字列を示すメモリ領域
 * @return 処理結果
 */
int cli_set_prompt(cli_context_t *ctx, const char *prompt)
{
    int success = 0;

    if (ctx == NULL)
    {
        success = -1;
    }
    else
    {
        cli_private_t *priv = get_priv(ctx);

        /* プロンプトの設定内容を更新 */
        if (prompt == NULL)
        {
            /* デフォルトのプロンプト('>')を設定 */
            priv->prompt = ">";
        }
        else
        {
            /* 任意のプロンプトを設定 */
            priv->prompt = prompt;
        }
    }

    return success;
}

/**
 * @brief CLI用データ書き込みCB処理の登録
 *        CLIモジュール内で使用する出力処理のコールバックを設定する。
 * @return 処理結果
 */
int cli_set_stdout_cb(cli_context_t *ctx, io_write_cb_t write_cb)
{
    int success = 0;

    if (ctx == NULL)
    {
        success = -1;
    }
    else
    {
        cli_private_t *priv = get_priv(ctx);

        /* CB登録 */
        priv->io_write_cb = write_cb;
    }

    return success;
}