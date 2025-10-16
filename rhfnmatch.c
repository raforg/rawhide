/*
rawhide - find files using pretty C expressions
https://raf.org/rawhide
https://github.com/raforg/rawhide
https://codeberg.org/raforg/rawhide

Copyright (C) 1990 Ken Stauffer, 2022-2023 raf <raf@raf.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <https://www.gnu.org/licenses/>.

20231013 raf <raf@raf.org>
*/

/*
----------------------------------------------------------------------
Copyright © 2005-2020 Rich Felker, et al.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
----------------------------------------------------------------------
*/

/*
 * An implementation of what I call the "Sea of Stars" algorithm for
 * POSIX fnmatch(). The basic idea is that we factor the pattern into
 * a head component (which we match first and can reject without ever
 * measuring the length of the string), an optional tail component
 * (which only exists if the pattern contains at least one star), and
 * an optional "sea of stars", a set of star-separated components
 * between the head and tail. After the head and tail matches have
 * been removed from the input string, the components in the "sea of
 * stars" are matched sequentially by searching for their first
 * occurrence past the end of the previous match.
 *
 * - Rich Felker, April 2012
 * - raf, October 2025
 *       Support non-utf8 bytes in pattern and target (as if latin1)
 *       Trivial/inadequate POSIX character equivalents
 *       Trivial/inadequate POSIX collating sequences
 */

#define _GNU_SOURCE /* For FNM_EXTMATCH and FNM_CASEFOLD in <fnmatch.h> */

#include <string.h>
#include <fnmatch.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

#include "rhfnmatch.h"

#define END 0
#define BRACKET -3
#define QUESTION -4
#define STAR -5

#define RHFNM_ONLY /* Exclude code that rawhide never uses (for test coverage) */

static int nextwc(wchar_t *wcp, const char *s, size_t n)
{
	mbtowc(NULL, NULL, 0);
	int l = mbtowc(wcp, s, n);
	mbtowc(NULL, NULL, 0);
	if (l < 0) l = 1, *wcp = (unsigned char)*s;
	return l;
}

static int str_next(const char *str, size_t n, size_t *step)
{
	if (!n || !*str) {
		*step = 0;
		return 0;
	}
	*step = 1;
	if (str[0] >= 128U) {
		wchar_t wc;
		*step = nextwc(&wc, str, n);
		return wc;
	}
	return str[0];
}

static int pat_next(const char *pat, size_t m, size_t *step, int flags)
{
	int esc = 0;
	if (!m || !*pat) {
		*step = 0;
		return END;
	}
	*step = 1;
	if (pat[0]=='\\' && pat[1] && !(flags & FNM_NOESCAPE)) {
		*step = 2;
		pat++;
		esc = 1;
		goto escaped;
	}
	if (pat[0]=='[') {
		size_t k = 1;
		if (k<m) if (pat[k] == '^' || pat[k] == '!') k++;
		if (k<m) if (pat[k] == ']') k++;
		for (; k<m && pat[k] && pat[k]!=']'; k++) {
			if (k+1<m && pat[k+1] && pat[k]=='[' && (pat[k+1]==':' || pat[k+1]=='.' || pat[k+1]=='=')) {
				int z = pat[k+1];
				k+=2;
				if (k<m && pat[k]) k++;
				while (k<m && pat[k] && (pat[k-1]!=z || pat[k]!=']')) k++;
				if (k==m || !pat[k]) break;
			}
		}
		if (k==m || !pat[k]) {
			*step = 1;
			return '[';
		}
		*step = k+1;
		return BRACKET;
	}
	if (pat[0] == '*')
		return STAR;
	if (pat[0] == '?')
		return QUESTION;
escaped:
	if (pat[0] >= 128U) {
		wchar_t wc;
		*step = nextwc(&wc, pat, m) + esc;
		return wc;
	}
	return pat[0];
}

static int casefold(int k)
{
	int c = towupper(k);
	return c == k ? towlower(k) : c;
}

