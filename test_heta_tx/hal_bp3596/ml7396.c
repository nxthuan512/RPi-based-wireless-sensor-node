/* ml7396.c - ML7396ドライバ	ML7396 driver
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


#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "ml7396_hwif.h"
#include "ml7396_reg.h"
#include "endian.h"
#include "ieee802154.h"
#include "ml7396.h"
// 2015.06.08 Eiichi Saito
#include "hal.h"


#ifdef DEBUG
/* デバッグ時はエラー発生で処理中断 */
#define ASSERT(_test) if(!(_test)) while(!0)
#define GOTO_ERROR while(!0)
#endif  /* #ifdef DEBUG */


/* デバッグ情報
 */

/* 必ず _test が真になる事を示す
 *  なので _test が偽の状態は考慮不要
 */
#ifndef ASSERT
#define ASSERT(_test)  /* 処理なし */
#endif  /* #ifndef ASSERT */

/* 通常動作で起こりえるエラー分岐を示す
 */
#ifndef GOTO_ERROR
#define GOTO_ERROR goto error
#endif  /* #ifndef GOTO_ERROR */


/** 固定パラメータ
 */

/* 送受信FIFO読み書き開始タイミング
 *  FIFOが埋まるか/空になるこの指定バイト分前に読み出し/書き込みを開始
 */
#define FIFO_MARGIN 32

/* 送受信時のCRCバイトサイズ
 *  できれば固定値ではなく自動取得させたい
 */
#define TXCRC_SIZE 2
#define RXCRC_SIZE 2

/* ACKパケットデータの最大サイズ
 */
#define ACK_BUFFER_CAPACITY (127-(TXCRC_SIZE<RXCRC_SIZE?TXCRC_SIZE:RXCRC_SIZE))


/** 戻り値制御
 */

/* _test のエラー判定 エラーならば真 */
#define IS_ERROR(_test) \
    ((_test) < 0)

/* _test を status に代入し更に status がエラーならば error へ飛ばす */
#define ON_ERROR(_test) \
    do { \
        status = (_test); \
        if (IS_ERROR(status)) \
            GOTO_ERROR; \
    } while (0)

/* _test がエラーならば status に _status を代入し error へ飛ばす */
#define ON_ERROR_STATUS(_test, _status) \
    do { \
        if (IS_ERROR(_test)) { \
            status = (_status); \
            GOTO_ERROR; \
        } \
    } while (0)


/** ML7396レジスタ操作
 */

/* レジスタ操作バッファ */
static struct {
    volatile uint8_t lock;  /* 排他ロックカウンタ */
    uint8_t bank;           /* 最後に切り替えたバンク番号 */
    uint8_t wdata[256];     /* 書き込みデータバッファ */
    uint8_t rdata[256];     /* 読み出しデータバッファ */
} reg = {
    0,    /* lock */
    0xff  /* bank */
};

/* バンク切り替え (ml7396_regwrite と ml7396_regread から間接的に呼び出される)
 *
 * bank: 切り替えるバンク番号
 */
static int regbank(uint8_t bank) {
    int status = ML7396_STATUS_UNKNOWN;

    switch (bank) {
    case 0:  /* BANK0 */
    case 1:  /* BANK1 */
    case 2:  /* BANK2 */
    case 8:  /* BANK0 + access enable */
    case 9:  /* BANK1 + access enable */
    case 10: /* BANK2 + access enable */
        if (bank != reg.bank) {
            reg.wdata[0] = (0x00<<1)|0x01, reg.wdata[1] = bank&0x03;
            // 2015.05.27 Eiichi Saito
            if(bank > 2) reg.wdata[1] = reg.wdata[1] | 0x80;
            ON_ERROR_STATUS(ml7396_hwif_spi_transfer(reg.wdata, reg.rdata, 2), ML7396_STATUS_EREGWRITE);
            reg.bank = bank;
        }
        status = ML7396_STATUS_OK;
        break;
    case 0xff:
        reg.lock = 0;
        reg.bank = bank;
        status = ML7396_STATUS_OK;
        break;
    default:
        GOTO_ERROR;
    }
error:
    return status;
}

/* 書き込み
 *
 * bank: 書き込むレジスタのバンク番号
 * addr: 書き込むレジスタの先頭アドレス
 * data[]: 書き込むレジスタ値の配列
 * size: 書き込みレジスタ数
 */
int ml7396_regwrite(uint8_t bank, uint8_t addr, const uint8_t *data, uint8_t size) {
    int status = ML7396_STATUS_UNKNOWN;

//	__DI();
    if (reg.lock++) {
        status = ML7396_STATUS_ELOCK;
        GOTO_ERROR;
    }
    ON_ERROR_STATUS(regbank(bank), ML7396_STATUS_EREGWRITE);
    reg.wdata[0] = (addr << 1) | 0x01;
    memcpy(reg.wdata + 1, data, size);
    ON_ERROR_STATUS(ml7396_hwif_spi_transfer(reg.wdata, reg.rdata, size + 1), ML7396_STATUS_EREGWRITE);
    status = ML7396_STATUS_OK;
error:
    --reg.lock;
//	__EI();
    return status;
}

/* 読み出し
 *
 * bank: 読み出すレジスタのバンク番号
 * addr: 読み出すレジスタの先頭アドレス
 * data[]: 読み出したレジスタ値を収納する配列
 * size: 読み出しレジスタ数
 */
int ml7396_regread(uint8_t bank, uint8_t addr, uint8_t *data, uint8_t size) {
    int status = ML7396_STATUS_UNKNOWN;

//	__DI();
    if (reg.lock++) {
        status = ML7396_STATUS_ELOCK;
        GOTO_ERROR;
    }
    ON_ERROR_STATUS(regbank(bank), ML7396_STATUS_EREGREAD);
    reg.wdata[0] = (addr << 1) | 0x00;
    memset(reg.wdata + 1, 0xff, size);  /* ここは仕様上不定値でも問題ないが、余計なノイズ出力を抑えるため'H'固定にする */
    ON_ERROR_STATUS(ml7396_hwif_spi_transfer(reg.wdata, reg.rdata, size + 1), ML7396_STATUS_EREGREAD);
    memcpy(data, reg.rdata + 1, size);
    status = ML7396_STATUS_OK;
error:
    --reg.lock;
//	__EI();
    return status;
}


/** よく使うバッファ操作
 */

/* コールバック関数呼び出し
 */
#define BUFFER_DONE(_buffer) \
    do { \
        if ((_buffer)->opt.common.done != NULL) \
            (_buffer)->opt.common.done(_buffer); \
    } while (0)


/** よく使うレジスタ操作
 *
 * 受信手順: (FIFOデータの最後にED値が付く設定である事)
 *   REG_RXON();
 *   連続受信時の繰り返し範囲 {
 *     FIFO_FULL 割り込み待ち
 *     REG_RXSTART(&buffer);
 *     REG_RXCONTINUE(&buffer);
 *     何度か繰り返し {
 *       FIFO_FULL 割り込み待ち
 *       REG_RXCONTINUE(&buffer);
 *     }
 *     受信完了割り込み待ち
 *     REG_RXCONTINUE(&buffer);
 *     REG_RXDONE(&buffer);
 *   (この時点で受信したデータは揃っている)
 *     CRCエラー割り込みあり {
 *       CRCエラー処理
 *     } else {
 *       ACKを返す場合 {
 *         ACKデータに対して「送信手順」を実行
 *       }
 *       正常終了処理
 *     }
 *   }
 *   REG_TRXOFF();
 *
 * 送信手順: (自動送信OFF(送信完了で自動的にTRX_OFFになる)が有効である事)
 *   REG_TRXOFF();
 *   連続送信時の繰り返し範囲 {
 *     REG_CCAEN();
 *     REG_RXON();
 *     CCA検出完了割り込み待ち
 *     REG_TRXOFF();
 *     キャリアなし {
 *       REG_TXSTART(&buffer);
 *       REG_TXCONTINUE(&buffer);
 *       何度か繰り返し {
 *         FIFO_EMPTY 割り込み待ち
 *         REG_TXCONTINUE(&buffer);
 *       }
 *       送信完了割り込み待ち
 *   (この時点で送信は完了している)
 *       ACKを待つ場合 {
 *         ACKデータに対して「受信手順」を実行
 *       }
 *       正常終了処理
 *     } else {
 *       キャリアありエラー処理
 *     }
 *   }
 */

/* レジスタ1バイト書き込み
 */
#define REG_WRB(_addr, _data) \
    do { \
        uint8_t _reg_data[1]; \
        _reg_data[0] = (_data); \
        ON_ERROR(ml7396_regwrite(_addr, _reg_data, 1)); \
    } while (0)

/* レジスタ1バイト読み出し
 */
#define REG_RDB(_addr, _data) \
    do { \
        uint8_t _reg_data[1]; \
        ON_ERROR(ml7396_regread(_addr, _reg_data, 1));  \
        (_data) = _reg_data[0]; \
    } while (0)

/* PHY強制リセット
 */
