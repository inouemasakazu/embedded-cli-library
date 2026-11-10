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

/****************************************************************************************************
 * Private MACROS
 ****************************************************************************************************/

 /**
  * @brief 型定義のサイズ保証用マクロ
  * @attention
  * 外部公開サイズ(cli_context_t) >= 内部サイズ(cli_private_t) を保証します。
  * 条件を満たさない場合はコンパイルエラーになります。
  */
CLI_STATIC_ASSERT(sizeof(cli_context_t) >= sizeof(cli_private_t), ctx_size_is_too_small);


/**
 * @brief アライメント調整マクロ
 * @attention
 * メモリのアライメント調整を目的としたマクロです。
 * `size`にはメモリ、`align`には2の累乗（2, 4, 8, 16...）を設定してください。
 */
#define CLI_ALIGN_UP(size, align) (((size) + ((align) - 1)) & ~((align) - 1))

/****************************************************************************************************
 * Private typedef
 ****************************************************************************************************/

typedef enum
{
    CLI_EVENT_OK = 0,
    CLI_EVENT_RUNNING,
    CLI_EVENT_ERROR
} cli_event_code_t;

typedef cli_event_code_t (* cli_event_handler_t)(cli_private_t *priv, cli_event_t ev);

/****************************************************************************************************
 * Private Variables
 ****************************************************************************************************/

/****************************************************************************************************
 * Private Functions
 ****************************************************************************************************/

static void cli_input_analysis( cli_private_t *priv, char c );

static void cli_emit_event( cli_private_t *priv, bool is_control, char command );

static cli_event_code_t cli_event_none(cli_private_t * priv, cli_event_t ev);
static cli_event_code_t cli_event_text(cli_private_t * priv, cli_event_t ev);
static cli_event_code_t cli_event_cursor_move(cli_private_t * priv, cli_event_t ev);
static cli_event_code_t cli_event_backspace(cli_private_t * priv, cli_event_t ev);
static cli_event_code_t cli_event_enter(cli_private_t * priv, cli_event_t ev);
static cli_event_code_t cli_event_input_interrupt(cli_private_t * priv, cli_event_t ev);

static void cli_render_process(cli_private_t * priv);
static void cli_render_required(cli_private_t * priv, cli_render_required_t required);
static void cli_render_commandline(cli_private_t * priv);

static void cli_tokenizer(cli_private_t *priv);

static cli_command_code_t cli_command_execute(cli_private_t * priv);
static const cli_command_t *cli_command_find(cli_private_t *priv, const char *name);

static cli_impl_code_t cli_fifo_push(cli_private_t * priv, uint8_t data);
static cli_impl_code_t cli_fifo_pull(cli_private_t * priv, uint8_t * data);
static uint16_t cli_fifo_is_items(cli_private_t * priv);


static void cli_set_prompt(cli_private_t *priv, const char *prompt);
static void cli_set_output(cli_private_t *priv, cli_output_t * output );
static void cli_set_command_list(cli_private_t *priv, const cli_command_t *list);
static void cli_set_command_list_size(cli_private_t *priv, uint16_t size);

static const cli_command_t *cli_get_command_list(cli_private_t *priv);
static uint16_t cli_get_command_list_size(cli_private_t *priv);

/**
 * @brief イベントハンドラの関数マトリクス
 *        状態×イベントの組み合わせによる関数マトリクス
 */
const static cli_event_handler_t cli_event_handler_matrix[CLI_SYSTEM_STATE_MAX][CLI_EVENT_MAX] =
{
    /* [STATE / EVENT TYPE]      [ CLI_EVENT_NONE, CLI_EVENT_TEXT, CLI_EVENT_CURSOR_MOVE, CLI_EVENT_BACKSPACE, CLI_EVENT_ENTER, CLI_EVENT_INPUT_INTERRUPT ] */
    [CLI_SYSTEM_STATE_IDLE   ] = { NULL,           cli_event_text, cli_event_cursor_move, cli_event_backspace, cli_event_enter, NULL                      },
    [CLI_SYSTEM_STATE_RUNNING] = { NULL,           NULL          , NULL                 , NULL               , NULL           , cli_event_input_interrupt }
};

/**
 * @brief CLI初期化
 * @param ctx       制御データ(context)のポインタ
 * @param cfg       設定データのポインタ
 * @param workspace workspaceのポインタ
 * @param size      workspaceのメモリサイズ
 * @return 処理結果
 */
