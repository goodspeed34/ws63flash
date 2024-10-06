/*
  ymodem.h - X/Y Modem Implimentation for Image Xfer
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

#ifndef _YMODEM_H_
#define _YMODEM_H_

#include "config.h"

#include <ctype.h>
#include <endian.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

static const unsigned short crc16_tbl[256]= {
	0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
	0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef,
	0x1231,0x0210,0x3273,0x2252,0x52b5,0x4294,0x72f7,0x62d6,
	0x9339,0x8318,0xb37b,0xa35a,0xd3bd,0xc39c,0xf3ff,0xe3de,
	0x2462,0x3443,0x0420,0x1401,0x64e6,0x74c7,0x44a4,0x5485,
	0xa56a,0xb54b,0x8528,0x9509,0xe5ee,0xf5cf,0xc5ac,0xd58d,
	0x3653,0x2672,0x1611,0x0630,0x76d7,0x66f6,0x5695,0x46b4,
	0xb75b,0xa77a,0x9719,0x8738,0xf7df,0xe7fe,0xd79d,0xc7bc,
	0x48c4,0x58e5,0x6886,0x78a7,0x0840,0x1861,0x2802,0x3823,
	0xc9cc,0xd9ed,0xe98e,0xf9af,0x8948,0x9969,0xa90a,0xb92b,
	0x5af5,0x4ad4,0x7ab7,0x6a96,0x1a71,0x0a50,0x3a33,0x2a12,
	0xdbfd,0xcbdc,0xfbbf,0xeb9e,0x9b79,0x8b58,0xbb3b,0xab1a,
	0x6ca6,0x7c87,0x4ce4,0x5cc5,0x2c22,0x3c03,0x0c60,0x1c41,
	0xedae,0xfd8f,0xcdec,0xddcd,0xad2a,0xbd0b,0x8d68,0x9d49,
	0x7e97,0x6eb6,0x5ed5,0x4ef4,0x3e13,0x2e32,0x1e51,0x0e70,
	0xff9f,0xefbe,0xdfdd,0xcffc,0xbf1b,0xaf3a,0x9f59,0x8f78,
	0x9188,0x81a9,0xb1ca,0xa1eb,0xd10c,0xc12d,0xf14e,0xe16f,
	0x1080,0x00a1,0x30c2,0x20e3,0x5004,0x4025,0x7046,0x6067,
	0x83b9,0x9398,0xa3fb,0xb3da,0xc33d,0xd31c,0xe37f,0xf35e,
	0x02b1,0x1290,0x22f3,0x32d2,0x4235,0x5214,0x6277,0x7256,
	0xb5ea,0xa5cb,0x95a8,0x8589,0xf56e,0xe54f,0xd52c,0xc50d,
	0x34e2,0x24c3,0x14a0,0x0481,0x7466,0x6447,0x5424,0x4405,
	0xa7db,0xb7fa,0x8799,0x97b8,0xe75f,0xf77e,0xc71d,0xd73c,
	0x26d3,0x36f2,0x0691,0x16b0,0x6657,0x7676,0x4615,0x5634,
	0xd94c,0xc96d,0xf90e,0xe92f,0x99c8,0x89e9,0xb98a,0xa9ab,
	0x5844,0x4865,0x7806,0x6827,0x18c0,0x08e1,0x3882,0x28a3,
	0xcb7d,0xdb5c,0xeb3f,0xfb1e,0x8bf9,0x9bd8,0xabbb,0xbb9a,
	0x4a75,0x5a54,0x6a37,0x7a16,0x0af1,0x1ad0,0x2ab3,0x3a92,
	0xfd2e,0xed0f,0xdd6c,0xcd4d,0xbdaa,0xad8b,0x9de8,0x8dc9,
	0x7c26,0x6c07,0x5c64,0x4c45,0x3ca2,0x2c83,0x1ce0,0x0cc1,
	0xef1f,0xff3e,0xcf5d,0xdf7c,0xaf9b,0xbfba,0x8fd9,0x9ff8,
	0x6e17,0x7e36,0x4e55,0x5e74,0x2e93,0x3eb2,0x0ed1,0x1ef0
};

static inline uint16_t crc16_xmodem(const void *ptr, int len)
{
	int counter;
	uint16_t crc = 0;
	const unsigned char *buf = (const unsigned char *) ptr;
	for(counter = 0; counter < len; counter++)
		crc = (crc<<8) ^
			crc16_tbl[((crc>>8)
				   ^ * ((unsigned char *)(buf++)))&0x00FF];
	return crc;
}

#define YMODEM_C_TIMEOUT	5
#define YMODEM_ACK_TIMEOUT	1.5
#define YMODEM_XMIT_TIMEOUT	10

/* Control Characters */
#define SOH 0x01
#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define C   'C'

static inline int ymodem_wait_ack(int fd)
{
	time_t t0 = time(NULL);
	char cc;
	int ret;

	while (1) {
		ret = read(fd, &cc, 1);

		if (ret == 0) continue;
		if (ret < 0) {
			perror("ymodem_wait_ack");
			return -errno;
		}

		if (cc == ACK)
			return 0;

		if (cc == NAK)
			return EAGAIN;

		if (difftime(time(NULL), t0) > YMODEM_ACK_TIMEOUT)
			return EAGAIN;
	}
}