static int match_bracket(const char *p, int k, int kfold)
{
	wchar_t wc;
	int inv = 0;
	p++;
	if (*p=='^' || *p=='!') {
		inv = 1;
		p++;
	}
	if (*p==']') {
		if (k==']') return !inv;
		p++;
	} else if (*p=='-') {
		if (k=='-') return !inv;
		p++;
	}
	wc = p[-1];
	for (; *p != ']'; p++) {
		if (p[0]=='-' && p[1]!=']') {
			wchar_t wc2;
			int l = nextwc(&wc2, p+1, 4);
			if (wc <= wc2)
				if ((unsigned)k-wc <= wc2-wc ||
				    (unsigned)kfold-wc <= wc2-wc)
					return !inv;
			p += l-1;
			continue;
		}
		if (p[0]=='[' && (p[1]==':' || p[1]=='.' || p[1]=='=')) {
			const char *p0 = p+2;
			int z = p[1];
			p+=3;
			while (p[-1]!=z || p[0]!=']') p++;
			if (z == ':' && p-1-p0 < 16) {
				char buf[16];
				memcpy(buf, p0, p-1-p0);
				buf[p-1-p0] = 0;
				if (iswctype(k, wctype(buf)) ||
				    iswctype(kfold, wctype(buf)))
					return !inv;
			}
			else if (z == '=' && *p0) { /* Note: This is woefully inadequate */
				wchar_t wc2;
				(void)nextwc(&wc2, p0, 4);
				if (wc2==k || wc2==kfold) return !inv;
			}
			else if (z == '.' && p-1-p0 < 4) { /* Note: This is woefully inadequate */
				char buf[4];
				wchar_t wbuf[4], wbuf1[2], wbuf2[2];
				memcpy(buf, p0, p-1-p0);
				buf[p-1-p0] = 0;
				int wlen = (int)mbstowcs(wbuf, buf, 3);
				if (wlen != -1)
				{
					wbuf[wlen] = 0;
					wbuf1[0] = k, wbuf1[1] = 0;
					wbuf2[0] = kfold, wbuf2[1] = 0;
					if (!wcscoll(wbuf, wbuf1) || !wcscoll(wbuf, wbuf2)) return !inv;
				}
			}
			continue;
		}
		if (*p < 128U) {
			wc = (unsigned char)*p;
		} else {
			int l = nextwc(&wc, p, 4);
			p += l-1;
		}
		if (wc==k || wc==kfold) return !inv;
	}
	return inv;
}