cli_code_t cli_init(cli_context_t * ctx, const cli_config_t * cfg, void * workspace, size_t size)
{
    if (ctx == NULL) return CLI_ERROR_INVALID_INSTANCE;
    if ((cfg == NULL) || (workspace == NULL)) return CLI_ERROR_INVALID_ARGUMENT;

    if ((cfg->max_fifo_size == 0) || (cfg->max_line_size == 0) || (cfg->depth_argv == 0))
    {
        /* configパラメータエラー */
        return CLI_ERROR_CONFIG_PARM;
    }

    if (size < cli_get_workspace_size(cfg))
    {
        /* メモリサイズ不足 */
        return CLI_ERROR_MEMORY_SIZE;
    }

    cli_private_t *priv = get_priv(ctx);

    /* メモリ0クリア */
    memset(priv, 0, sizeof(*priv));

    /* 設定パラメータをコピー */
    priv->prompt = cfg->prompt;

    priv->fifo_size                  = cfg->max_fifo_size;
    priv->text.current_line_max_size = cfg->max_line_size;
    priv->text.history_depth         = cfg->depth_history;
    priv->argument.max_size          = cfg->depth_argv;

    /* ワークスペースのアドレスを整数型に変換して切り分け開始 */
    uintptr_t current_addr = (uintptr_t)workspace;

    current_addr  = CLI_ALIGN_UP(current_addr, sizeof(char *));
    priv->fifo    = (uint8_t *)current_addr;
    current_addr += cfg->max_fifo_size;

    priv->text.current_line = (uint8_t *)current_addr;
    current_addr += cfg->max_line_size;

    priv->text.history_buffer = (uint8_t *)current_addr;
    current_addr += (cfg->max_line_size * cfg->depth_history);

    current_addr  = CLI_ALIGN_UP(current_addr, sizeof(char *));
    priv->argument.vector = (char **)current_addr;
    current_addr += (cfg->depth_argv * sizeof(char **));

    /* メモリ0クリア */
    memset(priv->fifo,                0, cfg->max_fifo_size);
    memset(priv->text.current_line,   0, cfg->max_line_size);
    memset(priv->text.history_buffer, 0, (cfg->max_line_size * cfg->depth_history));
    memset(priv->argument.vector    , 0, (cfg->depth_argv * sizeof(char **)));

    cli_render_required(priv, CLI_RENDER_STARTUP);

    return CLI_OK;
}

/**
 * @brief 初期化解除
 * @param ctx       制御データ(context)のポインタ
 * @return 処理結果
 */
cli_code_t cli_deinit(cli_context_t *ctx)
{
    if (ctx == NULL) return CLI_ERROR_INVALID_INSTANCE;

    cli_private_t *priv = get_priv(ctx);

    /* メモリ0クリア */
    memset(priv, 0, sizeof(*priv));

    return CLI_OK;
}

/**
 * @brief メイン処理
 * @param ctx       制御データ(context)のポインタ
 * @return 処理結果
 */
cli_code_t cli_process(cli_context_t *ctx)
{
    if (ctx == NULL) return CLI_ERROR_INVALID_INSTANCE;

    cli_private_t *priv = get_priv(ctx);

    while ( cli_fifo_is_items( priv ) )
    {
        uint8_t c;

        if ( CLI_IMPL_OK == cli_fifo_pull( priv, &c ) )
        {
            cli_input_analysis( priv, ((char)c) );
        }

        cli_event_handler_t event_handler = NULL;
        cli_event_code_t    event_code    = CLI_EVENT_OK;

        event_handler = cli_event_handler_matrix[priv->system][priv->event.type];
        if (event_handler)
        {
            /* イベントハンドラ */
            event_code = event_handler(priv, priv->event);

            /* システム状態管理 */
            switch (priv->system)
            {
            case CLI_SYSTEM_STATE_IDLE:
                if (event_code == CLI_EVENT_RUNNING)
                {
                    priv->system = CLI_SYSTEM_STATE_RUNNING;
                }
                break;

            case CLI_SYSTEM_STATE_RUNNING:
                if (event_code == CLI_EVENT_OK)
                {
                    priv->system = CLI_SYSTEM_STATE_IDLE;
                }
                break;

            default:
                /* DO NOTHING */
                break;
            }
        }
    }

    /* レンダリング */
    cli_render_process(priv);

    return CLI_OK;
}

