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
* 20231013 raf <raf@raf.org>
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

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wctype.h>
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
	int k;
	const char *end = (srcsz == -1) ? strchr(src, '\0') : src + srcsz;

	for (i = 0; (srcsz == -1) ? src[i] : i < srcsz; ++i)
	{
		if (src[i] == '\0')
			pos += ssnprintf(dst + pos, dstsz - pos, "\\0");
		else if ((e = strchr(escape + j, src[i])))
			pos += ssnprintf(dst + pos, dstsz - pos, "\\%c", (escaped + j)[e - (escape + j)]);
		else if ((opts & CESCAPE_QUOTES) && src[i] == '"')
			pos += ssnprintf(dst + pos, dstsz - pos, "\\\"");
		else if ((opts & CESCAPE_BIN) == CESCAPE_BIN && (src[i] & 0x80))
			pos += ssnprintf(dst + pos, dstsz - pos, "\\x%02x", (unsigned char)src[i]);
		else
		{
			wchar_t wc;
			int num_bytes_consumed = mbtowc(&wc, src + i, end - (src + i));
			mbtowc(NULL, NULL, 0);
			if (num_bytes_consumed == -1) /* "Invalid" byte sequence */
				pos += ssnprintf(dst + pos, dstsz - pos, (j) ? "\\u00%02x" : (opts & CESCAPE_HEX) ? "\\x%02x" : "\\%03o", (unsigned char)src[i]);
			else if (!is_wprint(wc)) /* Non-printable/control character */
			{
				if (j)
				{
					if ((unsigned int)wc > 0xffff) /* Need UTF-16 surrogate pair */
					{
						wchar_t u = wc - 0x10000;
						wchar_t hi = ((u >> 10) & 0x3ff) + 0xd800;
						wchar_t lo = (u & 0x3ff) + 0xdc00;
						pos += ssnprintf(dst + pos, dstsz - pos, "\\u%04x\\u%04x", (unsigned int)hi, (unsigned int)lo);
					}
					else
						pos += ssnprintf(dst + pos, dstsz - pos, "\\u%04x", (unsigned int)wc);
				}
				else
					for (k = 0; k < num_bytes_consumed; ++k)
						pos += ssnprintf(dst + pos, dstsz - pos, (opts & CESCAPE_HEX) ? "\\x%02x" : "\\%03o", (unsigned char)src[i + k]);

				i += num_bytes_consumed - 1;
			}
			else /* Printable characters */
			{
				pos += ssnprintf(dst + pos, dstsz - pos, "%.*s", num_bytes_consumed, src + i);
				i += num_bytes_consumed - 1;
			}
		}
	}

	return pos;
}

/*

char *defuse_tty(char *buf, size_t bufsz, const char *src, size_t srcbytes);

Copy src to buf, replacing non-printable characters (i.e., C0/C1 control
characters) with ? to prevent terminal escape injection. bufsz is the size
of the destination buffer (buf). No more than that many bytes will be
written to buf. srcbytes is the number of bytes to copy from src. If it is
-1, then src is treated as a nul-terminated string to determine the number
of bytes.

*/

char *defuse_tty(char *buf, size_t bufsz, const char *src, ssize_t srcbytes)
{
	const char *end = (srcbytes == -1) ? strchr(src, '\0') : src + srcbytes;
	char *s = buf;

	while ((s - buf) < (bufsz - 1) && src < end && *src)
	{
		wchar_t wc;
		int num_bytes_consumed = mbtowc(&wc, src, end - src);
		mbtowc(NULL, NULL, 0);
		if (num_bytes_consumed == -1) /* "Invalid" byte sequence */
			*s++ = '?', ++src;
		else if (!is_wprint(wc)) /* Non-printable/control character */
			*s++ = '?', src += num_bytes_consumed;
		else  /* Printable character */
			if ((s + num_bytes_consumed - buf) < (bufsz - 1))
				while (num_bytes_consumed--)
					*s++ = *src++;
			else
				break;
	}

	*s = '\0';

	return buf;
}

/* Sanitise text for errors */

static const char *sanitise(char *buf, size_t sz, const char *str)
{
	if (!attr.ttyerr)
		return str;

	return defuse_tty(buf, sz, str, -1);
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

/* The same, for when it's not nul-terminated (length byte) */

const char *oklen(const char *str, size_t len)
{
	static char buf[256];

	if (!attr.ttyerr)
		return strlcpy(buf, str, len + 1), buf;

	return defuse_tty(buf, 1024, str, len);
}

/* vi:set ts=4 sw=4: */
