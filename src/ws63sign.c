/*
  ws63sign - Hisense WS63 Machine Code Signing Tool
  Copyright (C) 2025  Gong Zhile

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

#include "ws63sign.h"
#include "io.h"
#include <config.h>
#include <errno.h>

#include <argp.h>
#include <stdio.h>

const char *argp_program_version =
	"ws63sign " PACKAGE_VERSION "\n"
  "Copyright (C) 2024-2025 Gong Zhile\n"
  "This is free software; see the source for copying conditions.  There is NO\n"
  "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n";
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

static char doc[] = "ws63sign -- ws63 machine code signing utility"
	" for Hisilicon WS63";
static char args_doc[] = "INPUT [OUTPUT]";

static struct argp_option options[] = {
	{"out", 'o', 0, 0, "specify the output file", 0},
	{0},
};

static struct args {
	FILE		*inf;
	FILE		*outf;
} arguments = { 0 };

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct args *args = state->input;

	switch (key) {
	case ARGP_KEY_ARG:
		if (!args->inf)
			args->inf = (arg[1] == 0 && arg[0] == '-') ? stdin
				: fopen(arg, "r");
		else
			argp_usage(state);
		break;
	case 'o':
		args->outf = fopen(arg, "w");
	case ARGP_KEY_END:
		if (!args->inf)
			argp_usage(state);
		break;
	}
	return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc,
	NULL, NULL, NULL };

int main(int argc, char **argv)
{
	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	if (!arguments.inf) {
		perror("failed top open input file");
		return 1;
	}

	if (!arguments.outf) {
		if (errno) goto checkoutf; /* Already opened but with error. */

		arguments.outf = fopen("a.signed", "w");
	checkoutf:
		if (!arguments.outf && errno) {
			perror("failed to open output file");
			return 1;
		}
	}

	struct ws63sign_ctx ctx = { 0 };
	ws63sign_init(&ctx);

	unsigned char buf[4096] = { 0 };
	size_t read_len = 0;

	checked_fwrite(ctx.buf, 1, sizeof(ctx.buf), arguments.outf);

	while ((read_len = fread(buf, 1, sizeof(buf), arguments.inf)) > 0) {
		ws63sign_feed(&ctx, buf, read_len);

		checked_fwrite(buf, 1, read_len, arguments.outf);
	}

	size_t padding = ws63sign_finalize(&ctx);
	unsigned char nil128[16] = { 0 };
	checked_fwrite(nil128, 1, padding, arguments.outf);

	fseek(arguments.outf, 0, SEEK_SET);
	checked_fwrite(ctx.buf, 1, sizeof(ctx.buf), arguments.outf);

	return 0;
}
