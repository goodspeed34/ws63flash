/*
  io.h - WS63 Flash IO Functions & Macros
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

#ifndef _WS63_FLASH_IO_H_
#define _WS63_FLASH_IO_H_

#include "config.h"

#include "ws63defs.h"
#include "ymodem.h"
#include "baud.h"

#include <assert.h>
#include <ctype.h>
#include <endian.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <termio.h>
#include <errno.h>
#include <fcntl.h>

#define UART_READ_TIMEOUT 2

#define SWAP_CMD(x) (((x) << 4) | ((x) >> 4))

static inline int uart_open (int *fd, const char *ttydev, int baud)
{
	struct termios tty;

	if (*fd == -1) {
		*fd = open(ttydev, O_RDWR | O_NOCTTY | O_SYNC);
		if (*fd < 0) {
			perror("open");
			return -errno;
		}
	}

	if (tcgetattr(*fd, &tty) < 0) {
		perror("tcgetattr");
		return -errno;
	}

	speed_t speed = B115200;
	int speed_found = 0;

	for (int i = 0; i < AVAIL_BAUD_N; i++) {
		struct baud_ipair baud_pair = avail_baud_tbl[i];
		if (baud_pair.baud != baud) continue;

		speed = baud_pair.speed;
		speed_found = 1;
		break;
	}

	if (!speed_found) {
		fprintf(stderr,
			"failed to switch to baud %d,"
			"maybe your system doesn't support it?\n",
			baud);
		return -EINVAL;
	}

	cfsetospeed(&tty, speed);
	cfsetispeed(&tty, speed);

	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
	tty.c_iflag &= ~IGNBRK;
	tty.c_lflag = 0;
	tty.c_oflag = 0;
	tty.c_cc[VMIN]  = 0;
	tty.c_cc[VTIME] = 1;

	tty.c_iflag &= ~(IXON | IXOFF | IXANY);
	tty.c_cflag |= (CLOCAL | CREAD);
	tty.c_cflag &= ~(PARENB | PARODD);
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	if (tcsetattr(*fd, TCSANOW, &tty) < 0) {
		perror("tcsetattr");
		*fd = -1;
		return -errno;
	}

	return 0;
}

static inline int uart_read_until_magic(int fd, int verbose)
{
	const uint32_t magic = htole32(0xdeadbeef);
	time_t t0 = time(NULL);
	char occ;

	if (verbose) printf("< ");

	while (1)
	{
		uint8_t buf[32], *needle = NULL;
		int len;

		len = read(fd, buf, 32);
		if (len == 0) continue;
		if (len < 0) {
			perror("read");
			return -errno;
		}

		needle = (unsigned char *)
			memmem(buf, len, &magic, sizeof(magic));

		if (verbose) {
			for (int i = 0; i < len; i++) {
				if (isascii(buf[i]) && isprint(buf[i]))
					printf("%c", (occ = buf[i]));
				if (buf[i] == '\n' && occ != '\n')
					printf("%c< ", (occ = buf[i]));
                                fflush(stdout);
			}
		}

		if (needle)
			break;

		if (difftime(time(NULL), t0) > UART_READ_TIMEOUT) {
			errno = ETIMEDOUT;
			perror("uart_read_until_magic");
			return -errno;
		}
	}

	if (verbose) printf("\n");

	return 0;
}

static inline int ws63_send_cmddef(int fd, struct cmddef cmddef, int verbose)
{
	uint8_t buf[1024+12];
	size_t written = 0;
	size_t total_bytes = cmddef.len + 10;

	assert(sizeof(buf) > total_bytes);

	/* Start of Frame, 0xDEADBEEF LE */
	*((uint32_t *) buf) = htole32(0xdeadbeef);
	/* Length */
	*((uint16_t *) (buf+4)) = htole16(total_bytes);
	/* Payload */
	buf[6] = cmddef.cmd;
	buf[7] = SWAP_CMD(cmddef.cmd);
	memcpy(buf+8, cmddef.dat, cmddef.len);
	/* Checksum */
	*((uint16_t *) (buf+8+cmddef.len)) =
		htole16(crc16_xmodem(buf, total_bytes - 2));

	while (written < cmddef.len + 10) {
		int wrote = write(fd, buf + written, total_bytes - written);
		if (wrote < 0) {
			perror("write");
			return -errno;
		}
		written += wrote;
	}

	if (verbose) {
		printf("> ");
		for (int i = 0; i < total_bytes; i++)
			printf("%02x ", buf[i]);
		printf("\n");
	}

	return 0;
}

#endif /* _WS63_FLASH_IO_H_ */
