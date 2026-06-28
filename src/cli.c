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
#include "../inc/cli_stdio.h"

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

/*** ASCiiコード(制御文字) ***/
#define NUL                 '\0'    /* null文字 */
#define BS                  '\b'    /* 後退 */
#define HT                  '\t'    /* 水平タブ */
#define LF                  '\n'    /* 改行 */
#define CR                  '\r'    /* 復帰 */
#define ESC                 '\e'    /* エスケープ */
#define DEL                 0x7f    /* 削除 */

/*** ASCiiコード(図形文字) ***/
#define SPC                 ' '     /* 空白文字 */

/*** ESCシーケンス(未使用) ***/
#define ESC_SEQ_UP          "\e[A"  /* 上矢印 */
#define ESC_SEQ_DOWN        "\e[B"  /* 下矢印 */
#define ESC_SEQ_RIGHT       "\e[C"  /* 右矢印 */
#define ESC_SEQ_LEFT        "\e[D"  /* 左矢印 */

/****************************************************************************************************
 * Private typedef
 ****************************************************************************************************/
typedef enum
{
    /*** 入力タイプ ***/
    CLI_INPUT_NONE,
    CLI_INPUT_TEXT,
    CLI_INPUT_ENTER,
    CLI_INPUT_BACKSPACE,
    CLI_INPUT_ESCAPE,
    CLI_INPUT_DELETE,
    CLI_INPUT_MAX
} cli_input_type_t;

/****************************************************************************************************
 * Private Variables
 ****************************************************************************************************/

/****************************************************************************************************
 * Private Functions
 ****************************************************************************************************/

static cli_input_type_t cli_check_input_type(char c);

static int cli_line_editor(cli_context_t *ctx, char c);

static int cli_execute_cmd(cli_context_t *ctx);
static int cli_tokenizer(cli_context_t *ctx);
static int cli_dispatch(cli_context_t *ctx);

static int cli_cmd_find(cli_context_t *ctx, const char *name, bool is_used);

/**
 * @brief CLI初期化
 * @param ctx CLIの状態データを保持するメモリ領域
 * @return 処理結果
 */
int cli_init(cli_context_t *ctx)
{
    int success = 0;

    if (ctx == NULL)
    {
        success = -1;
    }
    else
    {
        /* メモリ0クリア */
        memset(ctx, 0, sizeof(cli_context_t));

        /* デフォルトのプロンプト('>')を設定 */
        ctx->prompt = ">";
    }

    return success;
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
        if ((message != NULL) || (*message != '\0'))
        {
            /* 開始を通知するメッセージを表示 */
            cli_printf(ctx, "\r\n%s", message);
        }

        /* プロンプトを表示 */
        cli_printf(ctx, "\r\n%s", ctx->prompt);
    }

    return success;
}

/**
 * @brief CLI入力処理
 *        CLIに入力する文字の整形と解釈を行う。
 * @param c CLIに入力する文字
 * @return 処理結果
 */
int cli_input_char(cli_context_t *ctx, char c)
{
    if (ctx == NULL) return -1;

    /* 文字データから入力タイプを取得 */
    cli_input_type_t type = cli_check_input_type(c);

    switch (type)
    {
    case CLI_INPUT_TEXT:
    case CLI_INPUT_BACKSPACE:
        /* コマンドラインエディタ */
        cli_line_editor(ctx, c);
        break;

    case CLI_INPUT_ENTER:
        /* トークンに分割 */
        if (0 < cli_tokenizer(ctx))
        {
            /* コマンド実行 */
            cli_execute_cmd(ctx);
        }

        /* 改行復帰入力時はプロンプトを出力する */
        cli_printf(ctx, "\r\n%s", ctx->prompt);

        /* コマンドライン用バッファ初期化 */
        ctx->current_line.buf[0] = '\0';
        ctx->current_line.size = 0;
        break;

    case CLI_INPUT_ESCAPE:
        /* DO NOTHING */
        break;

    default:
        /* DO NOTHING */
        break;
    }

    return 0;
}

/**
 * @name  cli_check_input_type
 * @brief 入力タイプの判定
 * @param c 入力文字
 * @return 入力タイプ
 */
