/*
  ws63fwpkg - Hisense WS63 Flash Firmware Manipulation Tool
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

#include <config.h>

#include "baud.h"
#include "ws63defs.h"
#include "ymodem.h"
#include "fwpkg.h"
#include "io.h"

#include <endian.h>
#include <math.h>
#include <stdlib.h>
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
const char *argp_program_version =
	"ws63fwpkg " PACKAGE_VERSION "\n"
  "Copyright (C) 2024-2025 Gong Zhile\n"
  "This is free software; see the source for copying conditions.  There is NO\n"
  "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n";
const char	*argp_program_bug_address = PACKAGE_BUGREPORT;

static char doc[] = "ws63fwpkg -- firmware manipulation utility"
	" for Hisilicon WS63";

static char args_doc[] =
	"--inject FWPKG [BIN@ADDR...]";

static struct argp_option options[] = {
	{"inject", 'i', 0, 0,
	 "inject bin files to a fwpkg", 0},
	{"out", 'o', 0, 0,
	 "output manipulated fwpkg to a file", 0},
	{0},
};

static struct args {
	char	 verb;
	char	*args[MAX_PARTITION_CNT+3];
	int	 args_cnt;

	int	outfd;
} arguments;

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct args *args = state->input;

	switch (key) {
	case 'o':
		if (!arg)
			argp_usage(state);

		if (arg[0] == '-' && arg[1] == 0) /* stdout */
			break;

		args->outfd = open(arg, O_RDWR);
		break;
	case 'i':
		if (args->verb)
			argp_usage(state);
		args->verb = key;
		break;
	case ARGP_KEY_ARG:
		if ((args->verb == 'i' && state->arg_num >= MAX_PARTITION_CNT))
			argp_usage(state);
		args->args[state->arg_num] = arg;
		break;
	case ARGP_KEY_END:
		args->args_cnt = state->arg_num;
		if ((args->verb == 'i' && state->arg_num < 2))
			argp_usage(state);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	};

	return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc,
	NULL, NULL, NULL };

/* Main Entrance */

int verb_inject(int fd)
{
	int ret = 0;

	/* Parsing input arguments */
	struct wobj wobjs[MAX_PARTITION_CNT];

	for (int i = 1; i < arguments.args_cnt; i++) {
		char *arg_current = arguments.args[i];
		struct wobj *wobj_current = &wobjs[i-1];
		char *sep = strchr(arg_current, '@');

		if (!sep) {
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

	FILE *infw = fopen(arguments.args[0], "r");
	FILE *outfw = (fd == STDOUT_FILENO) ? stdout : fdopen(fd, "rw");

	if (!infw || !outfw) {
		perror("fopen");
		return EXIT_FAILURE;
	}

	struct fwpkg_header *header = fwpkg_read_header(infw);
	if (!header) return EXIT_FAILURE;

	struct fwpkg_bin_info *bins = fwpkg_read_bin_infos(infw, header);
	if (!bins) return EXIT_FAILURE;

	uint8_t buf[sizeof(*header)
		    + sizeof(struct fwpkg_bin_info)
		    * MAX_PARTITION_CNT];

	memcpy(buf, header, sizeof(*header));
	memcpy(buf + sizeof(*header), bins, sizeof(*bins) * header->cnt);

	fseek(infw, 0, SEEK_END);
	ssize_t infwlen = ftell(infw);
	ssize_t outfwlen = infwlen + sizeof(*bins) * (arguments.args_cnt - 1);
	struct fwpkg_header *new_header = (void *) buf;

	for (int i = 0; i < header->cnt; i++) {
		struct fwpkg_bin_info *ob = (void *) buf + sizeof(*header)
			+ sizeof(*bins) * i;

		/* add new bin header size to the offset */
		ob->offset += sizeof(*bins) * (arguments.args_cnt - 1);
	}

	struct fwpkg_bin_info *nb;
	for (int i = 0; i < arguments.args_cnt - 1; i++) {
		nb = (void *) buf + sizeof(*header)
			+ sizeof(*bins) * new_header->cnt;

		nb->type_2 = 1;
		nb->length = wobjs[i].length;
		nb->burn_addr = wobjs[i].addr;
		nb->burn_size = wobjs[i].length;

		strncpy(nb->name, basename(wobjs[i].name), sizeof(nb->name)-1);

		nb->offset = outfwlen;
		outfwlen += nb->length;

		new_header->cnt++;
	}

	new_header->crc = crc16_xmodem(buf+6,
				       sizeof(struct fwpkg_bin_info)
				       * new_header->cnt + 6);

	checked_fwrite(buf, sizeof(*header), 1, outfw);
	checked_fwrite(buf + sizeof(*header), sizeof(*nb), new_header->cnt, outfw);

        struct fwpkg_bin_info *fb = (void *) buf + sizeof(*header);
	for (size_t i = fb->offset;
	     i < (sizeof(*header) + sizeof(*nb) * new_header->cnt);
	     i++) {
		checked_fwrite("", 1, 1, outfw);
	}

	if (copy_part(infw, outfw, bins[0].offset, infwlen - bins[0].offset))
		return 1;

	for (int i = 0; i < arguments.args_cnt - 1; i++) {
		struct wobj *curobj = &wobjs[i];
		FILE *binf = fopen(curobj->name, "r");
		if (copy_part(binf, outfw, 0, curobj->length))
			return 1;
		fclose(binf);
	}

	fclose(infw);
	fclose(outfw);
	return ret;
}

int main(int argc, char **argv)
{
	int fd = -1, ret;
	arguments.verb = 0;
	arguments.outfd = STDOUT_FILENO;

	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	if (!arguments.verb) {
		argp_help(&argp, stderr, ARGP_HELP_SHORT_USAGE, "ws63fwpkg");
		return EXIT_FAILURE;
	}

	if (arguments.outfd <= 0) {
		perror("failed to open ouput fwpkg file");
		return EXIT_FAILURE;
	}

	ret = EXIT_SUCCESS;
	switch(arguments.verb) {
	case 'i':
		ret = verb_inject(arguments.outfd);
		break;
	default:
		argp_help(&argp, stderr, ARGP_HELP_SHORT_USAGE, PACKAGE_NAME);
		return EXIT_FAILURE;
	}

	return ret;
}
