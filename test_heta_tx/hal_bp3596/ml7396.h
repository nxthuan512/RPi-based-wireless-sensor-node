/* ml7396.h - ML7396ドライバ ヘッダファイル  ML7396 driver header file
 *
 * Copyright (c) 2015  Communication Technology Inc.,
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef _INCLUDE_ML7396_H
#define _INCLUDE_ML7396_H


#include <limits.h>
#include <stdint.h>


/** 定数定義 */
// Constant definition

/* 外部公開関数の戻り値
 *
 * 特に説明のない関数の戻り値は全てこれになる
 */
// External public function of the return value
// All especially the return value of the function with no explanation become this
#define ML7396_STATUS_OK         0       /* 正常終了 */
#define ML7396_STATUS_EINVALID  -1       /* 無効なイベント */
#define ML7396_STATUS_EINIT     -2       /* 周辺デバイス初期化エラー */
#define ML7396_STATUS_EREGWRITE -3       /* レジスタ書き込みエラー */
#define ML7396_STATUS_EREGREAD  -4       /* レジスタ読み出しエラー */
#define ML7396_STATUS_ETIMSTART -5       /* タイマ開始エラー */
#define ML7396_STATUS_ETIMSTOP  -6       /* タイマ停止エラー */
#define ML7396_STATUS_ESETUP    -7       /* 初期設定エラー */
#define ML7396_STATUS_ELOCK     -8       /* リソース競合エラー */
#define ML7396_STATUS_UNKNOWN   INT_MIN  /* 不明 */

/* 送受信バッファの状態 (コールバック関数の status 値)
 *
 * 特に説明のないコールバック関数の status 引数の値は全てこれになる
 * 送信の場合 status がエラーのコールバック関数を呼び出した時点で続きのデータが在っても処理を中断する
 * 受信の場合はエラーのコールバック呼び出し後も続きのデータが在れば処理を続ける
 */
// Transmit and receive buffer state (status value of the callback function)
// Become this all especially the value of the status argument of no callback function of the description
// Interrupt the treatment even if continuation of the data is there in the * when the case of the transmission status calls the callback function of the error
// Continue the process if after the call-back call of error is also a continuation of the data is there in the case of reception *
#define ML7396_BUFFER_INIT   -100  /* 未処理状態 */
#define ML7396_BUFFER_ESIZE  -101  /* データサイズが異常 */
#define ML7396_BUFFER_ESTOP  -102  /* 処理が中断された */
#define ML7396_BUFFER_ECRC   -103  /* CRCエラー */
#define ML7396_BUFFER_ECCA   -104  /* CCAエラー */
#define ML7396_BUFFER_ERETRY -105  /* リトライエラー */

/* ドライバの状態
 */
typedef enum {
    ML7396_StateReset = 0, /* 初期状態 */
    ML7396_StateIdle,      /* アイドル状態 */
    ML7396_StateSendACK,   /* データ受信完了ACK送信中 */
    ML7396_StateSending,   /* データ送信中 */
    ML7396_StateWaitACK,   /* データ送信完了ACK待ち */
    ML7396_StateSleep      /* 省電力状態 */
} ML7396_State;


/** 変数型定義
 */

/* ヘッダ情報
 */
#define ML7396_HEADER_ADDRNONE  0x0000  /* アドレス指定なし */
#define ML7396_HEADER_PANIDNONE 0x0000  /* PANID指定なし */
#define ML7396_HEADER_SEQNONE   -1      /* シーケンス番号なし */
typedef struct {
    uint16_t fc;        /* ヘッダのフレームコントロール */
    int16_t seq;        /* シーケンス番号 */
    uint16_t dstpanid;  /* 宛て先PANID */
    uint16_t dstaddr;   /* 宛て先アドレス */
    uint16_t srcpanid;  /* 送り元PANID */
    uint16_t srcaddr;   /* 送り元アドレス */
} ML7396_Header;

/* 送受信バッファ		Send and receive buffer
 *
 * パラメータの使われ方は送信か受信かによって異なる	Usage of the parameters are different depending on whether the reception or transmission
 *  U は使用前にアプリ側で設定する必要あり	U is necessary to set the app side before use
 *  S はドライバが設定/更新する	S driver to set / update
 *  - は未使用		Unused
 *               受信時 送信時 (ACK送受信)	When receiving the time of transmission (ACK transmit and receive)
 * data             U      U        S
 * capacity         U      U        S
 * data[]           S      U        S
 * size             S      U        S
 * status           S      S        S
 * opt.rx.done      U      -        -
 * opt.rx.next      U      -        -
 * opt.rx.filter    U      -        -
 * opt.rx.ed        S      -        -
 * opt.tx.done      -      U        -
 * opt.tx.next      -      U        -
 * opt.tx.ack.wait  -      U        -
 * opt.tx.ack.retry -      U        -
 * opt.tx.cca.wait  -      U        -
 * opt.tx.cca.retry -      U        -
 * opt.tx.ed        -      S        -
 */