static int fnmatch_internal(const char *pat, size_t m, const char *str, size_t n, int flags)
{
	const char *p, *ptail, *endpat;
	const char *s, *stail, *endstr;
	size_t pinc, sinc, tailcnt=0;
	int c, k, kfold;

	/* Match up to before the first STAR */
	#ifndef RHFNM_ONLY /* Note: rawhide never sets FNM_PERIOD */
	if (flags & FNM_PERIOD) {
		if (*str == '.' && *pat != '.')
			return FNM_NOMATCH;
	}
	#endif
	for (;;) {
		switch ((c = pat_next(pat, m, &pinc, flags))) {
		case STAR:
			pat++;
			m--;
			break;
		default:
			k = str_next(str, n, &sinc);
			if (!k)
				return (c==END) ? 0 : FNM_NOMATCH;
			str += sinc;
			n -= sinc;
			kfold = flags & FNM_CASEFOLD ? casefold(k) : k;
			if (c == BRACKET) {
				if (!match_bracket(pat, k, kfold))
					return FNM_NOMATCH;
			} else if (c != QUESTION && k != c && kfold != c) {
				return FNM_NOMATCH;
			}
			pat+=pinc;
			m-=pinc;
			continue;
		}
		break;
	}

	/* Compute real pat length if it was initially unknown/-1 */
	m = strnlen(pat, m);
	endpat = pat + m;
	/* Find the last * in pat and count chars needed after it */
	for (p=ptail=pat; p<endpat; p+=pinc) {
		switch (pat_next(p, endpat-p, &pinc, flags)) {
		case STAR:
			tailcnt=0;
			ptail = p+1;
			break;
		default:
			tailcnt++;
			break;
		}
	}

	/* Compute real str length if it was initially unknown/-1 */
	n = strnlen(str, n);
	endstr = str + n;
	if (n < tailcnt) return FNM_NOMATCH;

	/* Find the final tailcnt chars of str, accounting for UTF-8 */
	#define b1(c, mask, bits) (c & mask) == bits
	#define bn(c) b1(c, 0xc0, 0x80)
	for (s=endstr; s>str && tailcnt; tailcnt--) {
		if (s[-1] < 128U || MB_CUR_MAX==1) s--;
		else if (s>str+1 && bn(s[-1]) && b1(s[-2], 0xe0, 0xc0)) s-=2;
		else if (s>str+2 && bn(s[-1]) && bn(s[-2]) && b1(s[-3], 0xf0, 0xe0)) s-=3;
		else if (s>str+3 && bn(s[-1]) && bn(s[-2]) && bn(s[-3]) && b1(s[-4], 0xf8, 0xf0)) s-=4;
		else s--; /* Non-UTF-8 byte treated as Latin1 */
	}
	#undef b1
	#undef bn
	if (tailcnt) return FNM_NOMATCH;
	stail = s;

	/* Check that the pat and str tails match */
	p = ptail;
	for (;;) {
		c = pat_next(p, endpat-p, &pinc, flags);
		p += pinc;
		if (!(k = str_next(s, endstr-s, &sinc))) {
			if (c != END) return FNM_NOMATCH;
			break;
		}
		s += sinc;
		kfold = flags & FNM_CASEFOLD ? casefold(k) : k;
		if (c == BRACKET) {
			if (!match_bracket(p-pinc, k, kfold))
				return FNM_NOMATCH;
		} else if (c != QUESTION && k != c && kfold != c) {
			return FNM_NOMATCH;
		}
	}

	/* We're all done with the tails now, so throw them out */
	endstr = stail;
	endpat = ptail;

	/* Match pattern components until there are none left */
	while (pat<endpat) {
		p = pat;
		s = str;
		for (;;) {
			c = pat_next(p, endpat-p, &pinc, flags);
			p += pinc;
			/* Encountering * completes/commits a component */
			if (c == STAR) {
				pat = p;
				str = s;
				break;
			}
			k = str_next(s, endstr-s, &sinc);
			if (!k)
				return FNM_NOMATCH;
			kfold = flags & FNM_CASEFOLD ? casefold(k) : k;
			if (c == BRACKET) {
				if (!match_bracket(p-pinc, k, kfold))
					break;
			} else if (c != QUESTION && k != c && kfold != c) {
				break;
			}
			s += sinc;
		}
		if (c == STAR) continue;
		/* If we failed, advance str, by 1 char if it's a valid char,
		 * or by 1 non-utf8 byte treated as latin1 otherwise. */
		str_next(str, endstr-str, &sinc);
		str += sinc;
	}

	return 0;
}

int rhfnmatch(const char *pat, const char *str, int flags)
{
	const char *s, *p;
	size_t inc;
	int c;
	if (flags & FNM_PATHNAME) for (;;) {
		for (s=str; *s && *s!='/'; s++);
		for (p=pat; (c=pat_next(p, -1, &inc, flags))!=END && c!='/'; p+=inc);
		if (c!=*s && (!*s || !(flags & FNM_LEADING_DIR)))
			return FNM_NOMATCH;
		if (fnmatch_internal(pat, p-pat, str, s-str, flags))
			return FNM_NOMATCH;
		if (!c) return 0;
		#ifndef RHFNM_ONLY /* Note: In rawhide, *s is always '\0' , never '/', so c is always END==0 */
		str = s+1;
		pat = p+inc;
		#endif
	#ifndef RHFNM_ONLY /* Note: rawhide never sets FNM_LEADING_DIR */
	} else if (flags & FNM_LEADING_DIR) {
		for (s=str; *s; s++) {
			if (*s != '/') continue;
			if (!fnmatch_internal(pat, -1, str, s-str, flags))
				return 0;
		}
	#endif
	}
	return fnmatch_internal(pat, -1, str, -1, flags);
}
