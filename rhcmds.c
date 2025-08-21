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

#define _GNU_SOURCE /* For FNM_EXTMATCH and FNM_CASEFOLD in <fnmatch.h> */
#define _FILE_OFFSET_BITS 64 /* For 64-bit off_t on 32-bit systems (Not AIX) */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fnmatch.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef HAVE_SYS_SYSMACROS
#include <sys/sysmacros.h>
#endif

#ifdef HAVE_SYS_MKDEV
#include <sys/mkdev.h>
#endif

#ifndef FNM_EXTMATCH
#define FNM_EXTMATCH 0
#endif

#ifdef HAVE_PCRE2
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#ifndef PCRE2_MATCH_INVALID_UTF
#define PCRE2_MATCH_INVALID_UTF 0
#endif
#endif

#ifdef HAVE_ACL
#include <sys/acl.h>
#endif

#if HAVE_LINUX_EA || HAVE_MACOS_EA
#include <sys/xattr.h>
#endif

#ifdef HAVE_FREEBSD_EA
#include <sys/extattr.h>
#endif

#ifdef HAVE_ATTR
#include <e2p/e2p.h>
#include <ext2fs/ext2_fs.h>
#endif

#include "rh.h"
#include "rhdir.h"
#include "rherr.h"
#include "rhstr.h"

/* Operators */

void c_le(llong i)     { Stack[SP - 2] = Stack[SP - 2] <= Stack[SP - 1]; SP--; }
void c_lt(llong i)     { Stack[SP - 2] = Stack[SP - 2] < Stack[SP - 1]; SP--; }
void c_ge(llong i)     { Stack[SP - 2] = Stack[SP - 2] >= Stack[SP - 1]; SP--; }
void c_gt(llong i)     { Stack[SP - 2] = Stack[SP - 2] > Stack[SP - 1]; SP--; }
void c_ne(llong i)     { Stack[SP - 2] = Stack[SP - 2] != Stack[SP - 1]; SP--; }
void c_eq(llong i)     { Stack[SP - 2] = Stack[SP - 2] == Stack[SP - 1]; SP--; }
void c_bitor(llong i)  { Stack[SP - 2] = Stack[SP - 2] | Stack[SP - 1]; SP--; }
void c_bitand(llong i) { Stack[SP - 2] = Stack[SP - 2] & Stack[SP - 1]; SP--; }
void c_bitxor(llong i) { Stack[SP - 2] = Stack[SP - 2] ^ Stack[SP - 1]; SP--; }
void c_lshift(llong i) { Stack[SP - 2] = Stack[SP - 2] << Stack[SP - 1]; SP--; }
void c_rshift(llong i) { Stack[SP - 2] = Stack[SP - 2] >> Stack[SP - 1]; SP--; }
void c_plus(llong i)   { Stack[SP - 2] = Stack[SP - 2] + Stack[SP - 1]; SP--; }
void c_minus(llong i)  { Stack[SP - 2] = Stack[SP - 2] - Stack[SP - 1]; SP--; }
void c_mul(llong i)    { Stack[SP - 2] = Stack[SP - 2] * Stack[SP - 1]; SP--; }
void check_zero(void)  { if (Stack[SP - 1] == 0) fatal("attempt to divide by zero"); }
void c_mod(llong i)    { check_zero(); Stack[SP - 2] = Stack[SP - 2] % Stack[SP - 1]; SP--; }
void c_div(llong i)    { check_zero(); Stack[SP - 2] = Stack[SP - 2] / Stack[SP - 1]; SP--; }

void c_not(llong i)      { Stack[SP - 1] = !Stack[SP - 1]; }
void c_bitnot(llong i)   { Stack[SP - 1] = ~Stack[SP - 1]; }
void c_uniminus(llong i) { Stack[SP - 1] = -Stack[SP - 1]; }

void c_qm(llong i)    { PC = (Stack[SP - 1]) ? PC : i; SP--; }
void c_colon(llong i) { PC = i; }

/* Functions */

void c_func(llong i)
{
	Stack[SP++] = PC;
	Stack[SP++] = FP;
	PC = i;
	FP = SP - (Program[PC].value + 2);
}

void c_return(llong i)
{
	PC = Stack[SP - 3];
	FP = Stack[SP - 2];
	Stack[SP - (3 + i)] = Stack[SP - 1];
	SP -= 2 + i;
}

void c_param(llong i) { Stack[SP++] = Stack[FP + i]; }

/* Count directory contents (excluding . and ..) */

static llong get_dirsize(struct stat *statbuf, const char *name)
{
	int dir_fd;
	DIR *dir;
	struct dirent *entry;

	if ((dir_fd = openat(attr.parent_fd, name, 0)) == -1)
		return (llong)statbuf->st_size;

	if (!(dir = fdopendir(dir_fd)))
	{
		close(dir_fd);

		return (llong)statbuf->st_size;
	}

	statbuf->st_size = 0;

	while ((entry = readdir(dir)))
	{
		if (entry->d_name[0] == '.' && (!entry->d_name[1] || (entry->d_name[1] == '.' && !entry->d_name[2])))
			continue;

		++statbuf->st_size;
	}

	closedir(dir);

	return (llong)statbuf->st_size;
}

static llong dirsize(void)
{
	char *name;

	if (attr.dirsize_done)
		return (llong)attr.statbuf->st_size;

	attr.dirsize_done = 1;

	name = (attr.parent_fd == AT_FDCWD) ? attr.fpath : attr.basename;

	return get_dirsize(attr.statbuf, name);
}

static llong rdirsize(int i)
{
	if (RefFile[i].dirsize_done)
		return (llong)RefFile[i].statbuf->st_size;

	RefFile[i].dirsize_done = 1;

	return get_dirsize(RefFile[i].statbuf, Strbuf + RefFile[i].fpathi);
}

static llong tdirsize(void)
{
	if (attr.linkdirsize_done)
		return (llong)attr.linkstatbuf->st_size;

	attr.linkdirsize_done = 1;

	return get_dirsize(attr.linkstatbuf, attr.ftarget);
}

void set_dirsize(void)
{
	if (isdir(attr.statbuf))
		dirsize();
}

/* Built-ins */

void c_number(llong i)  { Stack[SP++] = i; }
void c_dev(llong i)     { Stack[SP++] = attr.statbuf->st_dev; }
void c_major(llong i)   { Stack[SP++] = major(attr.statbuf->st_dev); }
void c_minor(llong i)   { Stack[SP++] = minor(attr.statbuf->st_dev); }
void c_ino(llong i)     { Stack[SP++] = attr.statbuf->st_ino; }
void c_mode(llong i)    { Stack[SP++] = attr.statbuf->st_mode; }
void c_nlink(llong i)   { Stack[SP++] = attr.statbuf->st_nlink; }
void c_uid(llong i)     { Stack[SP++] = attr.statbuf->st_uid; }
void c_gid(llong i)     { Stack[SP++] = attr.statbuf->st_gid; }
void c_rdev(llong i)    { Stack[SP++] = attr.statbuf->st_rdev; }
void c_rmajor(llong i)  { Stack[SP++] = major(attr.statbuf->st_rdev); }
void c_rminor(llong i)  { Stack[SP++] = minor(attr.statbuf->st_rdev); }
void c_size(llong i)    { Stack[SP++] = isdir(attr.statbuf) ? dirsize() : attr.statbuf->st_size; }
void c_blksize(llong i) { Stack[SP++] = attr.statbuf->st_blksize; }
void c_blocks(llong i)  { Stack[SP++] = attr.statbuf->st_blocks; }
void c_atime(llong i)   { Stack[SP++] = attr.statbuf->st_atime; }
void c_mtime(llong i)   { Stack[SP++] = attr.statbuf->st_mtime; }
void c_ctime(llong i)   { Stack[SP++] = attr.statbuf->st_ctime; }
void c_depth(llong i)   { Stack[SP++] = attr.depth; }
void c_prune(llong i)   { Stack[SP++] = 1; attr.prune = attr.pruned = 1; }
void c_trim(llong i)    { Stack[SP++] = 1; attr.prune = 1; }
void c_exit(llong i)    { Stack[SP++] = 1; attr.exit = 1; }