static inline int ymodem_blk_xmit(int fd, const uint8_t *blk, size_t len)
{
	size_t written = 0;
	int ret;

	while (written < len) {
		ret = write(fd, blk + written, len - written);
		if (ret < 0) {
			perror("ymodem_blk_xmit");
			return -errno;
		}
		written += ret;
	}

	/* for (int i = 0; i < len; i++) */
		/* printf("%02X ", blk[i]); */
	/* printf("\n"); */

	return 0;
}

static inline int ymodem_blk_timed_xmit(int fd, const uint8_t *blk, size_t len)
{
	time_t t0 = time(NULL);
	int ret;
blk_xmit:
	if (difftime(time(NULL), t0) > YMODEM_XMIT_TIMEOUT) {
		errno = ETIMEDOUT;
		perror("ymodem_blk_timed_xmit");
		return -errno;
	}

	ret = ymodem_blk_xmit(fd, blk, len);	
	if (ret < 0)
		return ret;

	ret = ymodem_wait_ack(fd);
	if (ret < 0)
		return ret;
	if (ret > 0)
		goto blk_xmit;

	return 0;
}

static inline int
ymodem_xfer(int fd, FILE *f, const char *fn, size_t len, int verbose)
{
        int total_blk = ceil(len/1024.0), i_blk = 0;
	uint8_t blkbuf[1029], cc, occ;
	size_t last_blk = (last_blk = len - ((len/1024)*1024)) ? last_blk : 1024;
	time_t t0 = time(NULL);
	int ret;

	/* Waiting for C */
	if (verbose) printf("< ");	
	while (1) {
		ret = read(fd, &cc, 1);

		if (ret < 0) {
			perror("read");
			return -errno;
		}

		if (ret == 0 && difftime(time(NULL), t0) > YMODEM_C_TIMEOUT) {
			errno = ETIMEDOUT;
			perror("read");
			return -errno;
		}

		if (ret > 0 && cc == C) break;

		if (verbose && isascii(cc) && isprint(cc))
			printf("%c", (occ = cc));
		else if (verbose && cc == '\n' && occ != '\n')
			printf("%c< ", occ = cc);
	};
	if (verbose && occ != '\n') printf("\n");

	/* Display current progress */

	printf("Xfer: %s (0x%zx)", fn, len);
	fflush(stdout);

	/* Block 0: File Info */

	memset(blkbuf, 0, sizeof(blkbuf));
	blkbuf[0] = SOH; blkbuf[1] = 0x00; blkbuf[2] = 0xff;

	strncpy((char *) blkbuf+3, fn, 128);

	snprintf((char *) blkbuf+3+strlen(fn)+ 1,
		 127 - strlen(fn), "0x%zx", len);

	*((uint16_t *) (blkbuf + 131)) = htobe16(crc16_xmodem(blkbuf+3, 128));

	ret = ymodem_blk_timed_xmit(fd, blkbuf, 128+5);	
	if (ret < 0)
		return ret;

	printf(",");
	fflush(stdout);

	i_blk++;

	/* Data Blocks: File Data */

	while (i_blk < total_blk+1) {
		memset(blkbuf, 0, sizeof(blkbuf));

		blkbuf[0] = STX;
		blkbuf[1] = i_blk % 0x100;
		blkbuf[2] = 0xff - (blkbuf[1]);

		ret = fread(blkbuf+3, 1, (i_blk == total_blk) ? last_blk : 1024, f);
		if (i_blk < total_blk && ret < 1024) {
			printf("\nERROR: No data left on file.");
			fflush(stdout);
			return EXIT_FAILURE;
		}

		*((uint16_t *) (blkbuf + 1027)) = \
			htobe16(crc16_xmodem(blkbuf+3, 1024));

		ret = ymodem_blk_timed_xmit(fd, blkbuf, 1029);	
		if (ret < 0)
			return ret;

		printf(".");
		fflush(stdout);

		i_blk++;
	}

	/* EOT */
	char eot = EOT;

eot_xmit:
	ret = write(fd, &eot, 1);
	if (ret <= 0) {
		perror("eot_xmit");
		return -errno;
	}

	ret = ymodem_wait_ack(fd);
	if (ret < 0)
		return ret;
	if (ret > 0)
		goto eot_xmit;

	printf(",");
	fflush(stdout);

	/* Block 0: Finish Xmit */

	memset(blkbuf, 0, sizeof(blkbuf));
	blkbuf[0] = SOH; blkbuf[1] = 0x00; blkbuf[2] = 0xff;
	*((uint16_t *) (blkbuf + 131)) = htobe16(crc16_xmodem(blkbuf+3, 128));

	ret = ymodem_blk_timed_xmit(fd, blkbuf, 128+5);	
	if (ret < 0)
		return ret;

	printf(",\n");
	return EXIT_SUCCESS;
}

#endif /* _YMODEM_H_ */