/**
 * @brief 文字データ送信
 * @param ctx       制御データ(context)のポインタ
 * @param c         入力文字（キャラクタ）
 * @return 処理結果
 */
cli_code_t cli_send_char(cli_context_t *ctx, char c)
{
    if (ctx == NULL) return CLI_ERROR_INVALID_INSTANCE;

    cli_private_t *priv = get_priv(ctx);

    if ( CLI_IMPL_ERROR_FIFO_FULL == cli_fifo_push(priv, (uint8_t)c) )
    {
        /* pushしたデータをprocessが処理しきれていないためBUSYを返す */
        return CLI_ERROR_SEND_CHAR_BUSY;
    }

    return CLI_OK;
}

/**
 * @brief workspaceの必要サイズ取得
 *        指定された設定パラメータからworkspaceに必要なメモリサイズを算出します。
 * @param cfg 設定パラメータのポインタ
 * @return 必要サイズ
 */
size_t cli_get_workspace_size(const cli_config_t * cfg)
{
    if ( (cfg == NULL)
    ||  ((cfg->max_fifo_size == 0) || (cfg->max_line_size == 0) || (cfg->depth_argv == 0)))
    {
        return 0;
    }

    size_t total_required_size = 0;

    /* 最初に要求される最大アライメント（ポインタの4byte）のパディングを足す */
    total_required_size += (sizeof(char *) - 1);

    total_required_size += cfg->max_fifo_size;
    total_required_size += cfg->max_line_size;
    total_required_size += cfg->max_line_size * cfg->depth_history;

    /* 4byteのメモリアライメントにするため切り上げておく */
    total_required_size  = CLI_ALIGN_UP(total_required_size, sizeof(char *));
    total_required_size += cfg->depth_argv * sizeof(char *);

    /* 最終的なメモリアライメントに調整 */
    total_required_size  = CLI_ALIGN_UP(total_required_size, 4);

    return total_required_size;
}


/**
 * @brief writeインターフェース登録
 * @param ctx  制御データ(context)のポインタ
 * @param output writeインターフェースのポインタ
 * @return 処理結果
 */
cli_code_t cli_output_register(cli_context_t * ctx, const cli_output_t * output)
{
    if (ctx == NULL) return CLI_ERROR_INVALID_INSTANCE;
    if (output == NULL) return CLI_ERROR_INVALID_ARGUMENT;

    cli_private_t *priv = get_priv(ctx);

    priv->output = output;

    return CLI_OK;
}


/********************
 * static functions
 ********************/

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
    priv->event.type    = CLI_EVENT_NONE;
    priv->event.parm[0] = 0;

    if ( is_control )
    {
        /* エスケープシーケンス・制御文字に対応するイベントを発行 */
        if ( ('A' == command) || ('B' == command) || ('C' == command) || ('D' == command) )
        {
            priv->event.type    = CLI_EVENT_CURSOR_MOVE;
            priv->event.parm[0] = command;
            // priv->event.parm[1] = '\r';
            // priv->event.parm[2] = '\n';
            // priv->event.parm[3] = '\0';

            // output_string(priv, "[DEBUG] CLI_EVENT_CURSOR_MOVE : ");
            // output_string(priv, (const char *)priv->event.parm);
        }
        else if ( (LF == command) || (CR == command) )
        {
            priv->event.type    = CLI_EVENT_ENTER;
            priv->event.parm[0] = '\0';

            // output_string(priv, "[DEBUG] CLI_EVENT_ENTER\r\n");
        }
        else if ( BS == command )
        {
            priv->event.type    = CLI_EVENT_BACKSPACE;
            priv->event.parm[0] = '\0';

            // output_string(priv, "[DEBUG] CLI_EVENT_BACKSPACE\r\n");
        }
        else if ( ETX == command )
        {
            priv->event.type   = CLI_EVENT_INPUT_INTERRUPT;

            // output_string(priv, "[DEBUG] CLI_EVENT_INPUT_INTERRUPT\r\n");
        }
        else
        {
            /* 該当するイベントがない */
            priv->event.type    = CLI_EVENT_NONE;
            priv->event.parm[0] = '\0';

            // output_string(priv, "[DEBUG] CLI_EVENT_NONE\r\n");
        }
    }
    else
    {
        /* テキストデータのイベントを発行 */
        priv->event.type    = CLI_EVENT_TEXT;
        priv->event.parm[0] = command;
        // priv->event.parm[1] = '\r';
        // priv->event.parm[2] = '\n';
        // priv->event.parm[3] = '\0';

        // output_string(priv, "[DEBUG] CLI_EVENT_TEXT : ");
        // output_string(priv, (const char *)priv->event.parm);
    }
}