unsigned long get_attr(void)
{
	if (!attr.attr_done)
	{
		attr.attr_done = 1;
		#if HAVE_ATTR
		fgetflags(attr.fpath, &attr.attr);
		#elif HAVE_FLAGS
		attr.attr = (unsigned long)attr.statbuf->st_flags;
		#endif
	}

	return attr.attr;
}

unsigned long get_proj(void)
{
	#ifdef HAVE_ATTR
	if (!attr.proj_done)
	{
		attr.proj_done = 1;
		fgetproject(attr.fpath, &attr.proj);
	}
	#endif

	return attr.proj;
}

unsigned long get_gen(void)
{
	#ifdef HAVE_ATTR
	if (!attr.gen_done)
	{
		attr.gen_done = 1;
		fgetversion(attr.fpath, &attr.gen);
	}
	#endif

	return attr.gen;
}

#if HAVE_ATTR || HAVE_FLAGS
void c_attr(llong i) { Stack[SP++] = get_attr(); }
#endif
#if HAVE_ATTR
void c_proj(llong i) { Stack[SP++] = get_proj(); }
void c_gen(llong i)  { Stack[SP++] = get_gen(); }
#endif

void c_strlen(llong i)
{
	int len;
	char *c;

	for (len = 0, c = attr.fpath; *c; c++)
		len = (*c == '/') ? 0 : len + 1;

	Stack[SP++] = len;
}

void c_nouser(llong i)
{
	Stack[SP++] = !getpwuid(attr.statbuf->st_uid);
}

void c_nogroup(llong i)
{
	Stack[SP++] = !getgrgid(attr.statbuf->st_gid);
}

void c_readable(llong i)
{
	if (attr.parent_fd != AT_FDCWD && attr.parent_fd != -1)
		Stack[SP++] = (faccessat(attr.parent_fd, attr.basename, R_OK, 0) == 0);
	else
		Stack[SP++] = (access(attr.fpath, R_OK) == 0);
}

void c_writable(llong i)
{
	if (attr.parent_fd != AT_FDCWD && attr.parent_fd != -1)
		Stack[SP++] = (faccessat(attr.parent_fd, attr.basename, W_OK, 0) == 0);
	else
		Stack[SP++] = (access(attr.fpath, W_OK) == 0);
}

void c_executable(llong i)
{
	if (attr.parent_fd != AT_FDCWD && attr.parent_fd != -1)
		Stack[SP++] = (faccessat(attr.parent_fd, attr.basename, X_OK, 0) == 0);
	else
		Stack[SP++] = (access(attr.fpath, X_OK) == 0);
}

static char *c_basename(void)
{
	char *tail;

	tail = strrchr(attr.fpath, '/');
	tail = (tail) ? tail + 1 : attr.fpath;

	return tail;
}

/* Read the current candidate symlink target path */

char *read_symlink(void)
{
	ssize_t nbytes = 0;

	if (!attr.ftarget_done)
	{
		if (!attr.ftarget && !(attr.ftarget = malloc(attr.fpath_size)))
			fatalsys("out of memory");

		if (attr.test_readlinkat_failure)
			errno = EPERM;

		if (attr.test_readlinkat_failure || (nbytes = readlinkat(attr.parent_fd, (attr.parent_fd == AT_FDCWD) ? attr.fpath : attr.basename, attr.ftarget, attr.fpath_size)) == -1)
			fatalsys("readlinkat %s", ok(attr.fpath));

		if (attr.test_readlinkat_too_long_failure || nbytes == attr.fpath_size)
			fatal("readlinkat %s: target is too long", ok(attr.fpath));

		attr.ftarget[nbytes] = '\0';
		attr.ftarget_done = 1;

		while (--nbytes > 0 && attr.ftarget[nbytes] == '/')
			attr.ftarget[nbytes] = '\0';
	}

	return attr.ftarget;
}

/*

Glob pattern matching.

The i parameter is an index into Strbuf.
The string contained there is the nul-terminated
string that is the pattern (e.g., "*.bak" without
the double quotes).

*/

void c_glob(llong i) { Stack[SP++] = fnmatch(&Strbuf[i], c_basename(), FNM_PATHNAME | FNM_EXTMATCH) == 0; }
void c_path(llong i) { Stack[SP++] = fnmatch(&Strbuf[i], attr.fpath, FNM_EXTMATCH) == 0; }
void c_link(llong i) { Stack[SP++] = (islink(attr.statbuf)) ? fnmatch(&Strbuf[i], read_symlink(), FNM_EXTMATCH) == 0 : 0; }

#ifdef FNM_CASEFOLD
void c_i(llong i)     { Stack[SP++] = fnmatch(&Strbuf[i], c_basename(), FNM_PATHNAME | FNM_EXTMATCH | FNM_CASEFOLD) == 0; }
void c_ipath(llong i) { Stack[SP++] = fnmatch(&Strbuf[i], attr.fpath, FNM_EXTMATCH | FNM_CASEFOLD) == 0; }
void c_ilink(llong i) { Stack[SP++] = (islink(attr.statbuf)) ? fnmatch(&Strbuf[i], read_symlink(), FNM_EXTMATCH | FNM_CASEFOLD) == 0 : 0; }
#endif

#ifdef HAVE_PCRE2

/* Wrapper for pcre2_compile() that handles errors with a fatal message */

static pcre2_code *pcre2_compile_checked(const char *pattern, uint32_t options)
{
	pcre2_code *re;
	int error_number;
	PCRE2_SIZE error_offset;
	PCRE2_UCHAR error_buffer[256];

	if (!(re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, options, &error_number, &error_offset, NULL)))
	{
		pcre2_get_error_message(error_number, error_buffer, sizeof error_buffer);
		fatal("invalid regex %s at offset %d: %s", ok(pattern), (int)error_offset, ok2((char *)error_buffer));
	}

	return re;
}

/* Wrapper for pcre2_compile() that caches the last two compiled regexes. That should speed up most searches. */

static pcre2_code *pcre2_compile_cached(const char *regex, uint32_t options)
{
	static const char *regex_cache[2];
	static uint32_t options_cache[2];
	static pcre2_code *code_cache[2];
	static int mru;

	/* Clear the cache when regex is null */

	if (!regex)
	{
		if (code_cache[mru])
			pcre2_code_free(code_cache[mru]);

		regex_cache[mru] = NULL;
		options_cache[mru] = 0;
		code_cache[mru] = NULL;

		mru = !mru;

		if (code_cache[mru])
			pcre2_code_free(code_cache[mru]);

		regex_cache[mru] = NULL;
		options_cache[mru] = 0;
		code_cache[mru] = NULL;

		return NULL;
	}

	/* Return the most recently used, if it matches */

	if (regex_cache[mru] && code_cache[mru] && options == options_cache[mru] && !strcmp(regex, regex_cache[mru]))
		return code_cache[mru];

	/* Return the other, if it matches */

	mru = !mru;

	if (regex_cache[mru] && code_cache[mru] && options == options_cache[mru] && !strcmp(regex, regex_cache[mru]))
		return code_cache[mru];

	/* Cache a new regex (freeing an old one if necessary) */

	if (code_cache[mru])
		pcre2_code_free(code_cache[mru]);

	regex_cache[mru] = regex;
	options_cache[mru] = options;
	code_cache[mru] = pcre2_compile_checked(regex, options);

	return code_cache[mru];
}

/* Clear the pcre2 cache when finished */

void pcre2_cache_free(void)
{
	pcre2_compile_cached(NULL, 0);
}

