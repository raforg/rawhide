/*
* rawhide - find files using pretty C expressions
* https://raf.org/rawhide
* https://github.com/raforg/rawhide
* https://codeberg.org/raforg/rawhide
*
* Copyright (C) 1990 Ken Stauffer, 2022-2023 raf <raf@raf.org>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, see <https://www.gnu.org/licenses/>.
*
* 20230609 raf <raf@raf.org>
*/

#define _FILE_OFFSET_BITS 64 /* For 64-bit off_t on 32-bit systems (Not AIX) */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "rh.h"

/*

int error(const char *format, ...);

Outputs an error message to stderr (prefixed by the program name).
The format parameter is a printf-like format string.
Subsequent arguments must satisfy its conversions.
Returns -1.

*/

int error(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	fprintf(stderr, "%s: ", prog_name);
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
	va_end(args);

	return -1;
}

/*

int errorsys(const char *format, ...);

Outputs an error message to stderr (prefixed by the program name),
including a trailing errno string.
The format parameter is a printf-like format string.
Subsequent arguments must satisfy its conversions.
Returns -1.

*/

int errorsys(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	fprintf(stderr, "%s: ", prog_name);
	vfprintf(stderr, format, args);
	fprintf(stderr, ": %s\n", strerror(errno));
	va_end(args);

	return -1;
}

/*

void fatal(const char *format, ...);

Outputs an error message to stderr (prefixed by the program name),
then exits with a non-zero failure exit status.
The format parameter is a printf-like format string.
Subsequent arguments must satisfy its conversions.

*/

void fatal(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	fprintf(stderr, "%s: ", prog_name);
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
	va_end(args);

	exit(EXIT_FAILURE);
}

/*

void fatalsys(const char *format, ...);

Outputs an error message to stderr (prefixed by the program name)
including a trailing errno string, then exits with a non-zero
failure exit status. The format parameter is a printf-like format string.
Subsequent arguments must satisfy its conversions.

*/

void fatalsys(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	fprintf(stderr, "%s: ", prog_name);
	vfprintf(stderr, format, args);
	fprintf(stderr, ": %s\n", strerror(errno));
	va_end(args);

	exit(EXIT_FAILURE);
}

/* vi:set ts=4 sw=4: */
