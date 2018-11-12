/* ieee802154.h - IEEE802.15.4e フレームコントロールフィールド ヘッダファイル
 * 								Frame control field header file
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

#ifndef _INCLUDE_IEEE802154_H
#define _INCLUDE_IEEE802154_H


/** IEEE802.15.4g MAC ヘッダのフレームコントロールフィールドビット割り付け
 */

/* bit00-02 Frame Type field */
#define IEEE802154_FC_TYPE_MASK       0x0007
#define IEEE802154_FC_TYPE_BEACON     0x0000
#define IEEE802154_FC_TYPE_DATA       0x0001
#define IEEE802154_FC_TYPE_ACK        0x0002
#define IEEE802154_FC_TYPE_CMD        0x0003
#define IEEE802154_FC_TYPE_LLDN       0x0004
#define IEEE802154_FC_TYPE_MP         0x0005

/* bit03 Security field */
#define IEEE802154_FC_SECURITY        0x0008

/* bit04 Frame Pending field */
#define IEEE802154_FC_PENDING         0x0010

/* bit05 Ack Request field */
#define IEEE802154_FC_ACKREQ          0x0020

/* bit06 PAN ID Compression field */
#define IEEE802154_FC_PANID_COMPS     0x0040

/* bit07 Reserved field */

/* bit08 Sequence Number Suppression field */
#define IEEE802154_FC_SEQ_SUPPRESS    0x0100

/* bit09 IE List Present field */
#define IEEE802154_FC_IE              0x0200

/* bit10-11 Destination Addressing Mode field */
#define IEEE802154_FC_DAMODE_MASK     0x0c00
#define IEEE802154_FC_DAMODE_NONE     0x0000
#define IEEE802154_FC_DAMODE_LLDN     0x0400
#define IEEE802154_FC_DAMODE_SHORT    0x0800
#define IEEE802154_FC_DAMODE_LONG     0x0c00

/* bit12-13 Destination Addressing Mode field */
#define IEEE802154_FC_IEEE802154_MASK 0x3000
#define IEEE802154_FC_IEEE802154_2003 0x0000
#define IEEE802154_FC_IEEE802154_2006 0x1000
#define IEEE802154_FC_IEEE802154_E    0x2000

/* bit14-15 Destination Addressing Mode field */
#define IEEE802154_FC_SAMODE_MASK     0xc000
#define IEEE802154_FC_SAMODE_NONE     0x0000
#define IEEE802154_FC_SAMODE_LLDN     0x4000
#define IEEE802154_FC_SAMODE_SHORT    0x8000
#define IEEE802154_FC_SAMODE_LONG     0xc000


#endif  /* #ifndef _INCLUDE_IEEE802154_H */
