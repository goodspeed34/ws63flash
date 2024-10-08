/*
  ws63flash - Hisense WS63 Flash Tool
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

#include <config.h>

#include "ws63defs.h"
#include "ymodem.h"
#include "fwpkg.h"
#include "io.h"

#include <endian.h>
#include <math.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "libgen.h"
#include "argp.h"

/* Argument Processing */

const char	*argp_program_version	  =
  PACKAGE_STRING "\n"
  "Copyright (C) 2024 Gong Zhile\n"
  "This is free software; see the source for copying conditions.  There is NO\n"
  "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n";
const char	*argp_program_bug_address = PACKAGE_BUGREPORT;

static char	doc[]	   = PACKAGE " -- flashing utility for Hisilicon WS63";
static char	args_doc[] = "TTY FWPKG [BIN...]";

static struct argp_option options[] = {
	{"baud", 'b', "BAUDRATE", OPTION_ARG_OPTIONAL,
	 "set the flashing serial baudrate", 0},
	{"verbose", 'v', 0, 0,
	 "verbosely output the interactions", 0},
	{0},
};

static struct args {
	char	*args[MAX_PARTITION_CNT+3];
	int	 verbose;
	long	 baud;
} arguments;

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct args *args = state->input;

	switch (key) {
	case 'b':
		if (!arg)
			argp_usage(state);
		fprintf(stderr, "Warning: specifying a baudrate is buggy!\n");
		args->baud = atol(arg);
		break;
	case 'v':
		args->verbose++;
		break;
	case ARGP_KEY_ARG:
		if (state->arg_num >= MAX_PARTITION_CNT)
			argp_usage(state);
		args->args[state->arg_num] = arg;
		break;
	case ARGP_KEY_END:
		if (state->arg_num < 2)
			argp_usage(state);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc,
			    NULL, NULL, NULL };

/* Main Entrance */

#define RESET_TIMEOUT 10.0

static int bin_in_args(const char *s, struct args *args) {
	char **bin_names = args->args+2;
	int found = 0;
	int i = 0;

	if (*bin_names == NULL)
		return 1;

	for (char *bin_name = bin_names[i]
		     ; bin_name
		     ; i++, bin_name = bin_names[i])
		if (!strcmp(s, bin_name)) {
			found = 1;
			break;
		}

	return found;
};