/**
 * @brief イベント処理「CLI_EVENT_NONE」
 */
static cli_event_code_t cli_event_none(cli_private_t * priv, cli_event_t ev)
{
    /* DO NOTHING */
    return CLI_EVENT_OK;
}

/**
 * @brief イベント処理「CLI_EVENT_TEXT」
 */
static cli_event_code_t cli_event_text(cli_private_t * priv, cli_event_t ev)
{
    /* コマンドラインにキャラクタを追加 */
    cli_text_add_char( priv, ev.parm[0] );

    cli_render_required(priv, CLI_RENDER_COMMANDLINE);

    return CLI_EVENT_OK;
}

/**
 * @brief イベント処理「CLI_EVENT_CURSOR_MOVE」
 */
static cli_event_code_t cli_event_cursor_move(cli_private_t * priv, cli_event_t ev)
{
    const char * text = NULL;

    switch ( ev.parm[0] )
    {
    case 'A':
    case 'B':
        /* 履歴の参照 */
        if ( 'A' == ev.parm[0] )
        {
            text = cli_text_get_history_prev(priv);
        }
        else
        {
            text = cli_text_get_history_next(priv);
        }

        if (text)
        {
            /* 現在のテキストは消去 */
            cli_text_delete_text(priv);

            /* 履歴から取得したテキストを、現在行のテキストとして追加する */
            cli_text_add_text(priv, text);
        }
        break;

    case 'C':
        /* カーソル右移動 */
        cli_text_cursor_right(priv);
        break;

    case 'D':
        /* カーソル左移動 */
        cli_text_cursor_left(priv);
        break;

    default:
        /* DO NOTHING */
        break;
    }

    cli_render_required(priv, CLI_RENDER_COMMANDLINE);

    return CLI_EVENT_OK;
}

/**
 * @brief イベント処理「CLI_EVENT_BACKSPACE」
 */
static cli_event_code_t cli_event_backspace(cli_private_t * priv, cli_event_t ev)
{
    cli_text_delete_char( priv );

    cli_render_required(priv, CLI_RENDER_COMMANDLINE);

    return CLI_EVENT_OK;
}

/**
 * @brief イベント処理「CLI_EVENT_ENTER」
 */
static cli_event_code_t cli_event_enter(cli_private_t * priv, cli_event_t ev)
{
    char *text = (char *)cli_text_get_current_line(priv);

    if ('\0' != *text)
    {
        /* コマンドラインを履歴バッファに送信 */
        cli_text_storage_text(priv, (const char *)text);

        /* トークン化 */
        cli_tokenizer(priv);

        /* コマンドハンドラを実行 */
        cli_command_code_t result = cli_command_execute(priv);

        /* コマンドライン削除 */
        cli_text_delete_text(priv);

        if (result == CLI_COMMAND_RUNNING)
        {
            return CLI_EVENT_RUNNING;
        }
    }

    cli_render_required(priv, CLI_RENDER_LINE_BREAK);

    return CLI_EVENT_OK;
}

/**
 * @brief イベント処理「CLI_EVENT_INPUT_INTERRUPT」
 */