static cli_input_type_t cli_check_input_type(char c)
{
    cli_input_type_t type = CLI_INPUT_NONE;

    if ((SPC <= c) && (c <= 0x7e))
    {
        /* 図形文字 */
        type = CLI_INPUT_TEXT;
    }
    else if ((c == LF) || (c == CR))
    {
        /* 改行 or 復帰 */
        type = CLI_INPUT_ENTER;
    }
    else if (c == BS)
    {
        /* バックスペース */
        type = CLI_INPUT_BACKSPACE;
    }
    else if (c == ESC)
    {
        /* エスケープシーケンスは未実装 */
    }
    else
    {
        /* その他の制御文字等 */
    }

    return type;
}

/**
 * @brief CLIラインエディタ
 *        文字データの内容に合わせて、バッファの編集を行う。
 * @param ctx CLIの状態データを保持するメモリ領域
 * @param c 入力文字
 * @return 引数異常(-1) / 成功(0)
 */
static int cli_line_editor(cli_context_t *ctx, char c)
{
    CLI_ASSERT(ctx != NULL);

    if ((SPC <= c) && (c <= 0x7e))
    {
        /* 図形文字(空白含む) */
        if (ctx->current_line.size < (CLI_LINE_SIZE - 1))
        {
            ctx->current_line.buf[ctx->current_line.size++] = c;
            ctx->current_line.buf[ctx->current_line.size  ] = '\0';         /* バッファ終端にNULL文字を挿入 */
        }

        cli_printf(ctx, "%c", c);        /* バッファフローしていてもエコーバックは行う */
    }
    else if (c == BS)
    {
        /* バックスペース */
        if (ctx->current_line.size > 0)
        {
            cli_printf(ctx, "%c", BS );
            cli_printf(ctx, "%c", SPC);
            cli_printf(ctx, "%c", BS );

            ctx->current_line.size--;
            ctx->current_line.buf[ctx->current_line.size] = '\0';
        }
    }
    else
    {
        /* 図形文字以外の処理はなし */
    }

    return 0;
}

/**
 * @brief コマンド実行
 * @param ctx CLIの状態データを保持するメモリ領域
 * @return 処理結果
 */
static int cli_execute_cmd(cli_context_t *ctx)
{
    CLI_ASSERT(ctx != NULL);

    /* コマンドのディスパッチ */
    int success = cli_dispatch(ctx);

    /* コマンドの結果に応じたメッセージを表示 */
    if (success == 1)
    {
        /* 該当コマンドなし */
        cli_printf(ctx, "\r\nError: '%s' command not found\r\n", ctx->args.argv[0]);
    }
    else if (success == -1)
    {
        /* 実行コマンドのエラー */
        cli_printf(ctx, "\r\nError: command execution failed\r\n");
    }
    else
    {
        /* コマンド実行成功 */
    }

    return 0;
}

/**
 * @brief コマンドラインをスペース区切りでトークンに分割
 * @param ctx CLIの状態データを保持するメモリ領域
 * @return 引数の数 (0以上)
 * @note 引数の最大数はCLI_ARGV_SIZEで定義されている値まで。引数の最大数を超える場合は切り詰める。
 *       引数の最大数に達していない場合は、argvの最後をNULLにする。
 */
