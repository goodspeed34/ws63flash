/*
  baud.h - Definitions of Baudrates
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

#ifndef _BAUD_H_
#define _BAUD_H_

#include <config.h>

#include <termios.h>

struct baud_ipair {
	int	baud;
	speed_t speed;
};

#define AVAIL_BAUD_N (sizeof(avail_baud_tbl)/sizeof(*avail_baud_tbl))

const static struct baud_ipair avail_baud_tbl[] = {
	{115200, B115200},
#if HAVE_DECL_B230400
	{230400, B230400},
#endif

#if HAVE_DECL_B460800
	{460800, B460800},
#endif
#if HAVE_DECL_B500000
	{500000, B500000},
#endif
#if HAVE_DECL_B576000
	{576000, B576000},
#endif
#if HAVE_DECL_B921600
	{921600, B921600},
#endif

#if HAVE_DECL_B1000000 
	{1000000, B1000000},
#endif
#if HAVE_DECL_B1152000 
	{1152000, B1152000},
#endif
#if HAVE_DECL_B1500000 
	{1500000, B1500000},
#endif
#if HAVE_DECL_B2000000
	{2000000, B2000000},
#endif
};

#endif