static cli_event_code_t cli_event_input_interrupt(cli_private_t * priv, cli_event_t ev)
{
    if (priv->active_command)
    {
        if (CLI_COMMAND_COMPLETE != priv->active_command->callback())
        {
            return CLI_EVENT_RUNNING;
        }

        /* アクティブコマンドを削除 */
        priv->active_command = NULL;
    }

    cli_render_required(priv, CLI_RENDER_LINE_BREAK);

    return CLI_EVENT_OK;
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

static void cli_render_process(cli_private_t * priv)
{
    switch (priv->render_required)
    {
    case CLI_RENDER_NONE:
        /* DO NOTIHNG */
        break;

    case CLI_RENDER_STARTUP:
        /* TBD 表記内容は要検討（ライブラリ名称とバージョン表記は欲しい） */
        output_string(priv, "\r\nLightweight CLI library for embedded systems[v0.0.0]");
        output_string(priv, "\r\n");
        output_string(priv, priv->prompt);
        break;

    case CLI_RENDER_COMMANDLINE:
        cli_render_commandline(priv);
        break;

    case CLI_RENDER_LINE_BREAK:
        output_string(priv, "\r\n");
        output_string(priv, priv->prompt);
        break;

    default:
        break;
    }

    cli_render_required(priv, CLI_RENDER_NONE);
}

static void cli_render_required(cli_private_t * priv, cli_render_required_t required)
{
    priv->render_required = required;
}

static void cli_render_commandline( cli_private_t * priv )
{
    char buf[8];
    size_t p_len = strlen(priv->prompt);
    size_t c_len = cli_text_get_cursor_pos(priv);

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
    if (ctx == NULL) return CLI_ERROR_INVALID_INSTANCE;
    if ((list == NULL) || (size == 0)) return CLI_ERROR_INVALID_ARGUMENT;

    cli_private_t *priv = get_priv(ctx);

    cli_set_command_list(priv, list);
    cli_set_command_list_size(priv, size);

    return CLI_OK;
}

/**
 * @brief コマンド登録解除
 * @param ctx  制御データ(context)のポインタ
 * @return 処理結果
 */
cli_code_t cli_command_unregister(cli_context_t *ctx)
{
    if (ctx == NULL) return CLI_ERROR_INVALID_INSTANCE;

    cli_private_t *priv = get_priv(ctx);

    cli_set_command_list(priv, NULL);
    cli_set_command_list_size(priv, 0);

    return CLI_OK;
}

/**
 * @brief コマンドハンドラ実行
 *        コマンドライン引数に一致するコマンドをエントリーテーブルから取得し、ハンドラを実行する。
 * @param priv 制御データ(context)のポインタ
 * @return None
 */
static cli_command_code_t cli_command_execute(cli_private_t * priv)
{
    int argc    = priv->argument.count;
    char **argv = priv->argument.vector;

    cli_command_code_t code = CLI_COMMAND_COMPLETE;

    /* コマンド検索 */
    priv->active_command = cli_command_find(priv, argv[0]);

    if (priv->active_command)
    {
        code = priv->active_command->handler(argc, argv);
        switch (code)
        {
        case CLI_COMMAND_RUNNING:
            /* 状態遷移を要求 */
            code = CLI_COMMAND_RUNNING;
            break;

        case CLI_COMMAND_ERROR:
            /* コマンド実行失敗 */
            output_string(priv, "\r\nError: command execution failed\r\n");
            /* Fall-through */

        case CLI_COMMAND_COMPLETE:
            /* アクティブコマンドを破棄 */
            priv->active_command = NULL;
            break;

        default:
            break;
        }
    }
    else
    {
        output_string(priv, "\r\nError: '");
        output_string(priv, priv->argument.vector[0]);
        output_string(priv, "' command not found\r\n");
    }

    return code;
}

/**
 * @brief コマンド探索
 *        登録済みコマンドの登録名称と、引数nameに一致するコマンドの探索を行う。
 * @param priv 制御データ(context)のポインタ
 * @param name 探索対象の名称(登録名)を示すポインタ
 * @return コマンドのポインタ、またはNULLポインタ
 */
static const cli_command_t * cli_command_find(cli_private_t *priv, const char *name)
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
                return list;
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
        return CLI_IMPL_ERROR_FIFO_FULL;
    }

    uint8_t *p = priv->fifo;
    uint16_t offset = priv->fifo_head;

    *(p + offset) = data;

    priv->fifo_head = next;

    return CLI_IMPL_OK;
}

/**
 * データをFIFOからpullする
 */
static cli_impl_code_t cli_fifo_pull(cli_private_t * priv, uint8_t * data)
{
    if (priv->fifo_head == priv->fifo_tail)
    {
        /* データなし */
        return CLI_IMPL_ERROR_FIFO_EMPTY;
    }

    uint8_t *p = priv->fifo;
    uint16_t offset = priv->fifo_tail;

    *data = *(p + offset);

    priv->fifo_tail = (priv->fifo_tail + 1) % priv->fifo_size;

    return CLI_IMPL_OK;
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
static void cli_set_output( cli_private_t * priv, cli_output_t * output )
{
    if (output)
    {
        priv->output = output;
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