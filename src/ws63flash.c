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

#include "baud.h"
#include "ws63defs.h"
#include "ymodem.h"
#include "fwpkg.h"
#include "io.h"

#include <endian.h>
#include <math.h>
#include <stddef.h>
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
static char	args_doc[] =
	"--flash TTY FWPKG [BIN...]\n"
	"--write TTY LOADERBOOT [BIN@ADDR...]\n"
	"--erase TTY FWPKG";

static struct argp_option options[] = {
	{"flash", 'f', 0, 0,
	 "flash a fwpkg file", 0},
	{"erase", 'e', 0, 0,
	 "erase the flash memory", 0},
	{"write", 'w', 0, 0,
	 "write bin(s) to specific address", 0},

	{"baud", 'b', "BAUDRATE", 0,
	 "set the flashing serial baudrate", 1},
	{"late-baud", 1, 0, 0,
	 "set the baudrate after loaderBoot (Available on Hi3863)", 1},
	{"verbose", 'v', 0, 0,
	 "verbosely output the interactions", 1},
	{0},
};

static struct args {
	char	 verb;
	char	*args[MAX_PARTITION_CNT+3];
	int	 args_cnt;
	int	 verbose;
	int	 baud;
	int	 late_baud;
} arguments;

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct args *args = state->input;

	switch (key) {
	case 1:
		args->late_baud = 1;
		break;
	case 'b':
		if (!arg)
			argp_usage(state);

		int baud = atoi(arg), speed_found = 0;

		for (int i = 0; i < AVAIL_BAUD_N; i++) {
			struct baud_ipair baud_pair = avail_baud_tbl[i];
			if (baud_pair.baud != baud) continue;
			speed_found = 1;
			break;
		}

		if (!speed_found) {
			fprintf(stderr,
				"Target baud %d not found,"
				" maybe not supported by OS?\n"
				"Available Baud: ", baud);
			for (int i = 0; i < AVAIL_BAUD_N; i++)
				printf("%d ", avail_baud_tbl[i].baud);
			putchar('\n');
			exit(EXIT_FAILURE);
		}

		args->baud = baud;
		break;
	case 'v':
		args->verbose++;
		break;
	case 'f':
	case 'w':
	case 'e':
		if (args->verb)
			argp_usage(state); /* Verb already set, Bail out */
		args->verb = key;
		break;
	case ARGP_KEY_ARG:
		if ((args->verb == 'f' && state->arg_num >= MAX_PARTITION_CNT)
		    || (args->verb == 'w' && state->arg_num >= MAX_PARTITION_CNT)
		    || (args->verb == 'e' && state->arg_num > 1))
			argp_usage(state);
		args->args[state->arg_num] = arg;
		break;
	case ARGP_KEY_END:
		args->args_cnt = state->arg_num;
		if ((args->verb == 'f' && state->arg_num < 2)
		    || (args->verb == 'w' && state->arg_num < 3)
		    || (args->verb == 'e' && state->arg_num < 2))
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

static inline int ws63_poll_reset(int fd, int verbose)
{
	char	buf[32] = { 0 };
	int	ret	= 0;
	time_t	t0	= time(NULL), t1 = t0;

	while (t1 - t0 < RESET_TIMEOUT) {
		t1 = time(NULL);

		ret = ws63_send_cmddef(fd, WS63E_FLASHINFO[CMD_RST],
				       arguments.verbose);
		if (ret < 0) return ret;

		ret = read(fd, buf, 32);
		if (ret < 0) return -errno;

		if (verbose)
			for (int i = 0; i < ret; i++) {
				if (isascii(buf[i]) && isprint(buf[i]))
					printf("%c", buf[i]);
				else
					printf("%02X ", (unsigned char) buf[i]);
			}

		if (ret > 0 && (memmem(buf, 32, "Reset", 5)
				|| memmem(buf, 32, "reset", 5)))
			return 0;
	}

	return -ETIMEDOUT;
}

int verb_flash(int fd) {
	time_t t0;
	int ret;

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

	/* Handshake to enter YModem Mode */

	printf("Waiting for device reset...\n");
	t0 = time(NULL);
	while (1) {
		struct cmddef handshake = WS63E_FLASHINFO[CMD_HANDSHAKE];
		uint8_t buf[32];
		int len;

		if (!arguments.late_baud && arguments.baud != 115200)
			*((uint32_t *) &handshake.dat) = htole32(arguments.baud);

		if (ws63_send_cmddef(fd, handshake,
				     (arguments.verbose > 2) ? 3 : 0))
			return EXIT_FAILURE;

		if (difftime(time(NULL), t0) > RESET_TIMEOUT) {
			errno = ETIMEDOUT;
			perror("Waiting for device reset");
			return 1;
		}

		len = read(fd, buf, 32);
		if (len == 0) continue;
		if (len < 0) {
			perror("read");
			return EXIT_FAILURE;
		}

		/* ACK Sequence, Command 0xE1 */
		char ack[] = "\xEF\xBE\xAD\xDE\x0C\x00\xE1\x1E\x5A\x00";
		uint8_t *needle = NULL;

		needle = memmem(buf, len, ack, sizeof(ack)-1);
		if (needle) {
			if (!arguments.late_baud && arguments.baud != 115200)
				uart_open(&fd, NULL, arguments.baud);
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

	/* Set baud if neccessary */

	if (arguments.late_baud && arguments.baud != 115200) {
		printf("Switching baud... ");
		fflush(stdout);

		struct cmddef baudcmd = WS63E_FLASHINFO[CMD_SETBAUDR];
		*((uint32_t *) baudcmd.dat) = htole32(arguments.baud);

		ret = ws63_send_cmddef(fd, baudcmd, arguments.verbose);
		if (ret < 0)
			return EXIT_FAILURE;

		uart_read_until_magic(fd, arguments.verbose);
		uart_open(&fd, NULL, arguments.baud);

		printf("%d\n", arguments.baud);
		uart_read_until_magic(fd, arguments.verbose);
	}

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

		/*
		  MCU won't respond if cmd followed immediately by ymodem.
		  Put 100ms delay here to prevent stale.
		*/
		usleep(100000);
	}

	printf("Done. Reseting device...\n");
	if (ws63_poll_reset(fd, arguments.verbose) < 0)
		perror("ws63_poll_reset");

	return 0;
}

struct wobj {
	char	*name;
	size_t   length;
	size_t	 addr;
};

int verb_write(int fd) {
	time_t t0;
	FILE *fw;
	int ret;

	/* Parsing input arguments */
	struct wobj wobjs[MAX_PARTITION_CNT];

	for (int i = 1; i < arguments.args_cnt; i++) {
		char *arg_current = arguments.args[i];
		struct wobj *wobj_current = &wobjs[i-1];
		char *sep = strchr(arg_current, '@');

		if (!sep && i > 1) {
			fprintf(stderr,
				"Error: address needed for %s (HINT: %s@addr)\n",
				arg_current, arg_current);
			return EXIT_FAILURE;
		}

		wobj_current->name = arg_current;

		if (sep) {
			*sep = '\0';
			if (sscanf(sep+1, "%zx", &wobj_current->addr) != 1) {
				fprintf(stderr, "Error: invalid address %s for %s\n",
					sep+1, arg_current);
				return EXIT_FAILURE;
			}
		}

		struct stat st;

		ret = stat(wobj_current->name, &st);
		if (ret < 0) {
			perror(wobj_current->name);
			return ret;
		}

		wobj_current->length = st.st_size;
	}

	printf("+-+-------------------------------+----------+----------+-+\n");
	printf("|F|BIN NAME                       |LENGTH    |BURN ADDR |T|\n");
	printf("+-+-------------------------------+----------+----------+-+\n");
	for (int i = 0; i < arguments.args_cnt-1; i++) {
		struct wobj *wobj_current = &wobjs[i];
		char flash_flag;
		int type_2;

		flash_flag = (i == 0) ? '!' : '*';
		type_2 = (i == 0) ? 0 : 1;

		printf("|%c|%-31s|0x%08zx|0x%08zx|%d|\n",
		       flash_flag,
		       basename(wobj_current->name),
		       wobj_current->length,
		       wobj_current->addr, type_2);
	}
	printf("+-+-------------------------------+----------+----------+-+\n");

	/* Stage 1: Flash loaderboot */

	/* Handshake to enter YModem Mode */

	printf("Waiting for device reset...\n");
	t0 = time(NULL);
	while (1) {
		struct cmddef handshake = WS63E_FLASHINFO[CMD_HANDSHAKE];
		uint8_t buf[32];
		int len;

		if (!arguments.late_baud && arguments.baud != 115200)
			*((uint32_t *) &handshake.dat) = htole32(arguments.baud);

		if (ws63_send_cmddef(fd, handshake,
				     (arguments.verbose > 2) ? 3 : 0))
			return EXIT_FAILURE;

		if (difftime(time(NULL), t0) > RESET_TIMEOUT) {
			errno = ETIMEDOUT;
			perror("Waiting for device reset");
			return 1;
		}

		len = read(fd, buf, 32);
		if (len == 0) continue;
		if (len < 0) {
			perror("read");
			return EXIT_FAILURE;
		}

		/* ACK Sequence, Command 0xE1 */
		char ack[] = "\xEF\xBE\xAD\xDE\x0C\x00\xE1\x1E\x5A\x00";
		uint8_t *needle = NULL;

		needle = memmem(buf, len, ack, sizeof(ack)-1);
		if (needle) {
			if (!arguments.late_baud && arguments.baud != 115200)
				uart_open(&fd, NULL, arguments.baud);
			printf("Establishing ymodem session...\n");
			break;
		}
	}

	/* Entered YModem Mode, Xfer loaderBoot */
	fw = fopen(wobjs[0].name, "r");
	if (!fw) { perror(wobjs[0].name); return errno; }

	ret = ymodem_xfer(fd, fw,
			  basename(wobjs[0].name),
			  wobjs[0].length,
			  arguments.verbose);
	if (ret < 0)
		return EXIT_FAILURE;

	uart_read_until_magic(fd, arguments.verbose);

	/* Set baud if neccessary */

	if (arguments.late_baud && arguments.baud != 115200) {
		printf("Switching baud... ");
		fflush(stdout);

		struct cmddef baudcmd = WS63E_FLASHINFO[CMD_SETBAUDR];
		*((uint32_t *) baudcmd.dat) = htole32(arguments.baud);

		ret = ws63_send_cmddef(fd, baudcmd, arguments.verbose);
		if (ret < 0)
			return EXIT_FAILURE;

		uart_read_until_magic(fd, arguments.verbose);
		uart_open(&fd, NULL, arguments.baud);

		printf("%d\n", arguments.baud);
		uart_read_until_magic(fd, arguments.verbose);
	}

	/* Xfer other files */
	for (int i = 1; i < arguments.args_cnt-1; i++) {
		struct wobj *wobj_current = &wobjs[i];
		struct cmddef cmd = WS63E_FLASHINFO[CMD_DOWNLOADI];

		ssize_t eras_size = -1;
		eras_size = ceil(wobj_current->length/8192.0)*0x2000;

		*((uint32_t *) (cmd.dat))     = htole32(wobj_current->addr);
		*((uint32_t *) (cmd.dat + 4)) = htole32(wobj_current->length);
		*((uint32_t *) (cmd.dat + 8)) = htole32(eras_size);

		if (ws63_send_cmddef(fd, cmd, arguments.verbose) < 0)
			return EXIT_FAILURE;

		uart_read_until_magic(fd, arguments.verbose);

		fw = fopen(wobj_current->name, "r");
		if (!fw) { perror(wobj_current->name); return errno; }

		ret = ymodem_xfer(fd, fw,
				  basename(wobj_current->name),
				  wobj_current->length,
				  arguments.verbose);
		if (ret < 0)
			return EXIT_FAILURE;

		/*
		  MCU won't respond if cmd followed immediately by ymodem.
		  Put 100ms delay here to prevent stale.
		*/
		usleep(100000);
	}

	printf("Done. Reseting device...\n");
	if (ws63_poll_reset(fd, arguments.verbose) < 0)
		perror("ws63_poll_reset");
	return 0;
}

int verb_erase(int fd) {
	time_t t0;
	int ret;

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

	/* Stage 1: Flash loaderboot */

	/* Handshake to enter YModem Mode */

	printf("Waiting for device reset...\n");
	t0 = time(NULL);
	while (1) {
		struct cmddef handshake = WS63E_FLASHINFO[CMD_HANDSHAKE];
		uint8_t buf[32];
		int len;

		if (!arguments.late_baud && arguments.baud != 115200)
			*((uint32_t *) &handshake.dat) = htole32(arguments.baud);

		if (ws63_send_cmddef(fd, handshake,
				     (arguments.verbose > 2) ? 3 : 0))
			return EXIT_FAILURE;

		if (difftime(time(NULL), t0) > RESET_TIMEOUT) {
			errno = ETIMEDOUT;
			perror("Waiting for device reset");
			return 1;
		}

		len = read(fd, buf, 32);
		if (len == 0) continue;
		if (len < 0) {
			perror("read");
			return EXIT_FAILURE;
		}

		/* ACK Sequence, Command 0xE1 */
		char ack[] = "\xEF\xBE\xAD\xDE\x0C\x00\xE1\x1E\x5A\x00";
		uint8_t *needle = NULL;

		needle = memmem(buf, len, ack, sizeof(ack)-1);
		if (needle) {
			if (!arguments.late_baud && arguments.baud != 115200)
				uart_open(&fd, NULL, arguments.baud);
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

	printf("Erasing flash....\n");
	if (ws63_send_cmddef(fd, WS63E_FLASHINFO[CMD_DOWNLOADI],
			     arguments.verbose) < 0)
		return EXIT_FAILURE;
	uart_read_until_magic(fd, arguments.verbose);

	printf("Done. Reseting device...\n");
        if (ws63_poll_reset(fd, arguments.verbose) < 0)
		perror("ws63_poll_reset");
	return 0;
}

int main (int argc, char **argv)
{
	time_t t0;
	int fd = -1, ret;

	arguments.verb	  = 0;
	arguments.baud	  = 115200;
	arguments.verbose = 0;

	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	if (!arguments.verb) {
		argp_help(&argp, stderr, ARGP_HELP_SHORT_USAGE, PACKAGE_NAME);
		return EXIT_FAILURE;
	}

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

	ret = EXIT_SUCCESS;
	switch (arguments.verb) {
	case 'f':
		ret = verb_flash(fd);
		break;
	case 'w':
		ret = verb_write(fd);
		break;
	case 'e':
		ret = verb_erase(fd);
		break;
	default:
		argp_help(&argp, stderr, ARGP_HELP_SHORT_USAGE, PACKAGE_NAME);
		return EXIT_FAILURE;
	}

	/* Reset TTY to 115200 baud/s */
	uart_open(&fd, NULL, 115200);
	return ret;
}
