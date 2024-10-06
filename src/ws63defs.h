/*
  ws63chips.h - CHIP Info & Magic Definitions
  Copyright (C) 2024  Gong Zhile

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef _WS63CHIPS_H_
#define _WS63CHIPS_H_

#include "config.h"

#include <stddef.h>
#include <stdint.h>

/*
  Hisense WS63 Flash Frame Structure:

  -> EF BE AD DE 12 00 F0 0F 00 C2 01 00 08 01 00 00 E0 64
  ^~~~~~~~~~~ ^~~~~ ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ^~~~~
  | START     | LEN | PAYLOAD (CMD,SCMD,DATA)     | CRC16/XMODEM

  CMD has one byte and SCMD is the 4 bits swaped CMD.
  All fields are little-endian.
*/

struct cmddef {
	uint8_t cmd;
	size_t	len;
	uint8_t	dat[1024];
};

enum WS63_CMDTYPE {
	CMD_HANDSHAKE = 0,
	CMD_SETBAUDR,
	CMD_DOWNLOADI,
	CMD_RST,
	CMD_END,
};

const static struct cmddef WS63E_FLASHINFO[CMD_END] = {
	[CMD_HANDSHAKE] = {
		.cmd = 0xf0,
		.dat = {0x00, 0xc2, 0x01, 0x00,  /* BAUD, 115200 */
			0x08, 0x01, 0x00, 0x00}, /* MAGC, 0x0108 */
		.len = 8,
        },
	[CMD_SETBAUDR] = {
		.cmd = 0x5a,
		.dat = {0x00, 0x10, 0x0e, 0x00,  /* BAUD, 921600  */
			0x08, 0x01, 0x00, 0x00}, /* MAGC, 0x0108 */
		.len = 8,
	},
	[CMD_DOWNLOADI] = {
		.cmd = 0xd2,
		.dat = {0xAA, 0xAA, 0xAA, 0xAA, /* ADDR  */
			0xBB, 0xBB, 0xBB, 0xBB, /* ILEN  */
			0xCC, 0xCC, 0xCC, 0xCC, /* ERAS  */
			0x00, 0xFF},		/* CONST */
		.len = 14,
	},
	[CMD_RST] = {
		.cmd = 0x87,
		.dat = {0x00, 0x00},
		.len = 2,
	},
};

struct bin_erase_info {
        char   name[32];
        size_t size;
};

const static struct bin_erase_info WS63E_ERASEINFO[] = {
        {
                .name = "root_params_sign.bin",
                .size = 0x1000,
        },
        {
                .name = "ssb_sign.bin",
                .size = 0x6000
        },
        {
                .name = "flashboot_sign.bin",
                .size = 0xd000
        },
        {
                .name = "flashboot_backup_sign.bin",
                .size = 0xd000
        },
        {
                .name = "ws63_all_nv.bin",
                .size = 0x4000,
        },
        {
                .name = "ws63_all_nv_backup.bin",
                .size = 0x4000,
        },
};

#endif	/* _WS63CHIPS_H_ */