#define REG_PHYRST() \
    do { \
        uint8_t _reg_data[1]; \
        _reg_data[0] = 0x03; \
        ON_ERROR(ml7396_regwrite(REG_ADR_RF_STATUS, _reg_data, 1)); \
        _reg_data[0] = 0x88; \
        ON_ERROR(ml7396_regwrite(REG_ADR_RST_SET, _reg_data, 1)); \
    } while (0)

/* 送受信停止
 *  ML7396の状態を TRX_OFF に変更
 */
#define REG_TRXOFF() \
    do { \
        uint8_t _reg_data[1]; \
        _reg_data[0] = 0x08; \
        ON_ERROR(ml7396_regwrite(REG_ADR_RF_STATUS, _reg_data, 1)); \
    } while (0)

/* 受信開始
 *  ML7396の状態を RX_ON に変更
 */
#define REG_RXON() \
    do { \
        uint8_t _reg_data[1]; \
        _reg_data[0] = 0x06; \
        ON_ERROR(ml7396_regwrite(REG_ADR_RF_STATUS, _reg_data, 1)); \
    } while (0)

/* 受信バッファ読み出し(先頭データ)
 */
#define REG_RXSTART(_buffer) \
    do { \
        uint16_t _data_size; \
        uint8_t _reg_data[2]; \
        ASSERT((_buffer)->status == ML7396_BUFFER_INIT); \
        ON_ERROR(ml7396_regread(REG_ADR_RD_RX_FIFO, _reg_data, 2)); \
        _data_size = n2u16(_reg_data) & 0x07ff; \
        if (_data_size < RXCRC_SIZE) { \
            (_buffer)->size = 0; \
            (_buffer)->status = ML7396_BUFFER_ESIZE; \
        } \
        else { \
            _data_size -= RXCRC_SIZE; \
            (_buffer)->size = _data_size; \
            if (_data_size > (_buffer)->capacity)  \
                (_buffer)->status = ML7396_BUFFER_ESIZE; \
            else \
                (_buffer)->status = 0; \
        } \
    } while (0)

/* 受信バッファ読み出し(継続データ)
 *  CRCとED値は読み出さずに残す
 */
#define REG_RXCONTINUE(_buffer) \
    do { \
        uint8_t _size; \
        uint16_t _data_size; \
        ASSERT((_buffer)->status >= 0); \
        _size = 256-FIFO_MARGIN; \
        _data_size = (_buffer)->size - (_buffer)->status; \
        if (_data_size <= _size) \
            _size = _data_size; \
        else \
            --_size; \
        if (_size > 0) { \
            ON_ERROR(ml7396_regread(REG_ADR_RD_RX_FIFO, (_buffer)->data + (_buffer)->status, _size)); \
            (_buffer)->status += _size; \
        } \
    } while (0)

/* ED値読み出し
 *  REG_RXCONTINUE() でFIFOに残ったCRCの破棄とED値を読みだすので読み出し処理の最後に実行する事
 */
#define REG_RXDONE(_buffer) \
    do { \
        uint8_t _reg_data[4]; \
        ON_ERROR(ml7396_regread(REG_ADR_RD_RX_FIFO, _reg_data, RXCRC_SIZE)); \
        ON_ERROR(ml7396_regread(REG_ADR_RD_RX_FIFO, &(_buffer)->opt.common.ed, 1)); \
    } while (0)

/* 送信バッファ書き込み(先頭データ)
 */
// 2015.05.07 Eiichi Saito : Change PHR CRC length field 0x0800 -> 0x1800
#define REG_TXSTART(_buffer) \
    do { \
        uint16_t _data_size; \
        uint8_t _reg_data[2]; \
        ASSERT((_buffer)->status == ML7396_BUFFER_INIT); \
        _data_size = (_buffer)->size; \
        if (_data_size > (_buffer)->capacity) \
            (_buffer)->status = ML7396_BUFFER_ESIZE; \
        else { \
            _data_size += TXCRC_SIZE; \
            _data_size |= 0x1800; \
            u2n16_set(_data_size, _reg_data); \
            ON_ERROR(ml7396_regwrite(REG_ADR_WR_TX_FIFO, _reg_data, 2)); \
            (_buffer)->status = 0; \
        } \
    } while (0)

/* 送信バッファ書き込み開始(継続データ)
 *  必要に応じて自動でML7396の状態を RX_ON に変更
 */
// 2015.06.08 Eiichi Saito : addition delay
#define REG_TXCONTINUE(_buffer) \
    do { \
        uint8_t _size; \
        uint16_t _data_size; \
        ASSERT((_buffer)->status >= 0); \
        _size = 256-FIFO_MARGIN; \
        _data_size = (_buffer)->size - (_buffer)->status; \
        if (_data_size <= _size) \
            _size = _data_size; \
        if (_size > 0) { \
            ON_ERROR(ml7396_regwrite(REG_ADR_WR_TX_FIFO, (_buffer)->data + (_buffer)->status, _size)); \
            (_buffer)->status += _size; \
            HAL_delayMicroseconds(300); \
        } \
    } while (0)

/* CCA実行
 */
// 2015.07.29 Eiichi Saito : not synchronize in CCA
#define REG_CCAEN() \
    do { \
        uint8_t _reg_data[1]; \
        _reg_data[0] = 0x00; \
        ON_ERROR(ml7396_regwrite(REG_ADR_DEMSET3, _reg_data, 1)); \
        ON_ERROR(ml7396_regwrite(REG_ADR_DEMSET14, _reg_data, 1)); \
        _reg_data[0] = 0x10; \
        ON_ERROR(ml7396_regwrite(REG_ADR_CCA_CNTRL, _reg_data, 1)); \
    } while (0)

/* 割り込み要因取得
 * uint32_t _intsrc
 */
#define REG_INTSRC(_intsrc) \
    do { \
        uint8_t _reg_data[3]; \
        ml7396_regread(REG_ADR_INT_SOURCE_GRP1, _reg_data, 3); \
        (_intsrc) = ((uint32_t)_reg_data[0] <<  0) | ((uint32_t)_reg_data[1] <<  8) | ((uint32_t)_reg_data[2] << 16); \
    } while (0)

/* 割り込み許可/禁止
 * uint32_t _inten
 */
#define REG_INTEN(_inten) \
    do { \
        uint8_t _reg_data[3]; \
        _reg_data[0] = (uint8_t)((_inten) >>  0) | 0xc0, _reg_data[1] = (uint8_t)((_inten) >>  8), _reg_data[2] = (uint8_t)((_inten) >> 16); \
        ml7396_regwrite(REG_ADR_INT_SOURCE_GRP1, _reg_data, 3); \
        ml7396_regwrite(REG_ADR_INT_EN_GRP1, _reg_data, 3); \
    } while (0)

/* 割り込み要因とFIFOクリア
 * uint32_t _intclr
 */
#define REG_INTCLR(_intclr) \
    do { \
        uint8_t _reg_data[3]; \
        if (_intclr) { \
            _reg_data[0] = ~(uint8_t)((_intclr) >>  0), _reg_data[1] = ~(uint8_t)((_intclr) >>  8), _reg_data[2] = ~(uint8_t)((_intclr) >> 16); \
            ml7396_regwrite(REG_ADR_INT_SOURCE_GRP1, _reg_data, 3); \
        } \
    } while (0)


/** IEEE 802.15.4g ヘッダ関係
 */

/* 送信データ作成
 *
 * header構造体を解析して下記情報をdataのMACヘッダに埋め込む:
 *  フレームコントロールのPANID圧縮
 *  フレームコントロールのシーケンス番号圧縮
 *  フレームコントロールの受信アドレスモード
 *  フレームコントロールの送信アドレスモード
 *  シーケンス番号
 *  宛て先PANID
 *  宛て先アドレス
 *  送り元PANID
 *  送り元アドレス
 *
 *  data: 送信データバッファ
 *  size: 送信データバッファサイズ
 *  *header: 送信データに展開するヘッダ情報
 *  戻り値: ペイロードデータの先頭アドレス
 *          戻り値 - data = MACヘッダサイズ
 *          MACヘッダサイズ + ペイロードサイス = 送信データサイズ
 *
 */