int main (int argc, char **argv)
{
	time_t t0;
	int fd = -1, ret;

	arguments.baud	  = 115200;
	arguments.verbose = 0;

	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	/* Stage 0: Reading FWPKG file & Locate reuqired bin */

	FILE *fw = fopen(arguments.args[1], "r");
	if (!fw) {
		perror("fopen");
		return EXIT_FAILURE;
	}

	struct fwpkg_header *header = fwpkg_read_header(fw);
	if (!header) return EXIT_FAILURE;

	struct fwpkg_bin_info *bins = fwpkg_read_bin_infos(fw, header);
	if (!bins) return EXIT_FAILURE;

	struct fwpkg_bin_info *loaderboot = NULL;
	for (int i = 0; i < header->cnt; i++)
		if (bins[i].type_2 == 0)
			loaderboot = &bins[i];
	if (!loaderboot) {
		fprintf(stderr, "Required loaderboot not found in fwpkg!\n");
		return EXIT_FAILURE;
	}

	printf("+-+-------------------------------+----------+----------+-+\n");
	printf("|F|BIN NAME                       |LENGTH    |BURN ADDR |T|\n");
	printf("+-+-------------------------------+----------+----------+-+\n");
	for (int i = 0; i < header->cnt; i++) {
		char flash_flag = ' ';

		if (!bins[i].type_2)
			flash_flag = '!';
		else if (bin_in_args(bins[i].name, &arguments))
			flash_flag = '*';

		printf("|%c|%-31s|0x%08x|0x%08x|%d|\n",
		       flash_flag,
		       bins[i].name, bins[i].length,
		       bins[i].burn_addr, bins[i].type_2);
	}
	printf("+-+-------------------------------+----------+----------+-+\n");

	for (int i = 2; ; i++) {
		int found = 0;

		char *arg = arguments.args[i];
		if (!arg) break;

		for (int j = 0; j < header->cnt; j++) {
			struct fwpkg_bin_info *bin = bins + j;
			if (!strcmp(arg, bin->name)) {
				found = 1;
				break;
			}
		}

		if (!found) {
			fprintf(stderr,
				"Required bin `%s' not found in fwpkg!\n", arg);
			return EXIT_FAILURE;
		}
	}

	/* Stage 1: Flash loaderboot */

#ifdef __CYGWIN__
        int uart_len = strlen(arguments.args[0]);

        if (!strncasecmp(arguments.args[0], "COM", 3)) {
                char *uart_str = malloc(uart_len+11);
                if (!uart_str) return EXIT_FAILURE;

                int uart_num = atoi(arguments.args[0]+3);
                snprintf(uart_str, uart_len+11, "/dev/ttyS%d", uart_num-1);

                arguments.args[0] = uart_str;
        }
#endif

	/* 115200 baud, default baud for MCU */
	ret = uart_open(&fd, arguments.args[0], 115200);
	if (ret < 0 || fd < 0) return EXIT_FAILURE;

	/* Handshake to enter YModem Mode */

	printf("Waiting for device reset...\n");
	t0 = time(NULL);
	while (1) {
		uint8_t buf[32];
		int len;

		if (ws63_send_cmddef(fd, WS63E_FLASHINFO[CMD_HANDSHAKE],
				     arguments.verbose > 1))
			return EXIT_FAILURE;

		len = read(fd, buf, 32);
		if (len == 0) continue;
		if (len < 0) {
			perror("read");
			return EXIT_FAILURE;
		}

		if (difftime(time(NULL), t0) > RESET_TIMEOUT) {
			errno = ETIMEDOUT;
			perror("Waiting for device reset");
			return 1;
		}

		/* ACK Sequence, Command 0xE1 */
		char ack[] = "\xEF\xBE\xAD\xDE\x0C\x00\xE1\x1E\x5A\x00";
		uint8_t *needle = NULL;

		needle = memmem(buf, len, ack, sizeof(ack)-1);
		if (needle) {
			printf("Establishing ymodem session...\n");
			break;
		}
	}

	/* Entered YModem Mode, Xfer loaderBoot */

	if (fseek(fw, loaderboot->offset, SEEK_SET) < 0) {
		perror("fseek");
		return EXIT_FAILURE;
	}

	ret = ymodem_xfer(fd, fw,
			  loaderboot->name,
			  loaderboot->length,
			  arguments.verbose);
	if (ret < 0)
		return EXIT_FAILURE;

	uart_read_until_magic(fd, arguments.verbose);

	/* Xfer other files */

	for (int i = 0; i < header->cnt; i++) {
		struct fwpkg_bin_info *bin = &bins[i];
		if (bin->type_2 != 1) continue;

		if (!bin_in_args(bin->name, &arguments))
			continue;

		struct cmddef cmd = WS63E_FLASHINFO[CMD_DOWNLOADI];

		ssize_t eras_size = -1;
		eras_size = ceil(bin->length/8192.0)*0x2000;

		*((uint32_t *) (cmd.dat))     = htole32(bin->burn_addr);
		*((uint32_t *) (cmd.dat + 4)) = htole32(bin->length);
		*((uint32_t *) (cmd.dat + 8)) = htole32(eras_size);

		if (ws63_send_cmddef(fd, cmd, arguments.verbose) < 0)
			return EXIT_FAILURE;

		uart_read_until_magic(fd, arguments.verbose);

		if (fseek(fw, bin->offset, SEEK_SET) < 0) {
			perror("fseek");
			return EXIT_FAILURE;
		}

		ret = ymodem_xfer(fd, fw,
				  bin->name, bin->length,
				  arguments.verbose);
		if (ret < 0)
			return EXIT_FAILURE;
	}

	printf("Done. Reseting device...\n");
	ws63_send_cmddef(fd, WS63E_FLASHINFO[CMD_RST], arguments.verbose);

	uart_read_until_magic(fd, arguments.verbose);
	return EXIT_SUCCESS;
}
