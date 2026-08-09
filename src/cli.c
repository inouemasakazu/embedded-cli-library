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

#define CLI_CMD_NULL       NULL

/****************************************************************************************************
 * Private typedef
 ****************************************************************************************************/
/**
 * @brief 入力種別
 */
typedef enum
{
    CLI_KIND_NONE = 0,
    CLI_KIND_CL_EDITOR,
    CLI_KIND_CMD_EXECUTE,
    CLI_KIND_MAX
} cli_kind_e;

/****************************************************************************************************
 * Private Variables
 ****************************************************************************************************/

/****************************************************************************************************
 * Private Functions
 ****************************************************************************************************/
static int cli_workspace_memory_layout(cli_private_t *priv, const cli_config_t *cfg, void *workspace, size_t size);

/**
 * @brief 入力種別チェック
 */
static cli_kind_e cli_kind_check(char c);

/**
 * @brief コマンド実行処理
 */
static void cli_cmd_execute(cli_private_t *priv);

static void cli_tokenizer(cli_private_t *priv);
static int cli_dispatch(cli_private_t *priv);

static int cli_cmd_find(cli_private_t *priv, const char *name, bool is_used);

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
    int success = 0;

    if (ctx == NULL)
    {
        success = -1;
    }
    else
    {
        cli_private_t *priv = get_priv(ctx);

        cli_kind_e kind = cli_kind_check(c);

        switch (kind)
        {
        case CLI_KIND_CL_EDITOR:
            /* Command Lineの編集 */
            cli_editor(priv, c);
            break;

        case CLI_KIND_CMD_EXECUTE:
            /* コマンド実行 */
            cli_cmd_execute(priv);
            break;

        case CLI_KIND_NONE:
        default:
            /* DO NOTHING */
            break;
        }
    }

    return success;
}

/**
 * @brief 入力種別チェック
 * @param c 入力文字
 * @return 入力種別
 */
static cli_kind_e cli_kind_check(char c)
{
    cli_kind_e kind = CLI_KIND_NONE;

    /* 入力文字から処理種別を確定 */
    if ((LF == c) || (CR == c))
    {
        /* Command Lineの確定 */
        /* 入力済みのCommand Lineを確定しコマンド実行処理を行う */
        kind = CLI_KIND_CMD_EXECUTE;
    }
    else if ((SPC <= c) && (c <= 0x7e)) {
        /* 図形文字 */
        /* Command Lineの編集 */
        kind = CLI_KIND_CL_EDITOR;
    }
    else if (ESC  == c)
    {
        /* ESCシーケンスの開始文字 */
        /* Command Lineの編集 */
        kind = CLI_KIND_CL_EDITOR;
    }
    else if (BS  == c)
    {
        /* backスペース文字 */
        /* Command Lineの編集 */
        kind = CLI_KIND_CL_EDITOR;
    }
    else
    {
        /* DO NOTHING */
    }

    return kind;
}

/**
 * @brief コマンド実行処理
 */