static uint8_t *make_data(uint8_t *data, uint16_t size, ML7396_Header *header) {
    // 2015.06.04 Eiichi Saito
    uint16_t tmp_addr;
    uint8_t *payload = NULL;
    struct {
        uint16_t dstaddrmode;
        uint16_t srcaddrmode;
        uint16_t panidcomps;
        uint16_t seqsuppress;
    } fc;

    /* IEEE 802.15.4e フレームのビーコンとデータ、ACK以外は未対応 */
    switch (header->fc & (IEEE802154_FC_IEEE802154_MASK|IEEE802154_FC_TYPE_MASK)) {
    case IEEE802154_FC_IEEE802154_E|IEEE802154_FC_TYPE_BEACON:
    case IEEE802154_FC_IEEE802154_E|IEEE802154_FC_TYPE_DATA:
        break;
    // 2015.06.04 Eiichi Saito
    case IEEE802154_FC_IEEE802154_E|IEEE802154_FC_TYPE_ACK:
        tmp_addr= header->dstaddr;
        header->dstaddr = header->srcaddr;
        header->srcaddr = tmp_addr;
        break;
    default:
        goto error;
    }
    /* ヘッダ情報から宛て先/送り元のPANID/アドレスとシーケンス番号のフィールドサイズを取得 */
    fc.dstaddrmode = header->dstaddr == ML7396_HEADER_ADDRNONE ? IEEE802154_FC_DAMODE_NONE : IEEE802154_FC_DAMODE_SHORT;
    fc.srcaddrmode = header->srcaddr == ML7396_HEADER_ADDRNONE ? IEEE802154_FC_SAMODE_NONE : IEEE802154_FC_SAMODE_SHORT;
    switch (fc.dstaddrmode | fc.srcaddrmode) {  /* 規格上無効な組み合わせは未対応 */
    case IEEE802154_FC_DAMODE_NONE|IEEE802154_FC_SAMODE_NONE:
        if (header->dstpanid == ML7396_HEADER_PANIDNONE && header->srcpanid == ML7396_HEADER_PANIDNONE)
            fc.panidcomps = 0;
        else if (header->dstpanid != ML7396_HEADER_PANIDNONE && header->srcpanid == ML7396_HEADER_PANIDNONE)
            fc.panidcomps = IEEE802154_FC_PANID_COMPS;
        else
            goto error;
        break;
    case IEEE802154_FC_DAMODE_NONE|IEEE802154_FC_SAMODE_SHORT:
        if (header->dstpanid == ML7396_HEADER_PANIDNONE && header->srcpanid == ML7396_HEADER_PANIDNONE)
            fc.panidcomps = IEEE802154_FC_PANID_COMPS;
        else if (header->dstpanid == ML7396_HEADER_PANIDNONE && header->srcpanid != ML7396_HEADER_PANIDNONE)
            fc.panidcomps = 0;
        else
            goto error;
        break;
    case IEEE802154_FC_DAMODE_SHORT|IEEE802154_FC_SAMODE_NONE:
    case IEEE802154_FC_DAMODE_SHORT|IEEE802154_FC_SAMODE_SHORT:
        if (header->dstpanid == ML7396_HEADER_PANIDNONE && header->srcpanid == ML7396_HEADER_PANIDNONE)
            fc.panidcomps = IEEE802154_FC_PANID_COMPS;
        else if (header->dstpanid != ML7396_HEADER_PANIDNONE && header->srcpanid == ML7396_HEADER_PANIDNONE)
            fc.panidcomps = 0;
        else
            goto error;
        break;
    default:
        goto error;
    }
    fc.seqsuppress = header->seq == ML7396_HEADER_SEQNONE ? IEEE802154_FC_SEQ_SUPPRESS : 0;
    if (size < 2)
        goto error;
    /* MACヘッダのフィールドコントロールに宛て先/送り元のPANID/アドレスとシーケンス番号のフィールドサイズ情報を反映 */
    header->fc &= ~(IEEE802154_FC_PANID_COMPS|IEEE802154_FC_SEQ_SUPPRESS|IEEE802154_FC_DAMODE_MASK|IEEE802154_FC_SAMODE_MASK);
    header->fc |= fc.dstaddrmode | fc.srcaddrmode | fc.panidcomps | fc.seqsuppress;
    u2v16_set(header->fc, data), data += 2, size -= 2;
    /* MACヘッダに宛て先/送り元のPANID/アドレスとシーケンス番号を付加 */
    if (!fc.seqsuppress) {
        if (size < 1)
            goto error;
        *data++ = header->seq, --size;
    }
    if (header->dstpanid != ML7396_HEADER_PANIDNONE) {
        if (size < 2)
            goto error;
        u2v16_set(header->dstpanid, data), data += 2, size -= 2;
    }
    if (header->dstaddr != ML7396_HEADER_ADDRNONE) {
        if (size < 2)
            goto error;
        u2v16_set(header->dstaddr, data), data += 2, size -= 2;
    }
    if (header->srcpanid != ML7396_HEADER_PANIDNONE) {
        if (size < 2)
            goto error;
        u2v16_set(header->srcpanid, data), data += 2, size -= 2;
    }
    if (header->srcaddr != ML7396_HEADER_ADDRNONE) {
        if (size < 2)
            goto error;
        u2v16_set(header->srcaddr, data), data += 2, size -= 2;
    }
    /* ペイロードの先頭アドレスを返す */
    payload = data;
error:
    return payload;
}

/* 受信データ解析
 *
 * dataのMACヘッダを解析して下記情報をheader構造体に反映する:
 *  フレームコントロールのPANID圧縮
 *  フレームコントロールのシーケンス番号圧縮
 *  フレームコントロールの受信アドレスモード
 *  フレームコントロールの送信アドレスモード
 *  シーケンス番号
 *  宛て先PANID
 *  宛て先アドレス
 *  送り元PANID
 *  送り元アドレス
 *
 *  data: 受信データバッファ
 *  size: 受信データサイズ
 *  *header: 送信データから展開されたヘッダ情報
 *  戻り値: ペイロードデータの先頭アドレス
 *          戻り値 - data = MACヘッダサイズ
 *          受信データサイズ - MACヘッダサイズ = 受信ペイロードサイズ
 */
static const uint8_t *parse_data(const uint8_t *data, uint16_t size, ML7396_Header *header) {
    const uint8_t *payload = NULL;
    struct {
        uint16_t dstaddrmode;
        uint16_t srcaddrmode;
        uint16_t panidcomps;
        uint16_t seqsuppress;
        // 2015.07.10 Eiichi Saito : The conditions for an address filter are changed.
        uint16_t dstaddr;
    } fc;

    /* IEEE 802.15.4e フレームのビーコンとデータ、ACK以外は未対応 */
    if (size < 2)
        goto error;
    header->fc = v2u16(data), data += 2, size -= 2;
    switch (header->fc & (IEEE802154_FC_IEEE802154_MASK|IEEE802154_FC_TYPE_MASK)) {
    case IEEE802154_FC_IEEE802154_E|IEEE802154_FC_TYPE_BEACON:
    case IEEE802154_FC_IEEE802154_E|IEEE802154_FC_TYPE_DATA:
    case IEEE802154_FC_IEEE802154_E|IEEE802154_FC_TYPE_ACK:
        break;
    default:
        goto error;
    }
    /* フレームコントロールフィールドから宛て先/送り元のPANID/アドレスとシーケンス番号のフィールドサイズを取得 */
    fc.dstaddrmode = header->fc & IEEE802154_FC_DAMODE_MASK;
    fc.srcaddrmode = header->fc & IEEE802154_FC_SAMODE_MASK;
    fc.panidcomps = header->fc & IEEE802154_FC_PANID_COMPS;
    fc.seqsuppress = header->fc & IEEE802154_FC_SEQ_SUPPRESS;
    /* MACヘッダから宛て先/送り元のPANID/アドレスとシーケンス番号を取得(16ビット以外のアドレスは未対応) */
    if (!fc.seqsuppress) {
        if (size < 1)
            goto error;
        header->seq = *data++, --size;
    }
    else
        header->seq = ML7396_HEADER_SEQNONE;
    if ( fc.panidcomps && fc.dstaddrmode == IEEE802154_FC_DAMODE_NONE && fc.srcaddrmode == IEEE802154_FC_SAMODE_NONE ||
        !fc.panidcomps && fc.dstaddrmode != IEEE802154_FC_DAMODE_NONE ) {
        if (size < 2)
            goto error;
        header->dstpanid = v2u16(data), data += 2, size -= 2;
    }
    else
        header->dstpanid = ML7396_HEADER_PANIDNONE;
    switch (fc.dstaddrmode) {
    case IEEE802154_FC_DAMODE_NONE:
        header->dstaddr = ML7396_HEADER_ADDRNONE;
        break;
    case IEEE802154_FC_DAMODE_SHORT:
        if (size < 2)
            goto error;
        header->dstaddr = v2u16(data), data += 2, size -= 2;
        break;
    case IEEE802154_FC_DAMODE_LLDN:
    case IEEE802154_FC_DAMODE_LONG:
    default:
        goto error;
    }
    if (!fc.panidcomps && fc.dstaddrmode == IEEE802154_FC_DAMODE_NONE && fc.srcaddrmode != IEEE802154_FC_SAMODE_NONE) {
        if (size < 2)
            goto error;
        header->srcpanid = v2u16(data), data += 2, size -= 2;
    }
    else
        header->srcpanid = ML7396_HEADER_PANIDNONE;
    switch (fc.srcaddrmode) {
    case IEEE802154_FC_SAMODE_NONE:
        header->srcaddr = ML7396_HEADER_ADDRNONE;
        break;
    case IEEE802154_FC_SAMODE_SHORT:
        if (size < 2)
            goto error;
        header->srcaddr = v2u16(data), data += 2, size -= 2;
        break;
    case IEEE802154_FC_SAMODE_LLDN:
    case IEEE802154_FC_SAMODE_LONG:
    default:
        goto error;
    }
    /* ペイロードの先頭アドレスを返す */
    payload = data;
error:
    return payload;
}

/* 受信データ解析と受信/破棄の判定
 *
 * *rx: 解析/判定する受信データバッファ
 * *rxheader: 解析したヘッダ情報
 * 戻り値: 0=破棄, 0以外=受信
 */
