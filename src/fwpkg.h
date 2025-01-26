/*
  fwpkg.h - Hisilicon Fwpkg Format Parser
  Copyright (C) 2024-2025  Gong Zhile

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

#ifndef _FWPKG_H_
#define _FWPKG_H_

#include "config.h"
#include "ymodem.h"

#include <assert.h>
#include <endian.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
     Fwpkg File Structure
  +------------------------+
  | struct fwpkg_header    |
  +------------------------+
  | struct fwpkg_bin_info  |
  +------------------------+
  |            .           |
  |            .           |
  |            .           |
  +------------------------+
  |DATA DATA DATA DATA DATA|
  +------------------------+
 */

#define MAX_PARTITION_CNT 16

struct fwpkg_header {
	uint32_t	mgc;	/* 0xdeadbeef, LE */
	uint16_t	crc;	/* from cnt to end of fwpkg_bin_info[] */
	uint16_t	cnt;	/* the number of bins */
	uint32_t	len;	/* total firmware size */
};

struct fwpkg_bin_info {
	char		name[32];
	uint32_t	offset;	/* offset from the start of fwpkg */
	uint32_t	length;
	uint32_t	burn_addr;
	uint32_t	burn_size;
	uint32_t	type_2;
};

static struct fwpkg_header inline *fwpkg_read_header(FILE *f)
{
	struct fwpkg_header *header = malloc(sizeof(*header));
	uint8_t buf[sizeof(*header)
		    + sizeof(struct fwpkg_bin_info)
		    * MAX_PARTITION_CNT];
	int ret;

	if (!header)
		return NULL;

	memset(header, 0, sizeof(*header));

	ret = fseek(f, 0, SEEK_SET);
	if (ret < 0) {
		perror("fseek");
		free(header);
		return NULL;
	}

	ret = fread(header, sizeof(*header), 1, f);
	if (ret <= 0) {
		perror("fread");
		free(header);
		return NULL;
	}

	memcpy(buf, header, sizeof(*header));

	if (header->mgc != 0xefbeaddf) {
		fprintf(stderr, "Bad fwpkg file, invalid magic number\n");
		free(header);
		return NULL;
	}

	assert(header->cnt < MAX_PARTITION_CNT);

	ret = fread(buf + sizeof(*header),
		    sizeof(struct fwpkg_bin_info),
		    header->cnt, f);

	if (ret < header->cnt) {
		perror("fread");
		free(header);
		return NULL;
	}

	ret = crc16_xmodem(buf+6, sizeof(struct fwpkg_bin_info)*header->cnt+6);
	if (ret != header->crc) {
		fprintf(stderr, "Bad fwpkg file, crc mismatch\n");
		free(header);
		return NULL;
	}

	return header;
}

static struct fwpkg_bin_info inline *
fwpkg_read_bin_infos(FILE *f, struct fwpkg_header *header)
{
	struct fwpkg_bin_info *bins = malloc(sizeof(*bins) * header->cnt);
	int ret;

	if (!bins) return NULL;

	ret = fseek(f, sizeof(*header), SEEK_SET);
	if (ret < 0) {
		perror("fseek");
		free(bins);
		return NULL;
	}

	ret = fread(bins, sizeof(*bins), header->cnt, f);
	if (ret < header->cnt) {
		perror("fread");
		free(bins);
		return NULL;
	}

	return bins;
}

#endif