/* Perl-compatible regex matching (pcre2) */

static int rematch(const char *pattern, const char *subject, size_t subject_length, uint32_t options)
{
	pcre2_code *re;
	pcre2_match_data *match_data;
	int rc;

	if (attr.utf)
		options |= PCRE2_UTF | PCRE2_MATCH_INVALID_UTF; /* Assume UTF-8 patterns and subject text */

	if (attr.dotall_always)
		options |= PCRE2_DOTALL;        /* . matches anything including newline (like /s) */

	if (attr.multiline_always)
		options |= PCRE2_MULTILINE;     /* ^ matches after every newline, $ matches before every newline (like /m) */

	options |= PCRE2_DOLLAR_ENDONLY;    /* $ matches only at the end of the subject */
	options |= PCRE2_EXTENDED;          /* Ignore whitespace and # comments (except in character classes) (like /x) */
	options |= PCRE2_EXTENDED_MORE;     /* Ignore space and tab inside character classes as well (like /xx) */
	options |= PCRE2_NO_AUTO_CAPTURE;   /* Prevent automatic numbered capturing parentheses (like /n) */

	re = pcre2_compile_cached(pattern, options);

	if (!(match_data = pcre2_match_data_create_from_pattern(re, NULL)))
		fatalsys("out of memory");

	if (subject_length == -1)
		subject_length = strlen(subject);

	rc = pcre2_match(re, (PCRE2_SPTR)subject, (PCRE2_SIZE)subject_length, 0, 0, match_data, NULL);

	pcre2_match_data_free(match_data);

	return rc >= 0;
}

void c_re(llong i)      { Stack[SP++] = rematch(&Strbuf[i], c_basename(), -1, PCRE2_DOTALL); }
void c_repath(llong i)  { Stack[SP++] = rematch(&Strbuf[i], attr.fpath, -1, PCRE2_DOTALL); }
void c_relink(llong i)  { Stack[SP++] = (islink(attr.statbuf)) ? rematch(&Strbuf[i], read_symlink(), -1, PCRE2_DOTALL) : 0; }
void c_rei(llong i)     { Stack[SP++] = rematch(&Strbuf[i], c_basename(), -1, PCRE2_DOTALL | PCRE2_CASELESS); }
void c_reipath(llong i) { Stack[SP++] = rematch(&Strbuf[i], attr.fpath, -1, PCRE2_DOTALL | PCRE2_CASELESS); }
void c_reilink(llong i) { Stack[SP++] = (islink(attr.statbuf)) ? rematch(&Strbuf[i], read_symlink(), -1, PCRE2_DOTALL | PCRE2_CASELESS) : 0; }

#endif /* HAVE_PCRE2 */

/* Load the file's content into attr.body (deallocated at exit) */

char *get_body(void)
{
	if (!isreg(attr.statbuf))
		return NULL;

	if (!attr.body_done)
	{
		attr.body_length = 0;

		int fd;

		if ((fd = open(attr.fpath, O_RDONLY)) == -1)
		{
			errorsys("%s", ok(attr.fpath));
			attr.exit_status = EXIT_FAILURE;
		}
		else
		{
			/* Increase the buffer size when necessary (include space for a nul byte) */

			if (attr.body_size < attr.statbuf->st_size + 1)
			{
				if (!(attr.body = realloc(attr.body, attr.statbuf->st_size + 1)))
					fatalsys("out of memory");

				attr.body_size = attr.statbuf->st_size + 1;
			}

			/* Read the file */

			ssize_t bytes = 0;

			#define READ_MAX 0x7ffff000 /* Limit on Linux, also needed on macOS */

			while (attr.body_length < attr.statbuf->st_size && (bytes = read(fd, attr.body + attr.body_length, (attr.statbuf->st_size - attr.body_length > READ_MAX) ? READ_MAX : (attr.statbuf->st_size - attr.body_length))) > 0)
				attr.body_length += bytes;

			if (bytes == -1)
			{
				errorsys("read %s", ok(attr.fpath));
				attr.exit_status = EXIT_FAILURE;
			}

			close(fd);

			/* Nul-terminate the buffer for fnmatch() */

			attr.body[attr.body_length] = '\0';
		}

		attr.body_done = 1;
	}

	return attr.body;
}

static void body_glob(llong i, int options)
{
	Stack[SP++] = get_body() ? fnmatch(&Strbuf[i], get_body(), FNM_EXTMATCH | options) == 0 : 0;
}

void c_body(llong i)  { body_glob(i, 0); }
#ifdef FNM_CASEFOLD
void c_ibody(llong i) { body_glob(i, FNM_CASEFOLD); }
#endif

#ifdef HAVE_PCRE2

static void body_re(llong i, int options)
{
	Stack[SP++] = get_body() ? rematch(&Strbuf[i], get_body(), attr.body_length, PCRE2_MULTILINE | options) : 0;
}

void c_rebody(llong i)  { body_re(i, 0); }
void c_reibody(llong i) { body_re(i, PCRE2_CASELESS); }

#endif

/* Load the file type description into attr.what (libmagic-managed data - deallocated at exit) */

const char *get_what(void)
{
	if (!attr.what_done)
	{
		#ifdef HAVE_MAGIC
		if (following_symlinks())
		{
			if (!attr.what_follow_cookie)
				if ((attr.what_follow_cookie = magic_open(MAGIC_RAW | MAGIC_COMPRESS | MAGIC_SYMLINK)))
					(void)magic_load(attr.what_follow_cookie, NULL);

			if (attr.what_follow_cookie)
				attr.what = magic_file(attr.what_follow_cookie, attr.fpath);
		}
		else
		{
			if (!attr.what_cookie)
				if ((attr.what_cookie = magic_open(MAGIC_RAW | MAGIC_COMPRESS)))
					(void)magic_load(attr.what_cookie, NULL);

			if (attr.what_cookie)
				attr.what = magic_file(attr.what_cookie, attr.fpath);
		}
		#endif

		attr.what_done = 1;
	}

	return attr.what;
}

#ifdef HAVE_MAGIC

static void what_glob(llong i, int options)
{
	Stack[SP++] = get_what() ? fnmatch(&Strbuf[i], get_what(), FNM_EXTMATCH | options) == 0 : 0;
}

void c_what(llong i)  { what_glob(i, 0); }
#ifdef FNM_CASEFOLD
void c_iwhat(llong i) { what_glob(i, FNM_CASEFOLD); }
#endif

#ifdef HAVE_PCRE2

static void what_re(llong i, int options)
{
	Stack[SP++] = get_what() ? rematch(&Strbuf[i], get_what(), -1, PCRE2_MULTILINE | options) : 0;
}

void c_rewhat(llong i)  { what_re(i, 0); }
void c_reiwhat(llong i) { what_re(i, PCRE2_CASELESS); }

#endif
#endif

/* Load the MIME type into attr.what (libmagic-managed data - deallocated at exit) */

const char *get_mime(void)
{
	if (!attr.mime_done)
	{
		#ifdef HAVE_MAGIC
		if (following_symlinks())
		{
			if (!attr.mime_follow_cookie)
				if ((attr.mime_follow_cookie = magic_open(MAGIC_MIME | MAGIC_SYMLINK)))
					(void)magic_load(attr.mime_follow_cookie, NULL);

			if (attr.mime_follow_cookie)
				attr.mime = magic_file(attr.mime_follow_cookie, attr.fpath);
		}
		else
		{
			if (!attr.mime_cookie)
				if ((attr.mime_cookie = magic_open(MAGIC_MIME)))
					(void)magic_load(attr.mime_cookie, NULL);

			if (attr.mime_cookie)
				attr.mime = magic_file(attr.mime_cookie, attr.fpath);
		}
		#endif

		attr.mime_done = 1;
	}

	return attr.mime;
}

#ifdef HAVE_MAGIC