#define ML7396_BUFFER_CAPACITY (256-2)  /* capacity の最大値 (ML7369の最大パケットサイズ(2047バイト) - CRCサイズ(2バイト)) */
typedef struct ml7396_buffer {
    uint8_t *data;                                   /* パケットデータ (capacity 分のサイズ領域へのポインタ) */
    uint16_t capacity;                               /* パケットデータの最大サイズ (最大値は ML7396_BUFFER_CAPACITY) */
    uint16_t size;                                   /* パケットサイズ */
    int16_t status;                                  /* バッファの状態 (0以上=処理済のデータバイトサイズ) */
    union {                                          /* オプションパラメータ */
        struct {                                       /* データ受信パラメータ */
            uint8_t ed;                                  /* ED値 */
            void (*done)(struct ml7396_buffer *buffer);  /* 受信完了コールバック関数 */
            struct ml7396_buffer *next;                  /* 連続受信時の次のバッファポインタ(NULL=最後のバッファ) */
            int (*filter)(const ML7396_Header *header);  /* 受信フィルタ関数（戻り値: 真=受信, 偽=破棄) */
        } rx;
        struct {                                       /* データ送信パラメータ */
            uint8_t ed;                                  /* ACK受信時のED値 */
            void (*done)(struct ml7396_buffer *buffer);  /* 送信完了コールバック関数 */
            struct ml7396_buffer *next;                  /* 連続送信時の次のバッファポインタ(NULL=最後のバッファ) */
            struct {                                     /* 再送とCCAの設定 */
                uint16_t wait;                             /* ACK待ち時間/CCAチェック間隔 [msec単位] */
                uint8_t retry;                             /* 再送/CCAチェックリトライ回数 */
            } ack, cca;
        } tx;
/* これ以降はドライバ内部で使用 */
        struct {                                       /* データ送受信共通パラメータ */
            uint8_t ed;                                  /* データ受信時のED値 */
            void (*done)(struct ml7396_buffer *buffer);  /* 処理完了コールバック関数 */
            struct ml7396_buffer *next;                  /* 次のバッファポインタ(NULL=最後のバッファ) */
        } common;
    } opt;
} ML7396_Buffer;


/** API関数
 */

/* 内部データ強制リセット
 */
extern int ml7396_reset(void);

/* 各種設定
 *
 * data: 設定パラメータのポインタ
 */
extern int ml7396_setup(void *data);

/* 受信待ち開始
 *
 * buffer: 受信データ収納バッファ
 *   buffer->data: 受信データ収納領域
 *   buffer->capacity: 受信データ収納領域のサイズ
 *   buffer->opt.rx.done: 受信完了コールバック関数
 *   buffer->opt.rx.filter: 受信フィルタ関数(フィルタリングしない場合はNULL)
 *   buffer->opt.rx.next: 次の受信バッファ (最後ならNULL, 1つのバッファを繰り返し使うなら自分自身)
 *                        途中でエラーが発生しても次のバッファへ進む
 *
 * 受信完了コールバック関数
 *   void (*done)(ML7396_Buffer *buffer)
 *     buffer->data[]: 受信したデータ
 *     buffer->status: 0以上=受信したデータサイズ, 0未満=異常終了(バッファの状態)
 *     buffer->opt.rx.ed: ED値
 *
 * 受信フィルタ関数
 *   int (*filter)(const uint8_t *data, int16_t status)
 *     data[]: 受信データ
 *     status: 受信データサイズ
 *     戻り値: 0以外=受信, 0=破棄
 */
extern int ml7396_rxstart(ML7396_Buffer *buffer);

/* 受信待ち停止
 */
extern int ml7396_rxstop(void);

/* 送信開始
 *
 * buffer: 送信データバッファ
 *   buffer->data: 送信信データ収納領域
 *   buffer->capacity: 送信信データ収納領域のサイズ
 *   buffer->data[]: 送信データ
 *   buffer->size: 送信データサイズ
 *   buffer->opt.tx.done: 送信完了コールバック関数
 *   buffer->opt.rx.next: 次の送信バッファ (最後ならNULL)
 *                        途中でエラーが発生した場合次のバッファが残っていても処理を停止する
 *   buffer->opt.tx.ack.wait: ACK待ちタイムアウト時間(msec単位)
 *   buffer->opt.tx.ack.retry: 送信リトライ回数
 *   buffer->opt.tx.cca.wait: CCAチェック間隔(msec単位)
 *   buffer->opt.tx.cca.retry: CCAチェックリトライ回数
 *
 * コールバック関数
 *   void (*done)(ML7396_Buffer *buffer)
 *     buffer->data[]: 送信したデータ配列
 *     buffer->status: 0以上=送信したデータ数, 0未満=異常終了(バッファの状態)
 *     buffer->opt.tx.ed: ED値
 *       異常終了の場合 buffer->opt.tx.next に未処理の送信バッファが残った状態で終了するので、必要に応じて続きの再開かバッファの開放が必要
 */
extern int ml7396_txstart(ML7396_Buffer *buffer);

/* 省電力状態へ移行
 */
extern int ml7396_sleep(void);

/* 省電力状態から復帰
 */
extern int ml7396_wakeup(void);

/* 自機アドレスのポインタを取得
 *
 * 戻り値: 自機アドレスのポインタ
 */
extern uint16_t *ml7396_myaddr(void);

/* 自機PANIDのポインタを取得
 *
 * 戻り値: 自機アドレスのポインタ
 */
extern uint16_t *ml7396_mypanid(void);

/* ドライバの状態を取得
 *
 * 戻り値: ドライバの状態
 */
extern ML7396_State ml7396_state(void);

/* 送信中のバッファを取得
 *
 * 戻り値: NULL=送信停止中, NULL以外=送信中のバッファ
 */
extern ML7396_Buffer *ml7396_txbuffer(void);

/* 受信中のバッファを取得
 *
 * 戻り値: NULL=受信停止中, NULL以外=受信中のバッファ
 */
extern ML7396_Buffer *ml7396_rxbuffer(void);


/* アドレスフィルタ設定
 *
 * 戻り値: なし
 */
extern void ml7396_setAddrFilter(uint8_t *rx_filter);

#endif  /* #ifndef _INCLUDE_ML7396_H */