static int is_rx_recvdata(const ML7396_Buffer *rx, ML7396_Header *rxheader) {
    int status = 0;
    uint16_t dstaddr;

    ASSERT(rx->status >= 0);
    if (parse_data(rx->data, rx->status, rxheader) == NULL)
        goto error;                      /* 解析不能なデータは破棄 */
    // 2015.07.10 Eiichi Saito : The conditions for an address filter are changed.
    dstaddr = *ml7396_myaddr();
    if ((dstaddr != rxheader->dstaddr) && 
        !(rxheader->dstaddr == 0xffff && rxheader->dstpanid == 0xffff))
       goto error;

    switch (rxheader->fc & IEEE802154_FC_TYPE_MASK) {
        case IEEE802154_FC_TYPE_BEACON:  /* IEEE802.15.4eパケットのビーコンは受信 */
        case IEEE802154_FC_TYPE_DATA:    /* IEEE802.15.4eパケットのデータも受信 */
            status = !0;
            break;
    }                                    /* その他は全て破棄 */
error:
    return status;
}

/* ACKを返信するかの判定とACKフレーム生成
 *
 * *rxheader: 解析済のヘッダ情報
 * myaddr: 自機アドレス
 * *ack: 送信するACKフレームの送信データバッファ
 * 戻り値: 0=ACK送信不要, 0以外=ACK送信必要
 */
static int make_rx_sendack(ML7396_Header *rxheader, uint16_t myaddr, ML7396_Buffer *ack) {
    int status = 0;
    uint8_t *payload;

    if (rxheader->fc & IEEE802154_FC_ACKREQ && rxheader->dstaddr == myaddr) {  /* ACK要求が付いていて自機アドレス宛てならばACK返信 */
        /* 受信データのACK要求フラグを落としてテータタイプをACKに変更したMACヘッダを返信 */
        rxheader->fc &= ~(IEEE802154_FC_TYPE_MASK|IEEE802154_FC_ACKREQ);
        rxheader->fc |= IEEE802154_FC_TYPE_ACK;
        payload = make_data(ack->data, ack->capacity, rxheader);
        if (payload == NULL)
            goto error;
        ack->size = payload - ack->data;
        status = !0;
    }
error:
    return status;
}

/* ACK待ちをすべきかの判定と待ち条件保持
 *
 * *tx: ACK待すべきか判定する送信データバッファ
 * *ackheader: 受信待ちACKのヘッダ情報
 * 戻り値: 0=ACK待ち不要, 0以外=ACK待ち必要
 */
static int is_tx_waitack(const ML7396_Buffer *tx, ML7396_Header *ackheader) {
    int status = 0;

    ASSERT(tx->status >= 0);
    if (parse_data(tx->data, tx->status, ackheader) == NULL)
        goto error;                            /* 解析不能なデータはACKでないと判定 */
    if (ackheader->fc & IEEE802154_FC_ACKREQ)  /* ACK要求がついていればACK待ちをする */
        status = !0;
error:
    return status;
}

/* 待っているACKを受信したかの判定
 *
 * *ack: ACK受信したデータバッファ
 * *ackheader: 受信待ちACKのヘッダ情報
 * 戻り値: 0=待っているACKではない, 0以外=待っているACKを受信した
 */
static int is_tx_recvack(const ML7396_Buffer *ack, const ML7396_Header *ackheader) {
    int status = 0;
    ML7396_Header header;

    ASSERT(ack->status >= 0);
    if (parse_data(ack->data, ack->status, &header) == NULL)
        goto error;                                    /* 解析不能なデータはACKでないと判定 */
    switch (header.fc & IEEE802154_FC_TYPE_MASK) {
    case IEEE802154_FC_TYPE_ACK:                       /* データタイプがACKであり */
//      if (header.seq == ackheader->seq &&            /* 送信データとヘッダ情報が一致するならそのACK返信と判定 */
//          header.dstpanid == ackheader->dstpanid &&
//          header.dstaddr == ackheader->dstaddr &&
//          header.srcpanid == ackheader->srcpanid &&
//          header.srcaddr == ackheader->srcaddr )
//          status = !0;
// 2015.05.07 Eiichi Saito : ACK Frame analysis is corrected.
        if (header.seq == ackheader->seq &&            /* 送信データとヘッダ情報が一致するならそのACK返信と判定 */
            header.dstpanid == ackheader->dstpanid &&
            header.dstaddr == ackheader->srcaddr )
            status = !0;
        break;
    }
error:
    return status;
}


/** イベントマシン共通データ
 */
// 2015.07.31 Eiichi Saito : Duplicate SequneceNumber is not notified to a higher layer.
typedef struct {
    uint16_t myaddr;          /* 自機アドレス */
    uint16_t mypanid;         /* 自機PANID */
    uint16_t last_seq;        /* 重複SequneceNumberチェック */
    ML7396_State state;       /* イベントマシンの状態 */
    ML7396_Buffer *rx;        /* パケット受信バッファ */
    ML7396_Buffer *tx;        /* パケット送信バッファ */
    ML7396_Buffer ack;        /* ACK送受信バッファ */
    ML7396_Header ackheader;  /* ACKを識別するヘッダ情報 */
    struct {                  /* リトライカウンタ */
        uint8_t ack;            /* 再送 */
        uint8_t cca;            /* CCAチェック */
    } count;
} EM_Data;


/** イベントフラグ
 */

/* ハードウエア要因 (複数同時発生時は論理和される) */
#define HW_EVENT_FIFO_EMPTY   0x00000010  /* FIFO_EMPTY */
#define HW_EVENT_FIFO_FULL    0x00000020  /* FIFO_FULL */
#define HW_EVENT_CCA_DONE     0x00000100  /* CCA検出完了 */
#define HW_EVENT_FIFO_TX_DONE 0x00030000  /* 送信完了 */
#define HW_EVENT_FIFO_RX_DONE 0x000c0000  /* 受信完了 */
#define HW_EVENT_CRC_ERROR    0x00300000  /* CRCエラー */
#define HW_EVENT_TIMEOUT      0x80000000  /* タイマータイムアウト */

/* ソフトウェア要因 */
#define SW_EVENT_SETUP   1  /* 初期化 */
#define SW_EVENT_RXSTART 2  /* パケット受信開始 */
#define SW_EVENT_RXSTOP  3  /* パケット受信停止 */
#define SW_EVENT_TXSTART 4  /* パケット送信開始 */
#define SW_EVENT_SLEEP   5  /* 省電力状態へ移行 */
#define SW_EVENT_WAKEUP  6  /* 省電力状態から復帰 */


/** 状態移行
 */

/* 各状態における割り込み許可状況 */
static const uint32_t event_enable[] = {
    0,                                                                             /* ML7396_StateReset */
    HW_EVENT_FIFO_RX_DONE|HW_EVENT_FIFO_FULL|HW_EVENT_CRC_ERROR,                   /* ML7396_StateIdle */
    HW_EVENT_FIFO_TX_DONE|HW_EVENT_FIFO_EMPTY,                                     /* ML7396_StateSendACK */
    HW_EVENT_FIFO_TX_DONE|HW_EVENT_FIFO_EMPTY|HW_EVENT_CCA_DONE|HW_EVENT_TIMEOUT,  /* ML7396_StateSending */
    HW_EVENT_FIFO_RX_DONE|HW_EVENT_FIFO_FULL|HW_EVENT_CRC_ERROR|HW_EVENT_TIMEOUT,  /* ML7396_StateWaitACK */
    0                                                                              /* ML7396_StateSleep */
};

/* 状態移行と同時にそれに必要な割り込みを許可 */
#define SWITCH_STATE(_state) \
    do { \
        uint32_t inten; \
        em_data->state = (_state); \
        inten = event_enable[em_data->state]; \
        REG_INTEN(inten); \
    } while (0)


/** ソフトウェアイベント処理
 */

/* Idle処理
 *  ソフトウェア待ち時間ループ時に実行される
 */
static void idle(void) {
    /* 処理なし */
}

/* 初期化
 *
 * *em_data: イベントマシン共有データ
 * *data: 各種設定値(必要な型にキャストして使用)
 *
 * Idle, TRXOFF of RXON
 */