static void mime_glob(llong i, int options)
{
	Stack[SP++] = get_mime() ? fnmatch(&Strbuf[i], get_mime(), FNM_EXTMATCH | options) == 0 : 0;
}

void c_mime(llong i)  { mime_glob(i, 0); }
#ifdef FNM_CASEFOLD
void c_imime(llong i) { mime_glob(i, FNM_CASEFOLD); }
#endif

#ifdef HAVE_PCRE2

static void mime_re(llong i, int options)
{
	Stack[SP++] = get_mime() ? rematch(&Strbuf[i], get_mime(), -1, PCRE2_DOTALL | options) : 0;
}

void c_remime(llong i)  { mime_re(i, 0); }
void c_reimime(llong i) { mime_re(i, PCRE2_CASELESS); }

#endif
#endif

/* Load ACLs into attr.facl (and attr.facl_verbose on FreeBSD/Solaris) which must be deallocated */

#define failure(args) if (want) { errorsys args; attr.exit_status = EXIT_FAILURE; }

char *get_acl(int want)
{
	if (!attr.facl_done)
	{
		/* The two main choices for ACL APIs are "POSIX" and SOLARIS */

		#ifdef HAVE_POSIX_ACL

		/* macOS has the "POSIX" ACL API, but has its own type: ACL_TYPE_EXTENDED */

		#ifdef HAVE_MACOS_ACL
		int acl_flags = ACL_TYPE_EXTENDED;
		#else
		int acl_flags = ACL_TYPE_ACCESS;
		#endif

		/* Get the "POSIX" or macOS (but not NFSv4) ACL */

		acl_t acl = acl_get_file(attr.fpath, acl_flags);

		/* FreeBSD fails above with EINVAL when an NFSv4 ACL is present, so try that */

		#ifdef HAVE_FREEBSD_ACL

		if (!acl && errno == EINVAL)
			acl = acl_get_file(attr.fpath, ACL_TYPE_NFS4);

		#endif

		#ifdef HAVE_MACOS_ACL

		if (!acl && errno && errno != ENOENT) /* macOS does this when there are no ACLs */
			failure(("acl_get_file %s", ok(attr.fpath)))

		#else

		if (!acl && errno && errno != EOPNOTSUPP && errno != ENOTSUP) /* FreeBSD does EOPNOTSUPP for char devices */
			failure(("acl_get_file %s", ok(attr.fpath)))

		#endif

		/* Convert the ACL to text */

		if (acl)
		{
			if (!(attr.facl = acl_to_text(acl, NULL)) && errno)
				failure(("acl_to_text %s", ok(attr.fpath)))

			/* On FreeBSD, NFSv4 ACLs have compact and verbose formats (like Solaris) */

			#ifdef HAVE_FREEBSD_ACL

			if (!(attr.facl_verbose = acl_to_text_np(acl, NULL, ACL_TEXT_VERBOSE)) && errno)
				failure(("acl_to_text_np %s", ok(attr.fpath)))

			#endif

			acl_free(acl);
		}

		#endif /* HAVE_POSIX_ACL */

		#ifdef HAVE_SOLARIS_ACL

		acl_t *acl = NULL;
		int acl_flags = (attr.facl_solaris_no_trivial ? ACL_NO_TRIVIAL : 0);

		if (acl_get(attr.fpath, acl_flags, &acl) != -1 && acl)
		{
			if (!(attr.facl = acl_totext(acl, ACL_COMPACT_FMT | ACL_SID_FMT)) && errno)
				failure(("acl_totext %s", ok(attr.fpath)))

			if (!(attr.facl_verbose = acl_totext(acl, ACL_SID_FMT)) && errno)
				failure(("acl_totext %s", ok(attr.fpath)))

			acl_free(acl);
		}
		else if (errno)
			failure(("acl_get %s", ok(attr.fpath)))

		#endif /* HAVE_SOLARIS_ACL */

		attr.facl_done = 1;
	}

	return attr.facl;
}

#ifdef HAVE_ACL

static void acl_glob(llong i, int options)
{
	Stack[SP++] = get_acl(1)
		? fnmatch(&Strbuf[i], get_acl(1), FNM_EXTMATCH | options) == 0 ||
			(attr.facl_verbose && fnmatch(&Strbuf[i], attr.facl_verbose, FNM_EXTMATCH | options) == 0)
		: 0;
}

void c_acl(llong i)  { acl_glob(i, 0); }
#ifdef FNM_CASEFOLD
void c_iacl(llong i) { acl_glob(i, FNM_CASEFOLD); }
#endif

#ifdef HAVE_PCRE2

static void acl_re(llong i, int options)
{
	Stack[SP++] = get_acl(1)
		? rematch(&Strbuf[i], get_acl(1), -1, PCRE2_MULTILINE | options) ||
			(attr.facl_verbose && rematch(&Strbuf[i], attr.facl_verbose, -1, PCRE2_MULTILINE | options))
		: 0;
}

void c_reacl(llong i)  { acl_re(i, 0); }
void c_reiacl(llong i) { acl_re(i, PCRE2_CASELESS); }

#endif
#endif

/* Load EAs into attr.fea which must be deallocated eventually */

