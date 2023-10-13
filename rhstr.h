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

#ifndef RAWHIDE_RHSTR_H
#define RAWHIDE_RHSTR_H

#include <unistd.h> /* For ssize_t on Debian6 */
#include <stddef.h>
#include <ctype.h>

#ifndef strlcpy /* For macOS */
size_t strlcpy(char *dst, const char *src, size_t size);
#endif
#ifndef strlcat /* For macOS */
size_t strlcat(char *dst, const char *src, size_t size);
#endif

int ssnprintf(char *str, ssize_t size, const char *format, ...);
int cescape(char *dst, ssize_t dstsize, const char *src, ssize_t srcsize, int options);
const char *ok(const char *s);
const char *ok2(const char *s);
const char *oklen(const char *s, size_t len);

#define CESCAPE_QUOTES 1
#define CESCAPE_HEX    2
#define CESCAPE_JSON   5
#define CESCAPE_BIN    10

#define isquotable(c) iscntrl((int)(unsigned char)(c))

#endif