static int em_setup(EM_Data *em_data, void *data) {
    int status = ML7396_STATUS_UNKNOWN;
    uint8_t reg_data;
    uint32_t intsrc;
    uint8_t get_my_addr[4];

    switch (em_data->state) {
    case ML7396_StateReset:
        ON_ERROR_STATUS(ml7396_hwif_init(), ML7396_STATUS_EINIT);
        regbank(0xff);
        /* クロック安定待ち */
        do {
            idle();
            REG_RDB(REG_ADR_CLK_SET, reg_data);
        } while (!(reg_data & 0x80));
        // 2015.10.26 Eiichi Saito   addition random backoff
        HAL_I2C_read(0x50, 0x26, get_my_addr, 2);
        srand(n2u16(get_my_addr));
        /* break無し */
    default:
        SWITCH_STATE(ML7396_StateReset);  /* Resetステートへ移行 */
        REG_PHYRST();  /* PHYをリセット */
        em_data->rx = NULL, em_data->tx = NULL;
        ON_ERROR_STATUS(ml7396_hwif_regset(data), ML7396_STATUS_ESETUP);  /* レジスタ設定 */
        /* IEEE802.15.4gパケット, 自動送信ON, 受信データにEDを付加, Whiteningを行う */
        REG_RDB(REG_ADR_PACKET_MODE_SET, reg_data);
        reg_data |=  0x1e;
        REG_WRB(REG_ADR_PACKET_MODE_SET, reg_data);
        /* 送受信時にCRC16を演算 */
        REG_RDB(REG_ADR_FEC_CRC_SET, reg_data);
        reg_data |=  0x0b, reg_data &= ~0x04;
        REG_WRB(REG_ADR_FEC_CRC_SET, reg_data);
        /* FIFO_MARGIN*2 バイト分FIFOに書き込んだ時点で自動で TX_ON へ移行 */
        REG_WRB(REG_ADR_FAST_TX_SET, FIFO_MARGIN<<1);
        /* 送信完了で自動で TRX_OFF へ移行 */
        // 2015.12.14 Eiichi Saito: enable TX_DONERX 
//      REG_WRB(REG_ADR_ACK_TIMER_EN, 0x10);
        REG_WRB(REG_ADR_ACK_TIMER_EN, 0x20);
        /* FIFO_MARGIN バイト分余裕を持ってFIFOを読み書きする設定 */
        REG_WRB(REG_ADR_TX_ALARM_LH, FIFO_MARGIN);      /* 未使用だが設定しておく必要あり 255では何故かFIFOアクセスエラーが発生する */
        REG_WRB(REG_ADR_TX_ALARM_HL, FIFO_MARGIN);      /* 送信FIFOの残りデータ数が FIFO_MARGIN になれば割り込み発生 */
        REG_WRB(REG_ADR_RX_ALARM_LH, 256-FIFO_MARGIN);  /* 受信FIFOの空き領域が FIFO_MARGIN になれば割り込み発生 */
        REG_WRB(REG_ADR_RX_ALARM_HL, 256-FIFO_MARGIN);  /* 未使用だが設定しておく必要あり */
        /* FIFOの制御仕様
         *
         * 送信:
         *  T1) 256-n バイト分(サイズがそれ未満の場合は全て)書き込む。
         *  T2) b まで( or 全てを)書き込んだタイミングで送信開始。
         *  T3) 残りデータが有る場合、a を切ったタイミングで 256-n バイ
         *      ト分のデータ(サイズがそれ未満の場合は全て)を書き込む。
         *  T4) 全データを書き込むまで T3 を繰り返す。
         *
         * 受信:
         *  R1) c を超えた(or 全データ受信した)タイミングで 256-n バイ
         *      ト分(or 全て)読み出す。
         *  R2) 全データを読み出すまで R1 を繰り返す。
         *
         * FIFO:
         *   +---+ 255
         *  c|---| 256-n (RX_ALARM_LH レジスタ設定値)
         *   |   |
         *  b|---| n*2   (FAST_TX_SET レジスタ設定値)
         *  a|---| n     (TX_ALARM_HL レジスタ設定値)
         *   +---+ 0
         *    n=FIFO_MARGIN
         */
        /* VCOキャリブレーション */
        // 2015.07.10 Eiichi Saito : After a VCO calibration clears all the interruption.
        REG_INTCLR(0x00000000);
        REG_WRB(REG_ADR_VCO_CAL_START, 0x01);
        do {
            idle();
            REG_INTSRC(intsrc);
        } while (!(intsrc & 0x00000004));
        REG_INTCLR(0x00000004);
        SWITCH_STATE(ML7396_StateIdle);  /* Idelステートへ移行 */
        status = ML7396_STATUS_OK;
    }
error:
    return status;
}

/* パケット受信待ち開始
 *
 * *em_data: イベントマシン共有データ
 * buffer: 送信データバッファのポインタ
 *
 * Idle, TRXOFF or RXON
 */
static int em_rxstart(EM_Data *em_data, ML7396_Buffer *buffer) {
    int status = ML7396_STATUS_UNKNOWN;

    if (em_data->rx != NULL) {  /* 既に待ち状態 */
        status = ML7396_STATUS_EINVALID;
        GOTO_ERROR;
    }
    buffer->status = ML7396_BUFFER_INIT;  /* 受信バッファクリア */
    em_data->rx = buffer;  /* 受信バッファ登録 */
    REG_RXON();
    status = ML7396_STATUS_OK;
error:
    return status;
}

/* パケット受信待ち停止
 *
 * *em_data: イベントマシン共有データ
 * data: 未使用
 *
 * Idle, RXON or TRXOFF
 */
static int em_rxstop(EM_Data *em_data, void *data) {
    int status = ML7396_STATUS_UNKNOWN;

    if (em_data->rx == NULL) {  /* 既に待ち状態でない */
        status = ML7396_STATUS_EINVALID;
        GOTO_ERROR;
    }
    REG_TRXOFF();
    em_data->rx->status = ML7396_BUFFER_ESTOP;
    BUFFER_DONE(em_data->rx);
    em_data->rx = NULL;  /* 受信バッファ削除 */
    status = ML7396_STATUS_OK;
error:
    return status;
}

/* パケット送信開始
 *
 * *em_data: イベントマシン共有データ
 * buffer: 送信データバッファのポインタ
 *   buffer->data[]: 送信データの配列
 *   buffer->size: 送信データ数
 *   buffer->opt.tx.ack.wait: ACK待ち時間
 *   buffer->opt.tx.ack.retry: 再送回数
 *   buffer->opt.tx.cca.wait: CCAチェック間隔
 *   buffer->opt.tx.cca.retry: CCAチェック回数
 *
 * Idle, RXON or TRXOFF
 */
static int em_txstart(EM_Data *em_data, ML7396_Buffer *buffer) {
    int status = ML7396_STATUS_UNKNOWN;

    if (em_data->rx != NULL)
        REG_TRXOFF();
    em_data->count.ack = 0, em_data->count.cca = 0;
    buffer->status = ML7396_BUFFER_INIT;  /* 送信バッファ未送信状態 */
    em_data->tx = buffer;  /* 送信バッファ登録 */
    SWITCH_STATE(ML7396_StateSending);
    REG_CCAEN();
    REG_RXON();
    status = ML7396_STATUS_OK;
error:
    return status;
}

/* 省電力状態へ移行
 *
 * *em_data: イベントマシン共有データ
 * data: 未使用
 *
 * Idle, RXON or TRXOFF
 */
static int em_sleep(EM_Data *em_data, void *data) {
    int status = ML7396_STATUS_UNKNOWN;
    uint8_t reg_data;

    if (em_data->rx != NULL)
        REG_TRXOFF();
    SWITCH_STATE(ML7396_StateSleep);
    /* 省電力状態へ移行 */
    REG_RDB(REG_ADR_CLK_SET, reg_data);
    reg_data |=  0x20;
    REG_WRB(REG_ADR_CLK_SET, reg_data);
    status = ML7396_STATUS_OK;
error:
    return status;
}

/* 省電力状態から復帰
 *
 * *em_data: イベントマシン共有データ
 * data: 未使用
 *
 * Sleep, TRXOFF
 */
static int em_wakeup(EM_Data *em_data, void *data) {
    int status = ML7396_STATUS_UNKNOWN;
    uint8_t reg_data;

    /* 省電力状態から復帰 */
    REG_RDB(REG_ADR_CLK_SET, reg_data);
    reg_data &= ~0x20;
    REG_WRB(REG_ADR_CLK_SET, reg_data);
    /* クロック安定待ち */
    do {
        idle();
        REG_RDB(REG_ADR_CLK_SET, reg_data);
    } while (!(reg_data & 0x80));
    SWITCH_STATE(ML7396_StateIdle);
    if (em_data->rx != NULL)
        REG_RXON();
    status = ML7396_STATUS_OK;
error:
    return status;
}


/** ハードウェアイベント処理
 */

/* パケット受信
 *
 * Idle, RXON
 */