char *get_ea(int want)
{
	if (!attr.fea_done)
	{
		#if HAVE_LINUX_EA || HAVE_MACOS_EA

		ssize_t buflen, vallen, rc;
		char *buf, *name, *val;
		size_t namelen;
		llong fea_size;
		int i, pos = 0;

		attr.fea_done = 1;

		/* Allocate a long-lived buffer (freed at the end of rawhide_search()) */

		fea_size = (attr.fea_size) ? attr.fea_size : 4 * 1024;

		if (!attr.fea && !(attr.fea = malloc(fea_size)))
		{
			errorsys("out of memory");
			attr.exit_status = EXIT_FAILURE;

			return NULL;
		}

		/* Handle the extended attributes changing while we're looking */

		for (i = 0; i < 5; ++i)
		{
			/* Get the size of the list of names */

			#if HAVE_LINUX_EA
			if ((buflen = ((following_symlinks()) ? listxattr : llistxattr)(attr.fpath, NULL, 0)) == -1)
			#elif HAVE_MACOS_EA
			if ((buflen = listxattr(attr.fpath, NULL, 0, (following_symlinks()) ? 0 : XATTR_NOFOLLOW)) == -1)
			#endif
			{
				if (errno != EPERM && errno != ENOTSUP && errno != EACCES) /* macOS + chardev = EPERM, Cygwin + chardev = ENOTSUP, Cygwin + symlink = EACCES */
					failure(("listxattr %s", ok(attr.fpath)))

				return NULL;
			}

			/* If there are none, stop */

			if (buflen == 0)
				return NULL;

			/* Allocate space for the list of names */

			if (!(buf = malloc(buflen)))
			{
				errorsys("out of memory");
				attr.exit_status = EXIT_FAILURE;

				return NULL;
			}

			/* Get the list of names, retry if the size got bigger */

			#if HAVE_LINUX_EA
			if ((rc = ((following_symlinks()) ? listxattr : llistxattr)(attr.fpath, buf, (size_t)buflen)) == -1 && errno == ERANGE)
			#elif HAVE_MACOS_EA
			if ((rc = listxattr(attr.fpath, buf, (size_t)buflen, (following_symlinks()) ? 0 : XATTR_NOFOLLOW)) == -1 && errno == ERANGE)
			#endif
			{
				free(buf);
				buf = NULL;

				continue;
			}

			/* Handle any other error */

			if (rc == -1)
			{
				failure(("listxattr %s", ok(attr.fpath)))
				free(buf);

				return NULL;
			}

			/* Success */

			break;
		}

		/* Give up after five attempts */

		if (i == 5)
		{
			failure(("listxattr %s: size keeps changing", ok(attr.fpath)))

			if (buf)
				free(buf);

			return NULL;
		}

		/* For each name, get its value (if any) */

		for (name = buf; buflen > 0; namelen = strlen(name) + 1, buflen -= namelen, name += namelen)
		{
			/* Maybe the name list size got smaller above and we couldn't detect it. */
			/* This might or might not handle that case. */

			if (!*name)
				break;

			/* Add the name */

			pos += cescape(attr.fea + pos, fea_size - pos, name, -1, CESCAPE_HEX);

			/* Don't consider Linux ACL or selinux EAs to be "real" EAs (for -l) */

			if (strcmp(name, "system.posix_acl_access") && strcmp(name, "system.posix_acl_default") && strcmp(name, "security.selinux"))
				attr.fea_real = 1;

			/* Handle the value changing while we're looking */

			for (i = 0; i < 5; ++i)
			{
				/* Get the size of the value */

				#if HAVE_LINUX_EA
				if ((vallen = ((following_symlinks()) ? getxattr : lgetxattr)(attr.fpath, name, NULL, 0)) == -1)
				#elif HAVE_MACOS_EA
				if ((vallen = getxattr(attr.fpath, name, NULL, 0, 0, (following_symlinks()) ? 0 : XATTR_NOFOLLOW)) == -1)
				#endif
				{
					failure(("getxattr %s name %s", ok(attr.fpath), ok2(name)))
					free(buf);

					return NULL;
				}

				/* If there is none, move on to the next name */

				if (vallen == 0)
					break;

				/* Allocate space for the value */

				if (!(val = malloc(vallen + 1)))
				{
					errorsys("out of memory");
					attr.exit_status = EXIT_FAILURE;
					free(buf);

					return NULL;
				}

				/* Get the value, retry if the size got bigger */

				#if HAVE_LINUX_EA
				if ((rc = ((following_symlinks()) ? getxattr : lgetxattr)(attr.fpath, name, val, (size_t)vallen)) == -1 && errno == ERANGE)
				#elif HAVE_MACOS_EA
				if ((rc = getxattr(attr.fpath, name, val, (size_t)vallen, 0, (following_symlinks()) ? 0 : XATTR_NOFOLLOW)) == -1 && errno == ERANGE)
				#endif
				{
					free(val);
					val = NULL;

					continue;
				}

				/* Handle any other error */

				if (rc == -1)
				{
					failure(("getxattr %s name %s", ok(attr.fpath), ok2(name)))
					free(buf);

					return NULL;
				}

				/* Add the value (which can be binary data) */

				pos += ssnprintf(attr.fea + pos, fea_size - pos, ": ");
				pos += cescape(attr.fea + pos, fea_size - pos, val, vallen, CESCAPE_BIN);

				/* Record separately for -L %Z if it's the selinux context */

				if (!attr.fea_selinux && !strcmp(name, "security.selinux"))
					attr.fea_selinux = strdup(val);

				free(val);

				/* Success */

				break;
			}

			/* Add a newline after every attribute */

			pos += ssnprintf(attr.fea + pos, fea_size - pos, "\n");
		}

		/* Mark attr.fea as usable (being non-NULL isn't enough, as it's a long-lived buffer) */

		free(buf);
		attr.fea_ok = 1;

		#endif /* if HAVE_LINUX_EA || HAVE_MACOS_EA */

		#if HAVE_FREEBSD_EA

		ssize_t buflen, vallen;
		char *buf, *name, *val;
		char *namebuf;
		llong fea_size;
		int ns, found = 0, pos = 0;
		static int namespace[2] = { EXTATTR_NAMESPACE_USER, EXTATTR_NAMESPACE_SYSTEM };
		static const char *namespace_name[2] = { "user", "system" };

		attr.fea_done = 1;

		/* Allocate a long-lived buffer (freed at the end of rawhide_search()) */

		fea_size = (attr.fea_size) ? attr.fea_size : 4 * 1024;

		if (!attr.fea && !(attr.fea = malloc(fea_size)))
		{
			errorsys("out of memory");
			attr.exit_status = EXIT_FAILURE;

			return NULL;
		}

		/* First the unprivileged user namespace, then the privileged system namespace */

		for (ns = 0; ns < 2; ++ns)
		{
			/* Only root may see the system attributes */

			if (namespace[ns] == EXTATTR_NAMESPACE_SYSTEM && getuid() != 0)
				continue;

			/* Find the amount of space needed for the list of names */

			if ((buflen = ((following_symlinks()) ? extattr_list_file : extattr_list_link)(attr.fpath, namespace[ns], NULL, 0)) == -1)
			{
				if (errno != EOPNOTSUPP)
					failure(("extattr_list_file user %s", ok(attr.fpath)))

				return NULL;
			}

			/* If there are none, skip this namespace */

			if (buflen == 0)
				continue;

			found = 1;

			/* Allocate space for the list of names */

			if (!(buf = malloc(buflen)))
			{
				errorsys("out of memory");
				attr.exit_status = EXIT_FAILURE;

				return NULL;
			}

			/* Get the list of names */

			if (((following_symlinks()) ? extattr_list_file : extattr_list_link)(attr.fpath, namespace[ns], buf, buflen) == -1)
			{
				failure(("extattr_list_file user %s", ok(attr.fpath)))
				free(buf);

				return NULL;
			}

			/* For each name, get its value (if any) */
			/* Attribute names on FreeBSD have leading length bytes, not terminating nul bytes */

			for (name = buf; name < buf + buflen; name += *name + 1)
			{
				/* Add the namespace and name */

				pos += ssnprintf(attr.fea + pos, fea_size - pos, "%s.", namespace_name[ns]);
				pos += cescape(attr.fea + pos, fea_size - pos, name + 1, *name, CESCAPE_HEX);

				/* Consider all EAs to be real EAs on FreeBSD (for -l) */

				attr.fea_real = 1;

				/* Allocate space for the name */

				if (!(namebuf = malloc(*name + 1)))
				{
					errorsys("out of memory");
					attr.exit_status = EXIT_FAILURE;
					free(buf);

					return NULL;
				}

				ssnprintf(namebuf, *name + 1, "%.*s", *name, name + 1);

				/* Get the size of the value */

				if ((vallen = ((following_symlinks()) ? extattr_get_file : extattr_get_link)(attr.fpath, namespace[ns], namebuf, NULL, 0)) == -1)
				{
					failure(("extattr_get_file user %s name %.*s", ok(attr.fpath), *name, oklen(name + 1, *name)))
					free(namebuf);
					free(buf);

					return NULL;
				}

				/* If there is none, move on to the next name */

				if (vallen == 0)
				{
					pos += ssnprintf(attr.fea + pos, fea_size - pos, "\n");
					free(namebuf);

					continue;
				}

				/* Allocate space for the value */

				if (!(val = malloc(vallen + 1)))
				{
					errorsys("out of memory");
					attr.exit_status = EXIT_FAILURE;
					free(namebuf);
					free(buf);

					return NULL;
				}

				/* Get the value */

				if (((following_symlinks()) ? extattr_get_file : extattr_get_link)(attr.fpath, namespace[ns], namebuf, val, vallen) == -1)
				{
					failure(("extattr_get_file user %s name %.*s", ok(attr.fpath), *name, oklen(name + 1, *name)))
					free(val);
					free(namebuf);
					free(buf);

					return NULL;
				}

				/* Add the value (which can be binary data) */

				pos += ssnprintf(attr.fea + pos, fea_size - pos, ": ");
				pos += cescape(attr.fea + pos, fea_size - pos, val, vallen, CESCAPE_BIN);
				pos += ssnprintf(attr.fea + pos, fea_size - pos, "\n");

				free(val);
				free(namebuf);
			}

			free(buf);
		}

		/* Mark attr.fea as usable (being non-NULL isn't enough, as it's a long-lived buffer) */

		if (found)
			attr.fea_ok = 1;

		#endif /* HAVE_FREEBSD_EA */

		#if HAVE_SOLARIS_EA

		ssize_t vallen;
		char *val;
		llong fea_size;
		int pos = 0;
		int adfd, afd;
		DIR *dir;
		struct dirent *entry;
		struct stat statbuf[1];

		attr.fea_done = 1;

		/* Symlinks can't have EAs on Solaris so handle symlink-following here */

		if (islink(attr.statbuf) && !following_symlinks())
			return NULL;

		/* First check if there are any extended attributes */

		if (!pathconf(attr.fpath, _PC_XATTR_EXISTS))
			return NULL;

		/* Allocate a long-lived buffer (freed at the end of rawhide_search()) */

		fea_size = (attr.fea_size) ? attr.fea_size : 64 * 1024;

		if (!attr.fea && !(attr.fea = malloc(fea_size)))
		{
			errorsys("out of memory");
			attr.exit_status = EXIT_FAILURE;

			return NULL;
		}

		/* Scan the extended attribute directory */

		if ((adfd = attropen(attr.fpath, ".", O_RDONLY | O_XATTR)) == -1)
		{
			if (errno != EINVAL)
				failure(("attropen %s", ok(attr.fpath)))

			return NULL;
		}

		if (!(dir = fdopendir(adfd)))
		{
			failure(("fdopendir %s (ea)", ok(attr.fpath)))
			close(adfd);

			return NULL;
		}

		while ((entry = readdir(dir)))
		{
			/* Note: .. is the parent *file* or directory, so skip by name not type */

			if (entry->d_name[0] == '.' && (!entry->d_name[1] || (entry->d_name[1] == '.' && !entry->d_name[2])))
				continue;

			if (fstatat(adfd, entry->d_name, statbuf, 0) == -1)
			{
				failure(("fstatat %s (ea) %s", ok(attr.fpath), ok2(entry->d_name)))
				closedir(dir);

				return NULL;
			}

			/* Theoretically there could be anything but really only regular files can exist */

			if ((statbuf->st_mode & S_IFMT) != S_IFREG)
				continue;

			/* Exclude the ubiquitous SUNWattr_ro and SUNWattr_rw if requested */

			if (attr.fea_solaris_no_sunwattr && (!strcmp(entry->d_name, "SUNWattr_ro") || !strcmp(entry->d_name, "SUNWattr_rw")))
				continue;

			/* Add the name */

			pos += cescape(attr.fea + pos, fea_size - pos, entry->d_name, -1, CESCAPE_HEX);

			/* Consider all EAs to be real EAs on Solaris (for -l) */

			attr.fea_real = 1;

			/* Does it have a value? */

			if (statbuf->st_size == 0)
			{
				pos += ssnprintf(attr.fea + pos, fea_size - pos, "\n");

				continue;
			}

			/* Read the value */

			if ((afd = openat(adfd, entry->d_name, O_RDONLY)) == -1)
			{
				failure(("openat %s (ea) %s", ok(attr.fpath), ok2(entry->d_name)))
				closedir(dir);

				return NULL;
			}

			vallen = statbuf->st_size;

			if (!(val = malloc(vallen)))
			{
				errorsys("out of memory");
				attr.exit_status = EXIT_FAILURE;
				close(afd);
				closedir(dir);

				return NULL;
			}

			if (read(afd, val, vallen) != vallen)
			{
				failure(("read %s (ea) %s", ok(attr.fpath), ok2(entry->d_name)))
				free(val);
				close(afd);
				closedir(dir);

				return NULL;
			}

			close(afd);

			/* Add the value (which can be binary data) */

			pos += ssnprintf(attr.fea + pos, fea_size - pos, ": ");
			pos += cescape(attr.fea + pos, fea_size - pos, val, vallen, CESCAPE_BIN);
			pos += ssnprintf(attr.fea + pos, fea_size - pos, "\n");

			free(val);
			val = NULL;

			/* Add an artificial stat(2) info EA about the real EA */

			if (!attr.fea_solaris_no_statinfo)
			{
				struct passwd *pwd = getpwuid(statbuf->st_uid);
				struct group *grp = getgrgid(statbuf->st_gid);

				pos += cescape(attr.fea + pos, fea_size - pos, entry->d_name, -1, CESCAPE_HEX);
				pos += ssnprintf(attr.fea + pos, fea_size - pos, "/stat: %s %d", modestr(statbuf), (int)statbuf->st_nlink);

				if (pwd)
					pos += ssnprintf(attr.fea + pos, fea_size - pos, " %s", pwd->pw_name);
				else
					pos += ssnprintf(attr.fea + pos, fea_size - pos, " %lld", (llong)statbuf->st_uid);

				if (grp)
					pos += ssnprintf(attr.fea + pos, fea_size - pos, " %s", grp->gr_name);
				else
					pos += ssnprintf(attr.fea + pos, fea_size - pos, " %lld", (llong)statbuf->st_gid);

				pos += ssnprintf(attr.fea + pos, fea_size - pos, " %lld", (llong)statbuf->st_size);

				pos += strftime(attr.fea + pos, fea_size - pos, " %Y-%m-%d %H:%M:%S %z", localtime(&statbuf->st_mtime));
				pos += strftime(attr.fea + pos, fea_size - pos, " %Y-%m-%d %H:%M:%S %z", localtime(&statbuf->st_atime));
				pos += strftime(attr.fea + pos, fea_size - pos, " %Y-%m-%d %H:%M:%S %z", localtime(&statbuf->st_ctime));
				pos += ssnprintf(attr.fea + pos, fea_size - pos, "\n");
			}
		}

		closedir(dir);

		/* Mark attr.fea as usable (being non-NULL isn't enough, as it's a long-lived buffer) */

		attr.fea_ok = 1;

		#endif /* HAVE_SOLARIS_EA */
	}

	return attr.fea;
}