static void cli_cmd_execute(cli_private_t *priv)
{
    CLI_ASSERT(ctx != NULL);

    /* Command Lineをトークンに分割 */
    cli_tokenizer(priv);

    if (0 < priv->argument.count)
    {
        cli_context_t *ctx = get_public(priv);

        /* コマンドのディスパッチ */
        int success = cli_dispatch(priv);

        /* コマンドの実行結果に応じてメッセージを描画 */
        if (success == 1)
        {
            /* 該当コマンドなし */
            cli_printf(ctx, "\r\nError: '%s' command not found\r\n", priv->argument.vector[0]);
        }
        else if (success == -1)
        {
            /* 実行コマンドのエラー */
            cli_printf(ctx, "\r\nError: command execution failed\r\n");
        }
        else
        {
            /* DO NOTHING */
        }
    }
    else
    {
        /* DO NOTHING */
    }

    /* 改行 */
    cli_textline_break(priv);
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
 * @name  cli_dispatch
 * @brief コマンドのディスパッチ
 * @param ctx CLIコンテキスト
 * @return 異常(-1) / コマンドなし(1) / 成功(0)
 * @note コマンドなしは、コマンドテーブルにコマンドが登録されていない場合を示す。
 *       コマンドなしとコマンド実行エラーは異なる値で返す。
 */
static int cli_dispatch(cli_private_t *priv)
{
    int result = -1;

    CLI_ASSERT(ctx != NULL);

    if (priv->argument.count <= 0)
    {
        result = 2;   /* トークンなし */
    }
    else
    {
        /* コマンドを探す */
        int index = cli_cmd_find(priv, priv->argument.vector[0], true);
        if (index >= 0)
        {
            /* コマンドあり */
            if (priv->cmd[index].handler != CLI_CMD_NULL)
            {
                /* コマンド関数がNULLでない場合は、コマンド実行 */
                result = priv->cmd[index].handler(priv->argument.count, priv->argument.vector);
            }
            else
            {
                result = -1;  /* コマンド関数未登録 */
            }
        }
        else
        {
            result = 1;   /* コマンドなし */
        }
    }

    return result;
}

/**
 * @brief コマンド登録
 * @param name    登録コマンドの名称
 * @param handler コマンド実行時に呼び出されるハンドラ
 * @return 処理結果
 */
int cli_cmd_register(cli_context_t *ctx, const char *name, cmd_handler_t handler)
{
    int result = -1;

    if ((ctx == NULL) || (name == NULL) || (handler == NULL)) return result;

    cli_private_t *priv = get_priv(ctx);

    int index = cli_cmd_find(priv, name, true);

    if (index >= 0)
    {
        /* 同名のコマンドが登録されている場合は、登録失敗 */
        result = 1;
    }
    else
    {
        /* 空きを探す */
        index = cli_cmd_find(priv, name, false);
        if (index >= 0)
        {
            /* コマンド登録 */
            snprintf(&priv->cmd[index].name[0], sizeof(priv->cmd[index].name), "%s", name);
            priv->cmd[index].handler = handler;
            priv->cmd[index].is_used = true;
    
            result = 0;     /* 登録成功 */
        }
        else
        {
            result = 1;     /* 空きなし */
        }
    }

    return result;
}

/**
 * @brief コマンド登録解除
 * @param name 登録解除するコマンドの名称
 * @return 処理結果
 */
int cli_cmd_unregister(cli_context_t *ctx, const char *name)
{
    int result = -1;

    if ((ctx == NULL) || (name == NULL)) return result;

    cli_private_t *priv = get_priv(ctx);

    int index = cli_cmd_find(priv, name, true);

    if (index >= 0)
    {
        /* コマンド削除 */
        memset(&priv->cmd[index].name[0], '\0', sizeof(priv->cmd[index].name));
        priv->cmd[index].handler = CLI_CMD_NULL;
        priv->cmd[index].is_used = false;

        result = 0;
    }
    else
    {
        result = 1;
    }

    return result;
}

/**
 * @brief cmd検索
 * @param ctx CLIコンテキスト
 * @param name コマンド名
 * @param is_used 登録されているコマンドを探す場合はtrue、空きを探す場合はfalse
 * @return コマンドテーブルのインデックス(0～) / 見つからなかった(-1)
 */
static int cli_cmd_find(cli_private_t *priv, const char *name, bool is_used)
{
    int index = -1;

    size_t i;
    bool is_find = false;

    /* 同名のコマンドを探す */
    for (i = 0; i < CLI_CMD_ENTRY_MAX; i++)
    {
        if (is_used == true)
        {
            /* 登録されているコマンドを探す場合は、同名のコマンドが登録されているかを探す */
            if ((priv->cmd[i].is_used == true) && (strcmp(name, priv->cmd[i].name) == 0))
            {
                /* コマンドあり */
                is_find = true;
                break;
            }
        }
        else
        {
            /* 空きを探す場合は、同名のコマンドが登録されていないかを探す */
            if ((priv->cmd[i].is_used == false) && (strcmp(name, priv->cmd[i].name) != 0))
            {
                /* 空きあり */
                is_find = true;
                break;
            }
        }
    }

    if (is_find == true)
    {
        /* 一致するコマンドあり */
        index = i;
    }

    return index;
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