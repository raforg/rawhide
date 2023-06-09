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

/*
$OpenBSD: strlcpy.c,v 1.4 1999/05/01 18:56:41 millert Exp $
$OpenBSD: strlcat.c,v 1.5 2001/01/13 16:17:24 millert Exp $
Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>

All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. The name of the author may not be used to endorse or promote products
       derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
    AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
    THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
    OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
    ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define _FILE_OFFSET_BITS 64 /* For 64-bit off_t on 32-bit systems (Not AIX) */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "rh.h"
#include "rhstr.h"

/*

size_t strlcpy(char *dst, const char *src, size_t size);

Copy src to dst (which is size bytes). The result will be no longer than
size-1 bytes, and will be nul-terminated (unless size is zero). This is like
strncpy, but it always terminates the string with a nul byte (so it's safer)
and it doesn't fill the remainder of the buffer with nul bytes (so it's
faster). Returns the length of src (If this is >= size, truncation
occurred). Use this rather than strcpy or strncpy.

*/

#ifndef strlcpy /* macOS */
size_t strlcpy(char *dst, const char *src, size_t size)
{
	const char *s = src;
	char *d = dst;
	size_t n = size;

	if (n)
		while (--n && (*d++ = *s++))
		{}

	if (n == 0)
	{
		if (size)
			*d = '\0';

		while (*s++)
		{}
	}

	return s - src - 1;
}
#endif

/*

size_t strlcat(char *dst, const char *src, size_t size);

Appends src to dst (which is size bytes). The result will be no longer than
size-1 bytes, and will be nul-terminated (unless size is zero). This is like
strncat but the last argument is the size of the buffer not the amount of
space available (so it's more intuitive and hence safer). Returns the sum of
the lengths of src and dst (If this is >= size, truncation occurred). Use
this rather than strcat or strncat.

*/

#ifndef strlcat /* macOS */
size_t strlcat(char *dst, const char *src, size_t size)
{
	const char *s = src;
	char *d = dst;
	size_t n = size;
	size_t dlen = 0;

	while (n-- && *d)
		++d;

	dlen = d - dst;

	if (++n == 0)
	{
		while (*s++)
		{}

		return dlen + s - src - 1;
	}

	for (; *s; ++s)
		if (n - 1)
			--n, *d++ = *s;

	*d = '\0';

	return dlen + s - src;
}
#endif

/*

int ssnprintf(char *str, ssize_t size, const char *format, ...);

A snprintf wrapper whose size parameter is signed so we can detect negative
size remaining and not overwrite the buffer. When that happens, return zero.

*/

int ssnprintf(char *str, ssize_t size, const char *format, ...)
{
	va_list args;
	int rc;

	if (size <= 0)
		return 0;

	va_start(args, format);
	rc = vsnprintf(str, (size_t)size, format, args);
	va_end(args);

	return rc;
}

/*

int cescape(char *dst, ssize_t dstsz, const char *src, ssize_t srcsz, int opts);

Copy src to dst using C escapes. At most dstsz bytes are written.
If srcsz is -1, src is nul-terminated. Otherwise, the given number
of bytes are processed. The opts bitmask can contain CESCAPE_QUOTES (to quote "),
CESCAPE_HEX (to use hex not octal), CESCAPE_JSON (for \uxxxx ctrls and no \a\v),
and CESCAPE_BIN (to hex all 8bit).

*/

int cescape(char *dst, ssize_t dstsz, const char *src, ssize_t srcsz, int opts)
{
	static char *escape  = "\013\007\010\011\012\014\015\\";
	static char *escaped = "vabtnfr\\";
	char *e;
	int i, pos = 0;
	int j = (opts & CESCAPE_JSON) == CESCAPE_JSON ? 2 : 0;

	for (i = 0; (srcsz == -1) ? src[i] : i < srcsz; ++i)
	{
		if (src[i] == '\0')
			pos += ssnprintf(dst + pos, dstsz - pos, "\\0");
		else if ((e = strchr(escape + j, src[i])))
			pos += ssnprintf(dst + pos, dstsz - pos, "\\%c", (escaped + j)[e - (escape + j)]);
		else if (isquotable(src[i]))
			pos += ssnprintf(dst + pos, dstsz - pos, (j) ? "\\u00%02x" : (opts & CESCAPE_HEX) ? "\\x%02x" : "\\%03o", (unsigned char)src[i]);
		else if ((opts & CESCAPE_QUOTES) && src[i] == '"')
			pos += ssnprintf(dst + pos, dstsz - pos, "\\\"");
		else if ((opts & CESCAPE_BIN) == CESCAPE_BIN && (src[i] & 0x80))
			pos += ssnprintf(dst + pos, dstsz - pos, "\\x%02x", (unsigned char)src[i]);
		else
			pos += ssnprintf(dst + pos, dstsz - pos, "%c", (unsigned char)src[i]);
	}

	return pos;
}

/* Sanitise text for errors */

static const char *sanitise(char *buf, size_t sz, const char *str)
{
	char *s;

	if (!attr.ttyerr)
		return str;

	for (s = buf; (s - buf) < (sz - 1) && *str; ++str)
		*s++ = isquotable(*str) ? '?' : *str;

	*s = '\0';

	return buf;
}

const char *ok(const char *str)
{
	static char buf[8192];

	return sanitise(buf, 8192, str);
}

/* The same, to allow two uses in one statement */

const char *ok2(const char *str)
{
	static char buf[8192];

	return sanitise(buf, 8192, str);
}

/* The same, for when it's not nul-terminated */

const char *oklen(const char *str, size_t len)
{
	static char buf[1024];
	char *s;

	for (s = buf; len > 0; ++str, --len)
		*s++ = (attr.ttyerr && isquotable(*str)) ? '?' : *str;

	*s = '\0';

	return buf;
}

/* vi:set ts=4 sw=4: */