static int em_rx_datarecv(EM_Data *em_data, const uint32_t *hw_event) {
    int status = ML7396_STATUS_UNKNOWN;
    ML7396_Header rxheader;

    ASSERT(em_data->rx != NULL);
    switch (em_data->rx->status) {
    case ML7396_BUFFER_INIT:  /* 先頭データならばパケットサイズ情報を取得 */
        REG_RXSTART(em_data->rx);
        if (IS_ERROR(em_data->rx->status)) {  /* 受信パケットサイズが異常 */
            REG_PHYRST();  /* この時点のエラーからの復旧はPHYリセットが必要 */
            BUFFER_DONE(em_data->rx);
            em_data->rx = em_data->rx->opt.rx.next;
            if (em_data->rx != NULL) {
                em_data->rx->status = ML7396_BUFFER_INIT;  /* 受信バッファをクリア */
                REG_RXON();
            }
            break;
        }
        /* break無し */
    default:
        #ifndef SNIFFER
        if (*hw_event & HW_EVENT_CRC_ERROR) {  /* CRCエラー */
            em_data->rx->status = ML7396_BUFFER_ECRC;
            BUFFER_DONE(em_data->rx);
            em_data->rx = em_data->rx->opt.rx.next;
            if (em_data->rx != NULL)
                em_data->rx->status = ML7396_BUFFER_INIT;  /* 受信バッファをクリア */
            else
                REG_TRXOFF();
            break;
        }
        #endif
        REG_RXCONTINUE(em_data->rx);
        if (*hw_event & HW_EVENT_FIFO_RX_DONE) {  /* 受信完了 */
            REG_RXDONE(em_data->rx);  /* ED値を取得 */
            #ifndef SNIFFER
            // 2015.07.10 Eiichi Saito : The conditions for an address filter are changed.
            // アドレス判定しておかないとACK送信モードになる。
            if (!is_rx_recvdata(em_data->rx, &rxheader) ||  /* 受信/破棄の判定 */
                em_data->rx->opt.rx.filter != NULL && !em_data->rx->opt.rx.filter(&rxheader) )  /* フィルタリングチェック */
                em_data->rx->status = ML7396_BUFFER_INIT;  /* 受信バッファを破棄して再利用 */
            else if (make_rx_sendack(&rxheader, em_data->myaddr, &em_data->ack)) {  /* ACKを送信するかの判定とACKフレーム生成 */
                em_data->ack.status = ML7396_BUFFER_INIT;
                switch (em_data->ack.status) {
                case ML7396_BUFFER_INIT:  /* アルゴリズム上必ずここへ入る */
                    REG_TXSTART(&em_data->ack);
                    if (IS_ERROR(em_data->ack.status)) {  /* ACKパケットサイズが異常 */
                        /* ACK送信は諦めて正常受信の処理をする */
                        BUFFER_DONE(em_data->rx);
                        em_data->rx = em_data->rx->opt.rx.next;
                        if (em_data->rx != NULL)
                            em_data->rx->status = ML7396_BUFFER_INIT;  /* 受信バッファをクリア */
                        else
                            REG_TRXOFF();
                    }
                    else {
                        SWITCH_STATE(ML7396_StateSendACK);
                        // 2015.12.14 Eiichi Saito adjusted 2msec from receiveing data to starting ack
                        HAL_delayMicroseconds(600);
                        REG_TXCONTINUE(&em_data->ack);
                    }
                    break;
                default:  /* コンパイラの最適化でこの分岐は消えると思われる */
                    ASSERT(0);
                }
            }
            else
            #endif
            {
                /* 受信完了 */
                BUFFER_DONE(em_data->rx);
                em_data->rx = em_data->rx->opt.rx.next;
                if (em_data->rx != NULL)
                    em_data->rx->status = ML7396_BUFFER_INIT;  /* 受信バッファをクリア */
                else
                    REG_TRXOFF();
            }
        }
    }
    status = ML7396_STATUS_OK;
error:
    return status;
}

/* ACK送信(パケット受信完了に対する)
 *
 * SendAck, TXON
 */
static int em_rx_acksend(EM_Data *em_data, const uint32_t *hw_event) {
    int status = ML7396_STATUS_UNKNOWN;

    switch (em_data->ack.status) {
    default:
        REG_TXCONTINUE(&em_data->ack);
    }
    status = ML7396_STATUS_OK;
error:
    return status;
}

/* ACK送信完了(パケット受信完了に対する)
 *
 * SendAck, TXON->TRXOFF
 */
// 2015.07.31 Eiichi Saito : Duplicate SequneceNumber is not notified to a higher layer.
static int em_rx_ackdone(EM_Data *em_data, const uint32_t *hw_event) {
    int status = ML7396_STATUS_UNKNOWN;

    ML7396_Header *header = em_data->ack.data;

    // __asm("nop"); // for debug
    ASSERT(em_data->rx != NULL);
    if ((uint8_t)em_data->last_seq != (uint8_t)header->seq)
        BUFFER_DONE(em_data->rx);
    em_data->last_seq = header->seq;

    em_data->rx = em_data->rx->opt.rx.next;
    SWITCH_STATE(ML7396_StateIdle);
    if (em_data->rx != NULL) {
        em_data->rx->status = ML7396_BUFFER_INIT;  /* 受信バッファをクリア */
        REG_RXON();
    }
    status = ML7396_STATUS_OK;
error:
    return status;
}

/* CCA完了(パケット送信前の)
 *
 * Sending, RXON
 */
static int em_tx_ccadone(EM_Data *em_data, const uint32_t *hw_event) {
    int status = ML7396_STATUS_UNKNOWN;
    uint8_t reg_data;
    // 2015.10.26 Eiichi Saito   addition random backoff
    uint16_t cca_wait;

    ASSERT(em_data->tx != NULL);
    REG_TRXOFF();  /* 自動でOFFになるなら不要 */
    REG_RDB(REG_ADR_CCA_CNTRL, reg_data);  /* CCA_RSLT読み出し */
    // 2015.07.29 Eiichi Saito : not synchronize in CCA
    REG_WRB(REG_ADR_DEMSET3, 0x64);
    REG_WRB(REG_ADR_DEMSET14, 0x27);
    switch (reg_data & 0x03) {
    case 0x00:  /* キャリアなし */
// 2015.10.26 Eiichi Saito   addition random backoff for Debug
//  if (em_data->count.cca != 0) {
        switch (em_data->tx->status) {
        case ML7396_BUFFER_INIT:
            REG_TXSTART(em_data->tx);
            if (IS_ERROR(em_data->tx->status)) {  /* 送信パケットサイズが異常 */
                BUFFER_DONE(em_data->tx);
                em_data->tx = NULL;
                SWITCH_STATE(ML7396_StateIdle);
                if (em_data->rx != NULL)
                    REG_RXON();
            }
            else
                REG_TXCONTINUE(em_data->tx);
            break;
        default:
            ASSERT(0);
        }
        break;
//  }
    case 0x01:  /* キャリアあり */
        if (em_data->count.cca < em_data->tx->opt.tx.cca.retry) {  /* リトライ回数が残っている? */
            ++em_data->count.cca;
            // 2015.10.26 Eiichi Saito   addition random backoff
            if (!em_data->tx->opt.tx.cca.wait) {
                cca_wait = 100;
            }else{
                cca_wait = rand();
                cca_wait = (cca_wait&0x000F) << em_data->tx->opt.tx.cca.wait;
            }

            if (!cca_wait) {
                cca_wait = 100;
            }

            ON_ERROR_STATUS(ml7396_hwif_timer_start(cca_wait), ML7396_STATUS_ETIMSTART);  /* タイマ割り込み設定 */
        }
        else {
            em_data->tx->status = ML7396_BUFFER_ECCA;
            BUFFER_DONE(em_data->tx);
            em_data->tx = NULL;
            SWITCH_STATE(ML7396_StateIdle);
            if (em_data->rx != NULL)
                REG_RXON();
        }
        break;
    default:
        ASSERT(0);
    }
    status = ML7396_STATUS_OK;
error:
    return status;
}

/* CCAリトライタイムアウト
 *
 * Sending, RXON
 */
static int em_tx_ccatimeout(EM_Data *em_data, const uint32_t *hw_event) {
    int status = ML7396_STATUS_UNKNOWN;

    ASSERT(em_data->tx != NULL);
    ON_ERROR_STATUS(ml7396_hwif_timer_stop(), ML7396_STATUS_ETIMSTOP);  /* タイマ割り込み停止 */
    REG_TRXOFF();
    REG_CCAEN();
    REG_RXON();
    status = ML7396_STATUS_OK;
error:
    return status;
}


/* パケット送信
 *
 * Sending, TXON
 */
static int em_tx_datasend(EM_Data *em_data, const uint32_t *hw_event) {
    int status = ML7396_STATUS_UNKNOWN;

    ASSERT(em_data->tx != NULL);
    switch (em_data->tx->status) {
    default:
        REG_TXCONTINUE(em_data->tx);
    }
    status = ML7396_STATUS_OK;
error:
    return status;
}

/* パケット送信完了
 *
 * Sending, TXON->TRXOFF
 */
static int em_tx_datadone(EM_Data *em_data, const uint32_t *hw_event) {
    int status = ML7396_STATUS_UNKNOWN;

    ASSERT(em_data->tx != NULL);
    switch (em_data->tx->status) {
    default:
        ASSERT(em_data->tx->status >= 0);
        em_data->tx->opt.tx.ed = 0;
        if (is_tx_waitack(em_data->tx, &em_data->ackheader)) {  /* ACK待ちをすべきかの判定と待条件保持 */
            em_data->ack.status = ML7396_BUFFER_INIT;
            SWITCH_STATE(ML7396_StateWaitACK);
            // 2015.12.14 Eiichi Saito: for preference of SubGHz
            HAL_EX_disableInterrupt();
            ON_ERROR_STATUS(ml7396_hwif_timer_start(em_data->tx->opt.tx.ack.wait), ML7396_STATUS_ETIMSTART);  /* タイマ割り込み設定 */
            REG_RXON();
        }
        else {
            BUFFER_DONE(em_data->tx);
            em_data->tx = em_data->tx->opt.tx.next;
            if (em_data->tx != NULL) {
                em_data->count.ack = 0, em_data->count.cca = 0;
                em_data->tx->status = ML7396_BUFFER_INIT;
                REG_CCAEN();
                REG_RXON();
            }
            else {
                SWITCH_STATE(ML7396_StateIdle);
                if (em_data->rx != NULL)
                    REG_RXON();
            }
        }
    }
    status = ML7396_STATUS_OK;
error:
    return status;
}