/* Does the current candidate file have a real ACL? */

int has_real_acl(void)
{
	char *acl = get_acl(0);

	if (!acl)
		return 0;

	/* NFSv4 ACLs */

	if (strstr(acl, "owner@:"))
		return strstr(acl, "user:") || strstr(acl, "group:");

	/* "POSIX" ACLs */

	if (strstr(acl, "user::"))
		return strstr(acl, "mask::") || (get_ea(0) && attr.fea_ok && strstr(attr.fea, "system.posix_acl_access: "));

	/* macOS ACLs */

	return acl != NULL;
}

/* Does the current candidate file have real EAs? */

int has_real_ea(void)
{
	char *ea = get_ea(0);

	return ea && attr.fea_ok && attr.fea_real;
}

#ifdef HAVE_EA

static void ea_glob(llong i, int options)
{
	Stack[SP++] = get_ea(1) && attr.fea_ok ? fnmatch(&Strbuf[i], get_ea(1), FNM_EXTMATCH | options) == 0 : 0;
}

void c_ea(llong i)  { ea_glob(i, 0); }
#ifdef FNM_CASEFOLD
void c_iea(llong i) { ea_glob(i, FNM_CASEFOLD); }
#endif

#ifdef HAVE_PCRE2

static void ea_re(llong i, int options)
{
	Stack[SP++] = get_ea(1) && attr.fea_ok ? rematch(&Strbuf[i], get_ea(1), -1, PCRE2_MULTILINE | options) : 0;
}

void c_reea(llong i)  { ea_re(i, 0); }
void c_reiea(llong i) { ea_re(i, PCRE2_CASELESS); }

#endif
#endif

/*  Execute a shell command with syscmd() */

void c_sh(llong i)
{
	char command[CMDBUFSIZE];
	int dot_fd;

	/* Change cwd to the file's directory (to avoid path-based race conditions) */

	dot_fd = chdir_local(0);

	/* Prepare the command for this entry and execute it */

	Stack[SP++] = interpolate_command(&Strbuf[i], command, CMDBUFSIZE, FOR_SH) != -1 && syscmd(command) == 0;

	/* Change cwd back */

	if (dot_fd != -1)
	{
		if (fchdir(dot_fd) == -1)
			fatalsys("fchdir back");

		close(dot_fd);
	}
}