static int cli_tokenizer(cli_context_t *ctx)
{
    CLI_ASSERT(ctx != NULL);

    char *token = ctx->current_line.buf;

    ctx->args.argc = 0;
    memset(ctx->args.argv, '\0', sizeof(ctx->args.argv));

    /* コマンドラインをスペース区切りでトークンに分割 */
    while ((*token != '\0') && (ctx->args.argc < CLI_ARGV_SIZE))
    {
        while (*token == ' ') token++;

        ctx->args.argv[ctx->args.argc] = token;
        ctx->args.argc++;

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

    if (ctx->args.argc < CLI_ARGV_SIZE)
    {
        /* 引数の最大数に達していない場合は、argvの最後をNULLにする */
        ctx->args.argv[ctx->args.argc] = NULL;
    }

    return ctx->args.argc;
}

/**
 * @name  cli_dispatch
 * @brief コマンドのディスパッチ
 * @param ctx CLIコンテキスト
 * @return 異常(-1) / コマンドなし(1) / 成功(0)
 * @note コマンドなしは、コマンドテーブルにコマンドが登録されていない場合を示す。
 *       コマンドなしとコマンド実行エラーは異なる値で返す。
 */
static int cli_dispatch(cli_context_t *ctx)
{
    int result = -1;

    CLI_ASSERT(ctx != NULL);

    if (ctx->args.argc <= 0)
    {
        result = 2;   /* トークンなし */
    }
    else
    {
        /* コマンドを探す */
        int index = cli_cmd_find(ctx, ctx->args.argv[0], true);
        if (index >= 0)
        {
            /* コマンドあり */
            if (ctx->cmd[index].func != CLI_CMD_NULL)
            {
                /* コマンド関数がNULLでない場合は、コマンド実行 */
                result = ctx->cmd[index].func(ctx->args.argc, ctx->args.argv);
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
 * @param ctx CLIコンテキスト
 * @param name コマンド名
 * @param func コマンド関数
 * @return 引数異常(-1) / 成功(0) / 空きなし(1)
 */
int cli_cmd_register(cli_context_t *ctx, const char *name, cli_func_t func)
{
    int result = -1;

    if ((ctx == NULL) || (name == NULL) || (func == NULL)) return result;

    int index = cli_cmd_find(ctx, name, true);

    if (index >= 0)
    {
        /* 同名のコマンドが登録されている場合は、登録失敗 */
        result = 1;
    }
    else
    {
        /* 空きを探す */
        index = cli_cmd_find(ctx, name, false);
        if (index >= 0)
        {
            /* コマンド登録 */
            snprintf(&ctx->cmd[index].name[0], sizeof(ctx->cmd[index].name), "%s", name);
            ctx->cmd[index].func    = func;
            ctx->cmd[index].is_used = true;
    
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
 * @brief コマンド削除
 * @param ctx CLIコンテキスト
 * @param name コマンド名
 * @return 引数異常(-1) / 成功(0) / 見つからなかった(1)
 */
int cli_cmd_unregister(cli_context_t *ctx, const char *name)
{
    int result = -1;

    if ((ctx == NULL) || (name == NULL)) return result;
    
    int index = cli_cmd_find(ctx, name, true);

    if (index >= 0)
    {
        /* コマンド削除 */
        memset(&ctx->cmd[index].name[0], '\0', sizeof(ctx->cmd[index].name));
        ctx->cmd[index].func    = CLI_CMD_NULL;
        ctx->cmd[index].is_used = false;

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
static int cli_cmd_find(cli_context_t *ctx, const char *name, bool is_used)
{
    int index = -1;

    size_t i;
    bool is_find = false;

    /* 同名のコマンドを探す */
    for (i = 0; i < CLI_CMD_SIZE; i++)
    {
        if (is_used == true)
        {
            /* 登録されているコマンドを探す場合は、同名のコマンドが登録されているかを探す */
            if ((ctx->cmd[i].is_used == true) && (strcmp(name, ctx->cmd[i].name) == 0))
            {
                /* コマンドあり */
                is_find = true;
                break;
            }
        }
        else
        {
            /* 空きを探す場合は、同名のコマンドが登録されていないかを探す */
            if ((ctx->cmd[i].is_used == false) && (strcmp(name, ctx->cmd[i].name) != 0))
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
        /* プロンプトの設定内容を更新 */
        if (prompt == NULL)
        {
            /* デフォルトのプロンプト('>')を設定 */
            ctx->prompt = ">";
        }
        else
        {
            /* 任意のプロンプトを設定 */
            ctx->prompt = prompt;
        }
    }

    return success;
}

/**
 * @brief CLI用標準出力のコールバック設定
 *        CLIモジュール内で使用する出力処理のコールバックを設定する。
 * @return 処理結果
 */
int cli_set_stdout_cb(cli_context_t *ctx, stdout_cb_t stdout_cb)
{
    int success = 0;

    if (ctx == NULL)
    {
        success = -1;
    }
    else
    {
        /* CB登録 */
        ctx->stdout_cb = stdout_cb;
    }

    return success;
}