/* ACK受信(パケット送信後の)
 *
 * WaitACK, RXON
 */
static int em_tx_ackrecv(EM_Data *em_data, const uint32_t *hw_event) {
    int status = ML7396_STATUS_UNKNOWN;

    switch (em_data->ack.status) {
    case ML7396_BUFFER_INIT:  /* 先頭データならばパケットサイズ情報を取得 */
        REG_RXSTART(&em_data->ack);
        if (IS_ERROR(em_data->ack.status)) {  /* ACKパケットサイズが異常 */
            REG_PHYRST();  /* この時点のエラーからの復旧はPHYリセットが必要 */
            em_data->ack.status = ML7396_BUFFER_INIT;  /* 受信データを破棄して引き続き次を受信 */
            REG_RXON();
            break;
        }
        /* break無し */
    default:
        if (*hw_event & HW_EVENT_CRC_ERROR) {  /* CRCエラー */
            em_data->ack.status = ML7396_BUFFER_INIT;  /* 受信データを破棄して引き続き次を受信 */
            break;
        }
        REG_RXCONTINUE(&em_data->ack);
        if (*hw_event & HW_EVENT_FIFO_RX_DONE) {  /* 受信完了 */
            REG_RXDONE(em_data->tx);  /* ED値を取得 */
            if (is_tx_recvack(&em_data->ack, &em_data->ackheader)) {  /* 待っているACKを受信したかの判定 */
                // 2015.12.14 Eiichi Saito: for preference of SubGHz
                HAL_EX_enableInterrupt();
                ON_ERROR_STATUS(ml7396_hwif_timer_stop(), ML7396_STATUS_ETIMSTOP);  /* タイマ割り込み停止 */
                REG_TRXOFF();
                BUFFER_DONE(em_data->tx);
                em_data->tx = em_data->tx->opt.tx.next;
                if (em_data->tx != NULL) {
                    em_data->count.ack = 0, em_data->count.cca = 0;
                    em_data->tx->status = ML7396_BUFFER_INIT;
                    SWITCH_STATE(ML7396_StateSending);
                    REG_CCAEN();
                    REG_RXON();
                }
                else {
                    SWITCH_STATE(ML7396_StateIdle);
                    if (em_data->rx != NULL)
                        REG_RXON();
                }
            }
            else  /* ACKでない */
                em_data->ack.status = ML7396_BUFFER_INIT;  /* 受信データを破棄して引き続き次を受信 */
        }
    }
    status = ML7396_STATUS_OK;
error:
    return status;
}

/* ACK待ちタイムアウト
 *
 * WaitACK, RXON
 */
static int em_tx_acktimeout(EM_Data *em_data, const uint32_t *hw_event) {
    int status = ML7396_STATUS_UNKNOWN;

    ASSERT(em_data->tx != NULL);
    REG_TRXOFF();
    if (em_data->count.ack < em_data->tx->opt.tx.ack.retry) {  /* リトライ回数が残っている? */
        ++em_data->count.ack, em_data->count.cca = 0;
        em_data->tx->status = ML7396_BUFFER_INIT;  /* 送信バッファを未送信状態に戻す */
        SWITCH_STATE(ML7396_StateSending);
        REG_CCAEN();
        REG_RXON();
    }
    else {
        // 2015.12.14 Eiichi Saito: for preference of SubGHz
        HAL_EX_enableInterrupt();
        // 2015.12.01 Eiichi Saito : SugGHz timer chaneged from TM01 to TM67.
        ON_ERROR_STATUS(ml7396_hwif_timer_stop(), ML7396_STATUS_ETIMSTOP);  /* タイマ割り込み停止 */
        em_data->tx->status = ML7396_BUFFER_ERETRY;
        BUFFER_DONE(em_data->tx);
        em_data->tx = NULL;
        SWITCH_STATE(ML7396_StateIdle);
        if (em_data->rx != NULL)
            REG_RXON();
    }
    status = ML7396_STATUS_OK;
error:
    return status;
}


/** イベントマシンメイン
 *
 * *em_data: イベントマシン共有データ
 * *data: オプション引数(必要な型にキャストして使用)
 * sw_event: ソフトウェア要因イベント
 * hw_event: ハードウェア要因イベント
 * *hw_done: イベントマシンメインで処理したハードウェア要因イベント
 */
static int em_main(EM_Data *em_data, void *data, int sw_event, uint32_t hw_event, uint32_t *hw_done) {
    int status = ML7396_STATUS_UNKNOWN;
    uint32_t event;

    ASSERT(em_data != NULL);
    switch (em_data->state) {
    case ML7396_StateReset:
        switch (sw_event) {
        case SW_EVENT_SETUP:  /* 初期化 */
            status = em_setup(em_data, data);
            break;
        case 0:
            status = ML7396_STATUS_OK;
            break;
        default:
            status = ML7396_STATUS_EINVALID;
        }
        break;
    case ML7396_StateIdle:
        switch (sw_event) {
        case SW_EVENT_SETUP:  /* 初期化 */
            status = em_setup(em_data, data);
            break;
        case SW_EVENT_RXSTART:  /* パケット受信待ち開始 */
            status = em_rxstart(em_data, data);
            break;
        case SW_EVENT_RXSTOP:  /* パケット受信待ち停止 */
            status = em_rxstop(em_data, data);
            break;
        case SW_EVENT_TXSTART:  /* パケット送信開始 */
            status = em_txstart(em_data, data);
            break;
        case SW_EVENT_SLEEP:  /* 省電力状態へ移行 */
            status = em_sleep(em_data, data);
            break;
        case 0:
            event = hw_event & (HW_EVENT_FIFO_RX_DONE|HW_EVENT_FIFO_FULL|HW_EVENT_CRC_ERROR);  /* パケット受信 */
            if (event) {
                em_rx_datarecv(em_data, &event);
                *hw_done |= event | HW_EVENT_FIFO_EMPTY | (event & HW_EVENT_CRC_ERROR) >> 14;  /* クリアする処理済割り込みフラグとFIFOバッファを指定 */
            }
            status = ML7396_STATUS_OK;
            break;
        default:
            status = ML7396_STATUS_EINVALID;
        }
        break;
    case ML7396_StateSendACK:
        switch (sw_event) {
        case 0:
            event = hw_event & HW_EVENT_FIFO_EMPTY;  /* ACK送信 */
            if (event) {
                em_rx_acksend(em_data, &event);
                *hw_done |= event | HW_EVENT_FIFO_FULL;  /* クリアする処理済割り込みフラグを指定 */
            }
            event = hw_event & HW_EVENT_FIFO_TX_DONE;  /* ACK送信完了 */
            if (event) {
                em_rx_ackdone(em_data, &event);
                *hw_done |= event;  /* クリアする処理済割り込みフラグを指定 */
            }
            status = ML7396_STATUS_OK;
            break;
        default:
            status = ML7396_STATUS_EINVALID;
        }
        break;
    case ML7396_StateSending:
        switch (sw_event) {
        case 0:
            event = hw_event & HW_EVENT_CCA_DONE;  /* CCA検出完了 */
            if (event) {
                em_tx_ccadone(em_data, &event);
                *hw_done |= event;  /* クリアする処理済割り込みフラグを指定 */
            }
            event = hw_event & HW_EVENT_TIMEOUT;  /* CCAリトライタイムアウト */
            if (event) {
                em_tx_ccatimeout(em_data, &event);
                *hw_done |= event;  /* クリアする処理済割り込みフラグを指定 */
            }
            event = hw_event & HW_EVENT_FIFO_EMPTY;  /* パケット送信  */
            if (event) {
                em_tx_datasend(em_data, &event);
                *hw_done |= event | HW_EVENT_FIFO_FULL;  /* クリアする処理済割り込みフラグを指定 */
            }
            event = hw_event & HW_EVENT_FIFO_TX_DONE;  /* パケット送信完了 */
            if (event) {
                em_tx_datadone(em_data, &event);
                *hw_done |= event;  /* クリアする処理済割り込みフラグを指定 */
            }
            status = ML7396_STATUS_OK;
            break;
        default:
            status = ML7396_STATUS_EINVALID;
        }
        break;
    case ML7396_StateWaitACK:
        switch (sw_event) {
        case 0:
            event = hw_event & (HW_EVENT_FIFO_RX_DONE|HW_EVENT_FIFO_FULL|HW_EVENT_CRC_ERROR);  /* ACK受信 */
            if (event) {
                em_tx_ackrecv(em_data, &event);
                *hw_done |= event | HW_EVENT_FIFO_EMPTY | (event & HW_EVENT_CRC_ERROR) >> 14;  /* クリアする処理済割り込みフラグとFIFOバッファを指定 */
            }
            event = hw_event & HW_EVENT_TIMEOUT;  /* ACK待ちタイムアウト */
            if (event) {
                em_tx_acktimeout(em_data, &event);
                *hw_done |= event;  /* クリアする処理済割り込みフラグを指定 */
            }
            status = ML7396_STATUS_OK;
            break;
        default:
            status = ML7396_STATUS_EINVALID;
        }
        break;
    case ML7396_StateSleep:
        switch (sw_event) {
        case SW_EVENT_WAKEUP:  /* 省電力状態から復帰 */
            status = em_wakeup(em_data, data);
            break;
        case 0:
            status = ML7396_STATUS_OK;
            break;
        default:
            status = ML7396_STATUS_EINVALID;
        }
        break;
    default:
        ASSERT(0);
    }
    return status;
}