/*  Execute a shell command with usyscmd() */

void c_ush(llong i)
{
	char command[CMDBUFSIZE];
	int dot_fd;

	/* Change cwd to the file's directory (to avoid path-based race conditions) */

	dot_fd = chdir_local(0);

	/* Prepare the command for this entry and execute it */

	Stack[SP++] = interpolate_command(&Strbuf[i], command, CMDBUFSIZE, FOR_USH) != -1 && usyscmd(command) == 0;

	/* Change cwd back */

	if (dot_fd != -1)
	{
		if (fchdir(dot_fd) == -1)
			fatalsys("fchdir back");

		close(dot_fd);
	}
}

/* Reference file fields */

static void check_reference(llong i, char *name)
{
	if (!RefFile[i].exists)
		fatal("invalid reference \"%s\".%s: No such file or directory", ok(Strbuf + RefFile[i].fpathi), name);
}

void r_exists(llong i)  { Stack[SP++] = RefFile[i].exists; }
void r_strlen(llong i)  { Stack[SP++] = RefFile[i].baselen; }
void r_dev(llong i)     { check_reference(i, "dev");     Stack[SP++] = RefFile[i].statbuf->st_dev; }
void r_major(llong i)   { check_reference(i, "major");   Stack[SP++] = major(RefFile[i].statbuf->st_dev); }
void r_minor(llong i)   { check_reference(i, "minor");   Stack[SP++] = minor(RefFile[i].statbuf->st_dev); }
void r_ino(llong i)     { check_reference(i, "ino");     Stack[SP++] = RefFile[i].statbuf->st_ino; }
void r_mode(llong i)    { check_reference(i, "mode");    Stack[SP++] = RefFile[i].statbuf->st_mode; }
void r_nlink(llong i)   { check_reference(i, "nlink");   Stack[SP++] = RefFile[i].statbuf->st_nlink; }
void r_uid(llong i)     { check_reference(i, "uid");     Stack[SP++] = RefFile[i].statbuf->st_uid; }
void r_gid(llong i)     { check_reference(i, "gid");     Stack[SP++] = RefFile[i].statbuf->st_gid; }
void r_rdev(llong i)    { check_reference(i, "rdev");    Stack[SP++] = RefFile[i].statbuf->st_rdev; }
void r_rmajor(llong i)  { check_reference(i, "rmajor");  Stack[SP++] = major(RefFile[i].statbuf->st_rdev); }
void r_rminor(llong i)  { check_reference(i, "rminor");  Stack[SP++] = minor(RefFile[i].statbuf->st_rdev); }
void r_size(llong i)    { check_reference(i, "size");    Stack[SP++] = isdir(RefFile[i].statbuf) ? rdirsize(i) : RefFile[i].statbuf->st_size; }
void r_blksize(llong i) { check_reference(i, "blksize"); Stack[SP++] = RefFile[i].statbuf->st_blksize; }
void r_blocks(llong i)  { check_reference(i, "blocks");  Stack[SP++] = RefFile[i].statbuf->st_blocks; }
void r_atime(llong i)   { check_reference(i, "atime");   Stack[SP++] = RefFile[i].statbuf->st_atime; }
void r_mtime(llong i)   { check_reference(i, "mtime");   Stack[SP++] = RefFile[i].statbuf->st_mtime; }
void r_ctime(llong i)   { check_reference(i, "ctime");   Stack[SP++] = RefFile[i].statbuf->st_ctime; }
void r_type(llong i)    { check_reference(i, "type");    Stack[SP++] = RefFile[i].statbuf->st_mode & S_IFMT; }
void r_perm(llong i)    { check_reference(i, "perm");    Stack[SP++] = RefFile[i].statbuf->st_mode & ~S_IFMT; }

#if HAVE_ATTR

void r_attr(llong i)
{
	check_reference(i, "attr");

	if (!RefFile[i].attr_done)
	{
		RefFile[i].attr_done = 1;
		fgetflags(Strbuf + RefFile[i].fpathi, &RefFile[i].attr);
	}

	Stack[SP++] = RefFile[i].attr;
}

void r_proj(llong i)
{
	check_reference(i, "proj");

	if (!RefFile[i].proj_done)
	{
		RefFile[i].proj_done = 1;
		fgetproject(Strbuf + RefFile[i].fpathi, &RefFile[i].proj);
	}

	Stack[SP++] = RefFile[i].proj;
}

void r_gen(llong i)
{
	check_reference(i, "gen");

	if (!RefFile[i].gen_done)
	{
		RefFile[i].gen_done = 1;
		fgetversion(Strbuf + RefFile[i].fpathi, &RefFile[i].gen);
	}

	Stack[SP++] = RefFile[i].gen;
}

#elif HAVE_FLAGS

void r_attr(llong i)
{
	check_reference(i, "attr");

	if (!RefFile[i].attr_done)
	{
		RefFile[i].attr_done = 1;
		RefFile[i].attr = RefFile[i].statbuf->st_flags;
	}

	Stack[SP++] = RefFile[i].attr;
}

#endif

/* Symlink targets */

void prepare_target(void)
{
	char *name;

	if (!attr.linkstat_done)
	{
		attr.linkstat_done = 1;
		attr.linkstat_ok = 0;
		attr.linkstat_baselen = 0;
		memset(attr.linkstatbuf, '\0', sizeof(*attr.linkstatbuf));

		if (!islink(attr.statbuf))
			return;

		name = (attr.parent_fd == AT_FDCWD) ? attr.fpath : attr.basename;
		attr.linkstat_ok = (fstatat(attr.parent_fd, name, attr.linkstatbuf, 0) != -1);
		attr.linkstat_errno = errno;
		name = (name = strrchr(read_symlink(), '/')) ? name + 1 : attr.ftarget;
		attr.linkstat_baselen = strlen(name);
	}
}

void t_exists(llong i)  { prepare_target(); Stack[SP++] = attr.linkstat_ok; }
void t_strlen(llong i)  { prepare_target(); Stack[SP++] = attr.linkstat_baselen; }
void t_dev(llong i)     { prepare_target(); Stack[SP++] = attr.linkstatbuf->st_dev; }
void t_major(llong i)   { prepare_target(); Stack[SP++] = major(attr.linkstatbuf->st_dev); }
void t_minor(llong i)   { prepare_target(); Stack[SP++] = minor(attr.linkstatbuf->st_dev); }
void t_ino(llong i)     { prepare_target(); Stack[SP++] = attr.linkstatbuf->st_ino; }
void t_mode(llong i)    { prepare_target(); Stack[SP++] = attr.linkstatbuf->st_mode; }
void t_nlink(llong i)   { prepare_target(); Stack[SP++] = attr.linkstatbuf->st_nlink; }
void t_uid(llong i)     { prepare_target(); Stack[SP++] = attr.linkstatbuf->st_uid; }
void t_gid(llong i)     { prepare_target(); Stack[SP++] = attr.linkstatbuf->st_gid; }
void t_rdev(llong i)    { prepare_target(); Stack[SP++] = attr.linkstatbuf->st_rdev; }
void t_rmajor(llong i)  { prepare_target(); Stack[SP++] = major(attr.linkstatbuf->st_rdev); }
void t_rminor(llong i)  { prepare_target(); Stack[SP++] = minor(attr.linkstatbuf->st_rdev); }
void t_size(llong i)    { prepare_target(); Stack[SP++] = isdir(attr.linkstatbuf) ? tdirsize() : attr.linkstatbuf->st_size; }
void t_blksize(llong i) { prepare_target(); Stack[SP++] = attr.linkstatbuf->st_blksize; }
void t_blocks(llong i)  { prepare_target(); Stack[SP++] = attr.linkstatbuf->st_blocks; }
void t_atime(llong i)   { prepare_target(); Stack[SP++] = attr.linkstatbuf->st_atime; }
void t_mtime(llong i)   { prepare_target(); Stack[SP++] = attr.linkstatbuf->st_mtime; }
void t_ctime(llong i)   { prepare_target(); Stack[SP++] = attr.linkstatbuf->st_ctime; }