/* イベントマシン共有データ */
#ifndef DEBUG
static  /* デバッグ時は外部公開 */
#endif  /* #ifndef DEBUG */
// 2015.07.31 Eiichi Saito : Duplicate SequneceNumber is not notified to a higher layer.
EM_Data em_data = {
    0x0000,            /* 自機アドレス */
    0x0000,            /* 自機PANID */
    0xffff,            /* 重複SequneceNumberチェック */
    ML7396_StateReset  /* 初期ステート */
};


/** イベント発生部
 */

/* ML7396によるイベント */
static void sint_handler(void) {
    uint32_t hw_event, hw_done;

    ml7396_hwif_timer_di();  /* em_main() と em_data の排他制御 */
    /* 割り込み要因取得 */
    REG_INTSRC(hw_event);
    /* イベントマシン呼び出し */
    hw_done = 0;
    em_main(&em_data, NULL, 0, hw_event, &hw_done);
    /* 処理済の割り込み要因をクリア */
    REG_INTCLR(hw_done);
    ml7396_hwif_timer_ei();  /* em_main() と em_data の排他制御 */
}

/* タイマーによるイベント */
static void timer_handler(void) {
    uint32_t hw_event, hw_done;

    ml7396_hwif_sint_di();  /* em_main() と em_data の排他制御 */
    /* ハードウェア要因のイベントフラグ生成 */
    hw_event = HW_EVENT_TIMEOUT, hw_done = 0;
    /* イベントマシン呼び出し */
    em_main(&em_data, NULL, 0, hw_event, &hw_done);
    ml7396_hwif_sint_ei();  /* em_main() と em_data の排他制御 */
}


/**  イベントマシンAPI
 */

/* 内部データ強制リセット
 */
int ml7396_reset(void) {
    int status = ML7396_STATUS_UNKNOWN;
    static uint8_t data[ACK_BUFFER_CAPACITY];

    ASSERT(ACK_BUFFER_CAPACITY <= ML7396_BUFFER_CAPACITY);
    em_data.myaddr  = 0x0000;           /* 自機アドレス */
    em_data.mypanid = 0x0000;           /* 自機PANID */
    em_data.state = ML7396_StateReset;  /* 初期ステート */
    em_data.ack.data = data, em_data.ack.capacity = ACK_BUFFER_CAPACITY;  /* ACK送受信データ領域設定 */
    status = ML7396_STATUS_OK;
    return status;
}

/* 初期化
 *
 * *data: 各種設定値
 */
int ml7396_setup(void *data) {
    int status = ML7396_STATUS_UNKNOWN;
    uint32_t hw_event, hw_done;

    ml7396_hwif_sint_di(), ml7396_hwif_timer_di();  /* em_main() と em_data の排他制御 */
    ml7396_hwif_sint_handler(sint_handler), ml7396_hwif_timer_handler(timer_handler);  /* 割り込みハンドラ関数登録 */
    hw_event = 0, hw_done = 0;  /* ハードウェア要因のイベントフラグ生成 */
    status = em_main(&em_data, data, SW_EVENT_SETUP, hw_event, &hw_done);  /* イベントマシン呼び出し */
    ml7396_hwif_sint_ei(), ml7396_hwif_timer_ei();  /* em_main() と em_data の排他制御 */
    return status;
}

/* 受信待ち開始
 *
 * buffer: 受信データバッファポインタ
 */
int ml7396_rxstart(ML7396_Buffer *buffer) {
    int status = ML7396_STATUS_UNKNOWN;
    uint32_t hw_event, hw_done;

    hw_event = 0, hw_done = 0;  /* ハードウェア要因のイベントフラグ生成 */
    ml7396_hwif_sint_di(), ml7396_hwif_timer_di();  /* em_main() と em_data の排他制御 */
    status = em_main(&em_data, buffer, SW_EVENT_RXSTART, hw_event, &hw_done);  /* イベントマシン呼び出し */
    ml7396_hwif_sint_ei(), ml7396_hwif_timer_ei();  /* em_main() と em_data の排他制御 */
    return status;
}

/* 受信待ち停止
 */
int ml7396_rxstop(void) {
    int status = ML7396_STATUS_UNKNOWN;
    uint32_t hw_event, hw_done;

    /* ハードウェア要因のイベントフラグ生成 */
    hw_event = 0, hw_done = 0;
    /* イベントマシン呼び出し */
    ml7396_hwif_sint_di(), ml7396_hwif_timer_di();  /* em_main() と em_data の排他制御 */
    status = em_main(&em_data, NULL, SW_EVENT_RXSTOP, hw_event, &hw_done);
    ml7396_hwif_sint_ei(), ml7396_hwif_timer_ei();  /* em_main() と em_data の排他制御 */
    return status;
}

/* 送信開始
 *
 * buffer: 送信データバッファポインタ
 *   buffer->data[]: 送信データの配列
 *   buffer->size: 送信データ数
 *   buffer->opt.tx.ack.wait: ACK待ち時間
 *   buffer->opt.tx.ack.retry: 再送回数
 *   buffer->opt.tx.cca.wait: CCAチェック間隔
 *   buffer->opt.tx.cca.retry: CCAチェック回数
 */
int ml7396_txstart(ML7396_Buffer *buffer) {
    int status = ML7396_STATUS_UNKNOWN;
    uint32_t hw_event, hw_done;

    /* ハードウェア要因のイベントフラグ生成 */
    hw_event = 0, hw_done = 0;
    /* イベントマシン呼び出し */
    ml7396_hwif_sint_di(), ml7396_hwif_timer_di();  /* em_main() と em_data の排他制御 */
    status = em_main(&em_data, buffer, SW_EVENT_TXSTART, hw_event, &hw_done);
    ml7396_hwif_sint_ei(), ml7396_hwif_timer_ei();  /* em_main() と em_data の排他制御 */
    return status;
}

/* 省電力状態へ移行
 */
int ml7396_sleep(void) {
    int status = ML7396_STATUS_UNKNOWN;
    uint32_t hw_event, hw_done;

    /* ハードウェア要因のイベントフラグ生成 */
    hw_event = 0, hw_done = 0;
    /* イベントマシン呼び出し */
    ml7396_hwif_sint_di(), ml7396_hwif_timer_di();  /* em_main() と em_data の排他制御 */
    status = em_main(&em_data, NULL, SW_EVENT_SLEEP, hw_event, &hw_done);
    ml7396_hwif_sint_ei(), ml7396_hwif_timer_ei();  /* em_main() と em_data の排他制御 */
    return status;
}

/* 省電力状態から復帰
 */
int ml7396_wakeup(void) {
    int status = ML7396_STATUS_UNKNOWN;
    uint32_t hw_event, hw_done;

    /* ハードウェア要因のイベントフラグ生成 */
    hw_event = 0, hw_done = 0;
    /* イベントマシン呼び出し */
    ml7396_hwif_sint_di(), ml7396_hwif_timer_di();  /* em_main() と em_data の排他制御 */
    status = em_main(&em_data, NULL, SW_EVENT_WAKEUP, hw_event, &hw_done);
    ml7396_hwif_sint_ei(), ml7396_hwif_timer_ei();  /* em_main() と em_data の排他制御 */
    return status;
}

/* アドレスフィルタ設定
 */
void ml7396_setAddrFilter(uint8_t *rx_filter){

    uint8_t reg_data[2];

    reg_data[0] = 0x1A;
    ml7396_regwrite(REG_ADR_ADDFIL_CNTRL, reg_data, 1);
    reg_data[0] = *(rx_filter);
    reg_data[1] = *(++rx_filter);
    ml7396_regwrite(REG_ADR_PANID_L, reg_data, 2);
    reg_data[0] = *(++rx_filter);
    reg_data[1] = *(++rx_filter);
    ml7396_regwrite(REG_ADR_SHT_ADDR0_L, reg_data, 2);
    reg_data[0] = *(++rx_filter);
    reg_data[1] = *(++rx_filter);
    ml7396_regwrite(REG_ADR_SHT_ADDR1_L, reg_data, 2);
}

/* 自機アドレスのポインタを取得
 */
uint16_t *ml7396_myaddr(void) {
    return &em_data.myaddr;
}

/* 自機PANIDのポインタを取得
 */
uint16_t *ml7396_mypanid(void) {
    return &em_data.mypanid;
}

/* ドライバの状態を取得
 */
ML7396_State ml7396_state(void) {
    return em_data.state;
}

/* 送信中のバッファを取得
 */
ML7396_Buffer *ml7396_txbuffer(void) {
    return em_data.tx;
}

/* 受信中のバッファを取得
 */
ML7396_Buffer *ml7396_rxbuffer(void) {
    return em_data.rx;
}