#ifndef NDEBUG
/*

char *instruction_name(void (*func)(llong));

Return the name of the given instruction function pointer (for debugging).

*/

char *instruction_name(void (*func)(llong))
{
	return
		(func == NULL) ? "null" :
		(func == c_le) ? "le" :
		(func == c_lt) ? "lt" :
		(func == c_ge) ? "ge" :
		(func == c_gt) ? "gt" :
		(func == c_ne) ? "ne" :
		(func == c_eq) ? "eq" :
		(func == c_bitor) ? "bitor" :
		(func == c_bitand) ? "bitand" :
		(func == c_bitxor) ? "bitxor" :
		(func == c_lshift) ? "lshift" :
		(func == c_rshift) ? "rshift" :
		(func == c_plus) ? "plus" :
		(func == c_minus) ? "minus" :
		(func == c_mul) ? "mul" :
		(func == c_div) ? "div" :
		(func == c_mod) ? "mod" :
		(func == c_not) ? "not" :
		(func == c_bitnot) ? "bitnot" :
		(func == c_uniminus) ? "uniminus" :
		(func == c_qm) ? "qm" :
		(func == c_colon) ? "colon" :
		(func == c_param) ? "param" :
		(func == c_func) ? "func" :
		(func == c_return) ? "return" :
		(func == c_number) ? "number" :
		(func == c_dev) ? "dev" :
		(func == c_major) ? "major" :
		(func == c_minor) ? "minor" :
		(func == c_ino) ? "ino" :
		(func == c_mode) ? "mode" :
		(func == c_nlink) ? "nlink" :
		(func == c_uid) ? "uid" :
		(func == c_gid) ? "gid" :
		(func == c_rdev) ? "rdev" :
		(func == c_rmajor) ? "rmajor" :
		(func == c_rminor) ? "rminor" :
		(func == c_size) ? "size" :
		(func == c_blksize) ? "blksize" :
		(func == c_blocks) ? "blocks" :
		(func == c_atime) ? "atime" :
		(func == c_mtime) ? "mtime" :
		(func == c_ctime) ? "ctime" :
		#if HAVE_ATTR || HAVE_FLAGS
		(func == c_attr) ? "cattr" :
		#endif
		#if HAVE_ATTR
		(func == c_proj) ? "cproj" :
		(func == c_gen) ? "cgen" :
		#endif
		(func == c_depth) ? "depth" :
		(func == c_prune) ? "prune" :
		(func == c_trim) ? "trim" :
		(func == c_exit) ? "exit" :
		(func == c_strlen) ? "strlen" :
		(func == c_nouser) ? "nouser" :
		(func == c_nogroup) ? "nogroup" :
		(func == c_readable) ? "readable" :
		(func == c_writable) ? "writable" :
		(func == c_executable) ? "executable" :
		(func == c_glob) ? "glob" :
		(func == c_path) ? "path" :
		(func == c_link) ? "link" :
		#ifdef FNM_CASEFOLD
		(func == c_i) ? "i" :
		(func == c_ipath) ? "ipath" :
		(func == c_ilink) ? "ilink" :
		#endif
		#ifdef HAVE_PCRE2
		(func == c_re) ? "re" :
		(func == c_repath) ? "repath" :
		(func == c_relink) ? "relink" :
		(func == c_rei) ? "rei" :
		(func == c_reipath) ? "reipath" :
		(func == c_reilink) ? "reilink" :
		#endif
		(func == c_body) ? "body" :
		#ifdef FNM_CASEFOLD
		(func == c_ibody) ? "ibody" :
		#endif
		#ifdef HAVE_PCRE2
		(func == c_rebody) ? "rebody" :
		(func == c_reibody) ? "reibody" :
		#endif
		#ifdef HAVE_MAGIC
		(func == c_what) ? "what" :
		#ifdef FNM_CASEFOLD
		(func == c_iwhat) ? "iwhat" :
		#endif
		#ifdef HAVE_PCRE2
		(func == c_rewhat) ? "rewhat" :
		(func == c_reiwhat) ? "reiwhat" :
		#endif
		#endif
		#ifdef HAVE_MAGIC
		(func == c_mime) ? "mime" :
		#ifdef FNM_CASEFOLD
		(func == c_imime) ? "imime" :
		#endif
		#ifdef HAVE_PCRE2
		(func == c_remime) ? "remime" :
		(func == c_reimime) ? "reimime" :
		#endif
		#endif
		#ifdef HAVE_ACL
		(func == c_acl) ? "acl" :
		#ifdef FNM_CASEFOLD
		(func == c_iacl) ? "iacl" :
		#endif
		#ifdef HAVE_PCRE2
		(func == c_reacl) ? "reacl" :
		(func == c_reiacl) ? "reiacl" :
		#endif
		#endif
		#ifdef HAVE_EA
		(func == c_ea) ? "ea" :
		#ifdef FNM_CASEFOLD
		(func == c_iea) ? "iea" :
		#endif
		#ifdef HAVE_PCRE2
		(func == c_reea) ? "reea" :
		(func == c_reiea) ? "reiea" :
		#endif
		#endif
		(func == c_sh) ? "sh" :
		(func == c_ush) ? "ush" :
		(func == r_exists) ? "rexists" :
		(func == r_dev) ? "rdev" :
		(func == r_major) ? "rmajor" :
		(func == r_minor) ? "rminor" :
		(func == r_ino) ? "rino" :
		(func == r_mode) ? "rmode" :
		(func == r_nlink) ? "rnlink" :
		(func == r_uid) ? "ruid" :
		(func == r_gid) ? "rgid" :
		(func == r_rdev) ? "rrdev" :
		(func == r_rmajor) ? "rrmajor" :
		(func == r_rminor) ? "rrminor" :
		(func == r_size) ? "rsize" :
		(func == r_blksize) ? "rblksize" :
		(func == r_blocks) ? "rblocks" :
		(func == r_atime) ? "ratime" :
		(func == r_mtime) ? "rmtime" :
		(func == r_ctime) ? "rctime" :
		#if HAVE_ATTR || HAVE_FLAGS
		(func == r_attr) ? "rattr" :
		#endif
		#if HAVE_ATTR
		(func == r_proj) ? "rproj" :
		(func == r_gen) ? "rgen" :
		#endif
		(func == r_strlen) ? "rstrlen" :
		(func == r_type) ? "rtype" :
		(func == r_perm) ? "rperm" :
		(func == t_exists) ? "texists" :
		(func == t_dev) ? "tdev" :
		(func == t_major) ? "tmajor" :
		(func == t_minor) ? "tminor" :
		(func == t_ino) ? "tino" :
		(func == t_mode) ? "tmode" :
		(func == t_nlink) ? "tnlink" :
		(func == t_uid) ? "tuid" :
		(func == t_gid) ? "tgid" :
		(func == t_rdev) ? "trdev" :
		(func == t_rmajor) ? "trmajor" :
		(func == t_rminor) ? "trminor" :
		(func == t_size) ? "tsize" :
		(func == t_blksize) ? "tblksize" :
		(func == t_blocks) ? "tblocks" :
		(func == t_atime) ? "tatime" :
		(func == t_mtime) ? "tmtime" :
		(func == t_ctime) ? "tctime" :
		(func == t_strlen) ? "tstrlen" :
		"unknown";
}
#endif /* not NDEBUG */

/* vi:set ts=4 sw=4: */
