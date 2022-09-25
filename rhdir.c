/*
* rawhide - find files using pretty C expressions
* https://raf.org/rawhide
* https://github.com/raforg/rawhide
*
* Copyright (C) 1990 Ken Stauffer, 2022 raf <raf@raf.org>
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
* 20220330 raf <raf@raf.org>
*/

#define _GNU_SOURCE /* For FNM_EXTMATCH and FNM_CASEFOLD in <fnmatch.h> */
#define _FILE_OFFSET_BITS 64 /* For 64-bit off_t on 32-bit systems (Not AIX) */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fnmatch.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef HAVE_ACL
#include <sys/acl.h>
#endif

#ifdef HAVE_SYS_SYSMACROS
#include <sys/sysmacros.h>
#endif

#ifdef HAVE_SYS_MKDEV
#include <sys/mkdev.h>
#endif

#ifdef HAVE_ATTR
#include <ext2fs/ext2_fs.h>
#endif

#include "rh.h"
#include "rhdata.h"
#include "rhcmds.h"
#include "rhdir.h"
#include "rhstr.h"
#include "rherr.h"

#ifdef NDEBUG
#define debug(args)
#define debug_extra(args)
#else
#define debug(args) debugf args
#define debug_extra(args) debug_extraf args
#endif

#ifndef NDEBUG
static char cwdbuf[CMDBUFSIZE];
#endif

#ifndef NDEBUG
/*

static void debugf(const char *format, ...);

Print a debug message to stderr.

*/

static void debugf(const char *format, ...)
{
	va_list args;

	if (!(attr.debug_flags & DEBUG_TRAVERSAL))
		return;

	va_start(args, format);
	fprintf(stderr, "%s: ", "traversal");
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
	va_end(args);
}

/*

static void debug_extraf(const char *format, ...);

Print an extra debug message to stderr.

*/

static void debug_extraf(const char *format, ...)
{
	va_list args;

	if (!(attr.debug_flags & (DEBUG_TRAVERSAL | DEBUG_EXTRA)))
		return;

	va_start(args, format);
	fprintf(stderr, "%s: ", "traversal");
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
	va_end(args);
}
#endif

/*

static void printf_sanitized(const char *format, ...);

Like printf(3), but if stdout is a terminal, replace
control chars with "?" to prevent terminal escape injection.

*/

static void printf_sanitized(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	if (attr.tty)
	{
		char *o;

		if (!attr.ttybuf && !(attr.ttybuf = malloc(CMDBUFSIZE)))
			fatalsys("out of memory");

		vsnprintf(attr.ttybuf, CMDBUFSIZE, format, args);

		for (o = attr.ttybuf; *o; ++o)
			putchar(isquotable(*o) ? '?' : *o);
	}
	else
		vprintf(format, args);

	va_end(args);
}

/*

static void caches_init(void);

Initialize per-file attr fields.

*/

static void caches_init(void)
{
	attr.dirsize_done = 0;
	attr.ftarget_done = 0;
	attr.linkstat_done = 0;
	attr.linkdirsize_done = 0;
	attr.facl_done = 0;
	attr.facl = NULL;
	attr.facl_verbose = NULL;
	attr.fea_done = 0;
	attr.fea_ok = 0;
	attr.fea_selinux = NULL;
	attr.fea_real = 0;
	attr.attr_done = 0;
	attr.attr = 0;
	attr.proj_done = 0;
	attr.proj = 0;
	attr.gen_done = 0;
	attr.gen = 0;
}

/*

static void caches_done(void);

Clear per-file attr fields.

*/

static void caches_done(void)
{
	#ifdef HAVE_POSIX_ACL

	if (attr.facl)
		acl_free(attr.facl);

	if (attr.facl_verbose) /* FreeBSD */
		acl_free(attr.facl_verbose);

	#endif

	#ifdef HAVE_SOLARIS_ACL

	if (attr.facl)
		free(attr.facl);

	if (attr.facl_verbose)
		free(attr.facl_verbose);

	#endif

	#ifdef HAVE_POSIX_EA

	if (attr.fea_selinux)
		free(attr.fea_selinux);

	#endif

	caches_init();
	attr.parent_fd = -1;
	attr.basename = NULL;
	attr.pruned = 0;
}

/*

int following_symlinks(void);

Return whether or not to follow the current symlink.

*/

int following_symlinks(void)
{
	return (attr.follow_symlinks == 1 && attr.depth == 0) || attr.follow_symlinks == 2;
}

/*

static int fcntl_set_fdflag(int fd, int flag);

Shorthand for setting the file descriptor flag, flag, on the file
descriptor fd, using fcntl. All other file descriptor flags are
unaffected. On success, returns 0. On error, returns -1 with errno
set by fcntl with F_GETFD or F_SETFD as the command. The only
file descriptor flag at time of writing is FD_CLOEXEC.

*/

static int fcntl_set_fdflag(int fd, int flag)
{
	int flags;

	if ((flags = fcntl(fd, F_GETFD, 0)) == -1)
		return -1;

	return fcntl(fd, F_SETFD, flags | flag);
}

/*

static void rawhide_traverse(char *nul_posp, int parent_fd, char *basename);

Traverse the directory tree, evaluating search criteria, and calling the
visit function, for each match.

The nul_posp parameter points to the nul byte at the end of attr.fpath.
There can be a trailing "/" before nul_posp (but usually not).

The parent_fd parameter is a file descriptor for the current entry's parent
directory.

The basename parameter points to the current entry's base name (often with a
trailing "/") within attr.fpath.

The parent_fd and basename parameters are used with openat() to minimize
race conditions. When called by rawhide_search() they are AT_FDCWD and NULL.

*/

static int rawhide_traverse(char *nul_posp, int parent_fd, char *basename)
{
	int dir_fd;
	DIR *dir = NULL;
	struct dirent *entry;
	size_t remaining, post_slash_nul_posi, len;
	struct stat save_statbuf[1];
	int save_followed = 0;
	int rc = 0, i;
	char *name;
	char *env;

	debug(("rawhide_traverse(fpath=%s, offset=%d, parent_fd=%d, basename=%s, depth=%d)", attr.fpath, (int)(nul_posp - attr.fpath), parent_fd, (basename) ? basename : "N/A", attr.depth));

	/* Check the depth (Can't happen: depth <= max_depth always) */

	if (attr.depth > attr.max_depth)
	{
		/* If we've reached the system limit that's an error */

		if (attr.depth > attr.depth_limit)
			return error("too many directory levels: %s", ok(attr.fpath));

		return 0;
	}

	/* Prepare for this entry (fstatat) */

	name = (parent_fd == AT_FDCWD) ? attr.fpath : basename;

	debug(("fstatat(parent_fd=%d, path=%s)", parent_fd, name));

	if ((env = getenv("RAWHIDE_TEST_FSTATAT_FAILURE")))
	{
		debug(("RAWHIDE_TEST_FSTATAT_FAILURE=%s", env));
		errno = EPERM;
	}

	if ((env && !strcmp(env, attr.fpath)) || fstatat(parent_fd, name, attr.statbuf, AT_SYMLINK_NOFOLLOW) == -1)
		return errorsys("fstatat %s", ok(attr.fpath));

	debug(("fstatat: basename %s type %o perm %o uid %d gid %d size %d", (basename) ? basename : "N/A", attr.statbuf->st_mode & S_IFMT, attr.statbuf->st_mode & ~S_IFMT, attr.statbuf->st_uid, attr.statbuf->st_gid, attr.statbuf->st_size));
	attr.followed = 0;

	/* Follow symlinks if requested. Overwrite attr.statbuf on success. */
	/* On failure, continue with the symlink's statbuf, like find(1). */

	if (islink(attr.statbuf) && following_symlinks())
	{
		if (fstatat(parent_fd, name, attr.statbuf, 0) == -1 && attr.report_broken_symlinks)
		{
			errorsys("fstatat %s (following symlink)", ok(attr.fpath));
			attr.exit_status = EXIT_FAILURE;
		}

		debug(("fstatat: basename %s type %o perm %o uid %d gid %d size %d", (basename) ? basename : "N/A", attr.statbuf->st_mode & S_IFMT, attr.statbuf->st_mode & ~S_IFMT, attr.statbuf->st_uid, attr.statbuf->st_gid, attr.statbuf->st_size));
		attr.followed = 1;
	}

	/* Test this entry and respond (if not searching depth-first) */

	if (!attr.depth_first)
	{
		caches_init();
		attr.parent_fd = parent_fd;
		attr.basename = basename;

		if (rawhide_execute() && !attr.pruned)
			if (attr.depth >= attr.min_depth)
				(*(attr.visitf))();

		caches_done();

		if (attr.exit)
			exit(attr.exit_status);
	}
	else
	{
		*save_statbuf = *attr.statbuf;
		save_followed = attr.followed;
	}

	/* Record the filesystem device number at the start, for single-filesystem traversal */

	if (attr.single_filesystem && parent_fd == AT_FDCWD)
		attr.fs_dev = attr.statbuf->st_dev;

	/* If it's a directory, process its entries */

	if (isdir(attr.statbuf) && attr.depth < attr.max_depth && !attr.prune && (!attr.single_filesystem || attr.statbuf->st_dev == attr.fs_dev))
	{
		/* Not too deep */

		if (attr.depth == attr.depth_limit)
			return error("too many directory levels: %s", ok(attr.fpath));

		/* Check for filesystem cycles (symlinks, or even hard links on OpenVMS or macOS Time Machine backups) */

		for (i = 0; i < attr.depth; ++i)
		{
			debug(("check search stack[%d] %d/%d looking for %d/%d", i, attr.search_stack[i].dev, attr.search_stack[i].ino, attr.statbuf->st_dev, attr.statbuf->st_ino));

			if (attr.search_stack[i].dev == attr.statbuf->st_dev && attr.search_stack[i].ino == attr.statbuf->st_ino)
			{
				if (attr.report_cycles)
					error("skipping %s: Filesystem cycle detected", ok(attr.fpath));

				return 0; /* This is not an error */
			}
		}

		/* Open the directory as a file descriptor to minimize race conditions */

		debug(("openat(parent_fd=%d, path=%s)", parent_fd, name));

		if ((env = getenv("RAWHIDE_TEST_OPENAT_FAILURE")))
		{
			debug(("RAWHIDE_TEST_OPENAT_FAILURE=%s", env));
			errno = EPERM;
		}

		if ((env && *env == '1') || (dir_fd = openat(parent_fd, name, O_RDONLY)) == -1)
			return errorsys("%s", ok(attr.fpath));

		debug(("openat: dir_fd %d", dir_fd));

		/* Prevent leaking this file descriptor into child processes */

		fcntl_set_fdflag(dir_fd, FD_CLOEXEC);

		/* Open the directory as a DIR to enumerate its entries */

		debug(("fdopendir(dir_fd=%d)", dir_fd));

		if ((env = getenv("RAWHIDE_TEST_FDOPENDIR_FAILURE")))
		{
			debug(("RAWHIDE_TEST_FDOPENDIR_FAILURE=%s", env));
			errno = EPERM;
		}

		if ((env && *env == '1') || !(dir = fdopendir(dir_fd)))
		{
			close(dir_fd);

			return errorsys("fdopendir %s", ok(attr.fpath));
		}

		/* Ensure that there's a trailing "/" */

		post_slash_nul_posi = nul_posp - attr.fpath;

		if (attr.fpath[post_slash_nul_posi - 1] != '/')
		{
			if (post_slash_nul_posi + 1 >= attr.fpath_size)
			{
				closedir(dir); /* This closes pid_fd */

				return error("path is too long: %s/", ok(attr.fpath));
			}

			attr.fpath[post_slash_nul_posi++] = '/';
			attr.fpath[post_slash_nul_posi] = '\0';
		}

		remaining = attr.fpath_size - post_slash_nul_posi;

		/* Descend into the directory */

		attr.search_stack[attr.depth].dev = attr.statbuf->st_dev;
		attr.search_stack[attr.depth].ino = attr.statbuf->st_ino;
		attr.depth += 1;

		/* Apply recursively to this directory's entries */

		while ((entry = readdir(dir)))
		{
			if (entry->d_name[0] == '.' && (!entry->d_name[1] || (entry->d_name[1] == '.' && !entry->d_name[2])))
				continue;

			if ((len = strlcpy(attr.fpath + post_slash_nul_posi, entry->d_name, remaining)) >= remaining)
			{
				attr.fpath[post_slash_nul_posi] = '\0';
				error("path is too long: %s%s", ok(attr.fpath), ok2(entry->d_name));
				rc = -1;

				continue;
			}

			debug(("dir entry %s", attr.fpath));

			if (rawhide_traverse(attr.fpath + post_slash_nul_posi + len, (dir_fd == -1) ? AT_FDCWD : dir_fd, attr.fpath + post_slash_nul_posi) == -1)
				rc = -1;
		}

		closedir(dir);

		/* Ascend from the directory */

		attr.depth -= 1;

		/* Remove the trailing slash and directory entry name */

		*nul_posp = '\0';
	}

	/* Test this entry and respond (if searching depth-first) */

	if (attr.depth_first)
	{
		caches_init();
		attr.parent_fd = parent_fd;
		attr.basename = basename;
		*attr.statbuf = *save_statbuf;
		attr.followed = save_followed;

		if (rawhide_execute() && !attr.pruned)
			if (attr.depth >= attr.min_depth)
				(*(attr.visitf))();

		caches_done();

		if (attr.exit)
			exit(attr.exit_status);
	}

	/* Don't prune siblings */

	attr.prune = 0;

	return rc;
}

/*

llong env_int(char *envname, llong min_value, llong max_value, llong default);

Get an integer from an environment variable. Check that it is within
required limits. Ignore invalid values.

*/

llong env_int(char *envname, llong min_value, llong max_value, llong default_value)
{
	char *env, *endptr;
	llong e;

	if ((env = getenv(envname)) && *env)
	{
		errno = 0;
		e = strtoll(env, &endptr, 10);

		if ((endptr && !*endptr) && e != 0 && errno != ERANGE)
			if ((min_value == -1 || e >= min_value) && (max_value == -1 || e <= max_value))
				return e;
	}

	return default_value;
}

/*

int rawhide_search(char *fpath);

Search for filesystem entries that satisfy the search criteria.
Initialize global attr runtime state, then call rawhide_traverse().
The fpath parameter is the search path.

*/

int rawhide_search(char *fpath)
{
	llong max_pathlen;
	char *slash_posp;
	int rc = 0;
	size_t len;

	debug(("rawhide_search(fpath=%s)", fpath));

	/* Remove any trailing slashes */

	while ((slash_posp = strrchr(fpath, '/')) && !slash_posp[1] && slash_posp > fpath)
		*slash_posp = '\0';

	/* Determine the maximum path length on this filesystem */

	max_pathlen = pathconf(fpath, _PC_PATH_MAX);
	max_pathlen = (max_pathlen == -1) ? 1024 : max_pathlen + 2;
	max_pathlen = env_int("RAWHIDE_TEST_PATHLEN_MAX", 1, max_pathlen, max_pathlen);

	/* Initialize state */

	if (!(attr.fpath = malloc(max_pathlen + 1)))
		return error("out of memory");

	attr.search_path = fpath;
	attr.search_path_len = strlen(fpath);
	attr.fpath_size = max_pathlen + 1;
	attr.depth = 0;
	attr.prune = 0;
	attr.exit = 0;

	attr.fs_dev = (dev_t)0;

	if ((len = strlcpy(attr.fpath, fpath, attr.fpath_size)) >= attr.fpath_size)
	{
		error("path is too long: %s", ok(fpath));
		free(attr.fpath);

		return -1;
	}

	/* Search */

	rc = rawhide_traverse(attr.fpath + len, AT_FDCWD, NULL);

	/* Cleanup */

	free(attr.fpath);

	#ifdef HAVE_PCRE2
	pcre2_cache_free();
	#endif

	if (attr.ftarget)
	{
		free(attr.ftarget);
		attr.ftarget = NULL;
	}

	if (attr.formatbuf)
	{
		free(attr.formatbuf);
		attr.formatbuf = NULL;
	}

	if (attr.fea)
	{
		free(attr.fea);
		attr.fea = NULL;
	}

	if (attr.ttybuf)
	{
		free(attr.ttybuf);
		attr.ttybuf = NULL;
	}

	return rc;
}

/*

void visitf_default(void);

The default action for matching files (without -l).
It prints the name.

*/

void visitf_default(void)
{
	static char buf[CMDBUFSIZE];
	static char linkbuf[CMDBUFSIZE];
	int pos = 0, i;

	if (attr.quote_name)
		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "\"");

	if (attr.escape_name)
	{
		pos += cescape(buf + pos, CMDBUFSIZE - pos, attr.fpath, -1, attr.quote_name ? CESCAPE_QUOTES : 0);
	}
	else if (attr.mask_name || attr.tty)
	{
		for (i = 0; attr.fpath[i]; ++i)
			pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "%s%c", (attr.quote_name && (attr.fpath[i] == '"' || attr.fpath[i] == '\\')) ? "\\" : "", (isquotable(attr.fpath[i])) ? '?' : attr.fpath[i]);
	}
	else if (attr.quote_name)
	{
		for (i = 0; attr.fpath[i]; ++i)
			pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "%s%c", (attr.fpath[i] == '"' || attr.fpath[i] == '\\') ? "\\" : "", attr.fpath[i]);
	}
	else
	{
		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "%s", attr.fpath);
	}

	if (attr.quote_name)
		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "\"");

	if (isdir(attr.statbuf) && (attr.dir_indicator || attr.most_indicators || attr.all_indicators))
		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "/");
	else if (islink(attr.statbuf) && (attr.most_indicators || attr.all_indicators))
		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "@");
	else if (issock(attr.statbuf) && (attr.most_indicators || attr.all_indicators))
		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "=");
	else if (isfifo(attr.statbuf) && (attr.most_indicators || attr.all_indicators))
		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "|");
	else if (isdoor(attr.statbuf) && (attr.most_indicators || attr.all_indicators))
		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, ">");
	else if (attr.statbuf->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH) && attr.all_indicators)
		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "*");

	if (islink(attr.statbuf) && attr.visitf == visitf_long)
	{
		ssize_t nbytes;

		if ((nbytes = readlinkat(attr.parent_fd, (attr.parent_fd == AT_FDCWD) ? attr.fpath : attr.basename, linkbuf, CMDBUFSIZE)) == -1)
			errorsys("readlinkat %s", ok(attr.fpath));
		else if (nbytes == CMDBUFSIZE)
			error("readlinkat %s: target is too long", ok(attr.fpath));
		else
		{
			linkbuf[nbytes] = '\0';
			pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, " -> %s", linkbuf);
		}
	}

	printf_sanitized("%s", buf);
	putchar(attr.nul ? '\0' : '\n');
}

/*

static int scale_units_round_up(char *buf, int size, ullong num, int kilo, int width, int shorter);

Writes to the given buffer buf, of size size, the given num expressed in
"human readable" units. The kilo parameter can be 1000 or 1024. When it's
1024, the result is expressed in terms of traditional storage units
(e.g., KiB, MiB, GiB, TiB for the -H option). When it's 1000, the result is
expressed in terms of SI-style units (e.g., KB, MB, GB, TB for the -I
option). Returns the number of bytes written to the buffer, excluding the
terminating nul byte.

This function mimics ls(1) behaviour (for the sake of familiarity).
When the number is below ten, a decimal place is shown. Otherwise,
it's an integer. Amounts are rounded up to the chosen precision.
This gives the property that files are never larger than the size
reported, but the difference between the actual size and the
reported size can be large.

This differs from ls(1) in that when kilo is 1000 the units are always
lower case, rather than only being lower case for KB.

*/

static int scale_units_round_up(char *buf, int size, ullong num, int kilo, int width, int shorter)
{
	static char *units = "-KMGTPE";
	ullong scale = 1, div, mod;
	int poweri = 0;

	#define u (kilo == 1000) ? tolower(units[poweri]) : units[poweri]
	#define unext (kilo == 1000) ? tolower(units[poweri + 1]) : units[poweri + 1]

	/* Numbers below 1000/1024 are just numbers without units */

	if (num < kilo)
		return ssnprintf(buf, size, "%*llu", width, num);

	/* Otherwise, determine the appropriate scale */

	while (poweri < strlen(units) - 1)
	{
		scale *= kilo;
		++poweri;

		div = num / scale;
		mod = num % scale;

		/* Below 10, we show one decimal place (rounded up to that decimal place) */

		if (div < 10)
		{
			/* Multiplying mod by 10 here makes it match ls(1) behaviour when kilo == 1024 */

			ullong tenths = (mod * 10) / scale + (((mod * 10) % scale) != 0);

			if (tenths == 10)
			{
				++div;
				tenths = 0;
			}

			/* If shorter (for blksize and space) don't show ".0" */

			if (div < 10 && (tenths || !shorter))
				return ssnprintf(buf, size, "%*llu.%llu%c", (width > 3) ? width - 3 : width, div, tenths, u);

			return ssnprintf(buf, size, "%*llu%c", (width) ? width - 1 : 0, div, u);
		}

		/* At 10 or above, show an integer (rounded up) */

		else if (div < kilo)
		{
			if (div + (mod != 0) == kilo && poweri < strlen(units) - 1)
				return ssnprintf(buf, size, "1.0%c", unext);

			return ssnprintf(buf, size, "%*llu%c", (width) ? width - 1 : 0, div + (mod != 0), u);
		}

		/* Move up to the next scale */
	}

	/* Exabytes/Exbibytes (need 128 bits to go any higher) */

	return ssnprintf(buf, size, "%*llu%c", (width) ? width - 1 : 0, num, u);
	#undef u
	#undef unext
}

/*

static int scale_units_round_half_up(char *buf, int size, ullong num, int kilo, int width, int shorter);

Writes to the given buffer buf, of size size, the given num expressed in
"human readable" units. The kilo parameter can be 1000 or 1024. When it's
1024, the result is expressed in terms of traditional storage units
(e.g., KiB, MiB, GiB, TiB for the -H option). When it's 1000, the result is
expressed in terms of SI-style units (e.g., KB, MB, GB, TB for the -I
option). Returns the number of bytes written to the buffer, excluding the
terminating nul byte.

When the number is below ten, a decimal place is shown. Otherwise, it's an
integer. Amounts are rounded half up to the chosen precision rather than
always rounding up. This is a more accurate alternative to the default
familiar ls(1) behaviour (just in case that matters to anyone).

This differs from ls(1) in that when kilo is 1000 the units are always
lower case, rather than only being lower case for KB.

*/

static int scale_units_round_half_up(char *buf, int size, ullong num, int kilo, int width, int shorter)
{
	static char *units = "-KMGTPE";
	ullong scale = 1, div, mod;
	int poweri = 0;

	#define u (kilo == 1000) ? tolower(units[poweri]) : units[poweri]
	#define unext (kilo == 1000) ? tolower(units[poweri + 1]) : units[poweri + 1]

	/* Numbers below 1000/1024 are just numbers without units */

	if (num < kilo)
		return ssnprintf(buf, size, "%*llu", width, num);

	/* Otherwise, determine the appropriate scale */

	while (poweri < strlen(units) - 1)
	{
		scale *= kilo;
		++poweri;

		div = num / scale;
		mod = num % scale;

		/* Below 10, we show one decimal place (rounded to that decimal place). */
		/* This nonsense is to avoid needing to link against the math library. */

		if (div < 10)
		{
			ullong tenth = 10 * scale / 10; /* times ten to avoid double */
			ullong min_distance = 10 * scale;
			ullong tenths = 0, t;

			for (t = 0; t <= 10; ++t)
			{
				llong distance = 10 * mod - t * tenth;

				if (distance < 0)
					distance = -distance;

				if (distance > min_distance)
					break;

				min_distance = distance;
				tenths = t;
			}

			if (tenths == 10)
			{
				++div;
				tenths = 0;
			}

			/* If shorter (for blksize and space) don't show ".0" */

			if (div < 10 && (tenths || !shorter))
				return ssnprintf(buf, size, "%*llu.%llu%c", (width > 3) ? width - 3 : width, div, tenths, u);

			return ssnprintf(buf, size, "%*llu%c", (width) ? width - 1 : 0, div, u);
		}

		/* At 10 or above, show an integer (rounded) */

		else if (div < kilo)
		{
			if (mod >= scale - mod)
				++div;

			if (div == kilo && poweri < strlen(units) - 1)
				return ssnprintf(buf, size, "1.0%c", unext);

			return ssnprintf(buf, size, "%*llu%c", (width) ? width - 1 : 0, div, u);
		}

		/* Move up to the next scale */
	}

	/* Exabytes/Exbibytes (need 128 bits to go any higher) */

	return ssnprintf(buf, size, "%*llu%c", (width) ? width - 1 : 0, num, u);
	#undef u
	#undef unext
}

/*

static char *ftypecode(struct stat *statbuf);

Return a pointer to a static buffer containing the file type code in ls -l
format.

*/

static char *ftypecode(struct stat *statbuf)
{
	#ifndef S_IFDOOR
	static char *ftype[] =
	{
		"p", "c", "d", "b", "-", "l", "s", "?" /* The 1990 version had "t" at the end. I wonder why. */
	};
	#define ftype_index(statbuf) ((statbuf)->st_mode >> 13)
	#else
	static char *ftype[] =
	{
		"?", "p", "c", "?", "d", "?", "b", "?",
		"-", "?", "l", "?", "s", "D" , "P", "?" /* "P" is a Solaris event port which can't happen */
	};
	#define ftype_index(statbuf) ((statbuf)->st_mode >> 12)
	#endif

	return ftype[ftype_index(statbuf)];
}

/*

static const char *ytypecode(struct stat *statbuf);

Return a pointer to a static buffer containing the file type code in ls -l
format, but with f for regular files (for -L %y and %Y).

*/

static const char *ytypecode(struct stat *statbuf)
{
	char *code = ftypecode(statbuf);

	return (*code == '-') ? "f" : code;
}

/*

const char *modestr(struct stat *statbuf);

Return a pointer to a static buffer containing the type and
permissions in symbolic ls -l format.

*/

const char *modestr(struct stat *statbuf)
{
	#define MODESIZE 11
	static char modebuf[MODESIZE];

	static char *ug_perm[] =
	{
		"---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx",
		"--S", "--s", "-wS", "-ws", "r-S", "r-s", "rwS", "rws"
	};

	static char *o_perm[] =
	{
		"---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx",
		"--T", "--t", "-wT", "-wt", "r-T", "r-t", "rwT", "rwt"
	};

	#define user_index(statbuf)  (((0777 & (statbuf)->st_mode) >> 6) + ((statbuf)->st_mode & S_ISUID ? 8 : 0))
	#define group_index(statbuf) (((0077 & (statbuf)->st_mode) >> 3) + ((statbuf)->st_mode & S_ISGID ? 8 : 0))
	#define other_index(statbuf) (((0007 & (statbuf)->st_mode) >> 0) + ((statbuf)->st_mode & S_ISVTX ? 8 : 0))

	snprintf(modebuf, MODESIZE, "%s%s%s%s",
		ftypecode(statbuf),
		ug_perm[user_index(statbuf)],
		ug_perm[group_index(statbuf)],
		o_perm[other_index(statbuf)]
	);

	return modebuf;
}

/*

static const char *aclea(void);

Return a pointer to a static buffer containing the ACL/EA indicator.

*/

static const char *aclea(void)
{
	int acl = has_real_acl();
	int ea = has_real_ea();

	return acl && ea ? "*" : ea ? "@" : acl ? "+" : attr.fea_selinux ? "." : " ";
}

/*

void visitf_long(void);

The -l action for matching files. It prints the name and other details. The
potential columns included are:

  dev, ino, blksize, blocks, space, mode (type+perm+aclea), nlink,
  user/owner, group, size/rdev, mtime, atime, ctime, path

Note that output isn't sorted, so we don't know how wide each column should
be. We start with small default widths, and grow wider as needed.

*/

void visitf_long(void)
{
	static char buf[CMDBUFSIZE];
	int pos = 0, ncols = 0, w;

	/* Initialize column widths */

	if (attr.dev_major_column_width == 0)
	{
		attr.dev_major_column_width = env_int("RAWHIDE_COLUMN_WIDTH_DEV_MAJOR", 1, 99, 1);
		attr.dev_minor_column_width = env_int("RAWHIDE_COLUMN_WIDTH_DEV_MINOR", 1, 99, 1);
		attr.ino_column_width = env_int("RAWHIDE_COLUMN_WIDTH_INODE", 1, 99, 6);
		attr.blksize_column_width = env_int("RAWHIDE_COLUMN_WIDTH_BLKSIZE", 1, 99, 1);
		attr.blocks_column_width = env_int("RAWHIDE_COLUMN_WIDTH_BLOCKS", 1, 99, 2);
		attr.space_column_width = (attr.human_units || attr.si_units) ? env_int("RAWHIDE_COLUMN_WIDTH_SPACE_UNITS", 1, 99, 4) : env_int("RAWHIDE_COLUMN_WIDTH_SPACE", 1, 99, 6);
		attr.nlink_column_width = env_int("RAWHIDE_COLUMN_WIDTH_NLINK", 1, 99, 1);
		attr.user_column_width = env_int("RAWHIDE_COLUMN_WIDTH_USER", 1, 99, 3);
		attr.group_column_width = env_int("RAWHIDE_COLUMN_WIDTH_GROUP", 1, 99, 3);
		attr.size_column_width = (attr.human_units || attr.si_units) ? env_int("RAWHIDE_COLUMN_WIDTH_SIZE_UNITS", 1, 99, 4) : env_int("RAWHIDE_COLUMN_WIDTH_SIZE", 1, 99, 6);
		attr.rdev_major_column_width = env_int("RAWHIDE_COLUMN_WIDTH_RDEV_MAJOR", 1, 99, 2);
		attr.rdev_minor_column_width = env_int("RAWHIDE_COLUMN_WIDTH_RDEV_MINOR", 1, 99, 3);
	}

	/* The dev column */

	if (attr.dev_column || attr.verbose)
	{
		if ((w = ssnprintf(buf + pos, CMDBUFSIZE - pos, "%*llu", attr.dev_major_column_width, (ullong)major(attr.statbuf->st_dev))) > attr.dev_major_column_width)
			attr.dev_major_column_width = w;

		pos += w;

		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, ",");

		if ((w = ssnprintf(buf + pos, CMDBUFSIZE - pos, "%*llu", attr.dev_minor_column_width, (ullong)minor(attr.statbuf->st_dev))) > attr.dev_minor_column_width)
			attr.dev_minor_column_width = w;

		pos += w;
		++ncols;
	}

	/* The ino column */

	if (attr.ino_column || attr.verbose)
	{
		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "%s", (ncols) ? " " : "");

		if ((w = ssnprintf(buf + pos, CMDBUFSIZE - pos, "%*llu", attr.ino_column_width, (ullong)attr.statbuf->st_ino)) > attr.ino_column_width)
			attr.ino_column_width = w;

		pos += w;
		++ncols;
	}

	/* The blksize column */

	if (attr.blksize_column || attr.verbose)
	{
		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "%s", (ncols) ? " " : "");

		if (attr.human_units == 1)
		{
			if ((w = scale_units_round_up(buf + pos, CMDBUFSIZE - pos, (ullong)attr.statbuf->st_blksize, 1024, attr.blksize_column_width, 1)) > attr.blksize_column_width)
				attr.blksize_column_width = w;
		}
		else if (attr.human_units > 1)
		{
			if ((w = scale_units_round_half_up(buf + pos, CMDBUFSIZE - pos, (ullong)attr.statbuf->st_blksize, 1024, attr.blksize_column_width, 1)) > attr.blksize_column_width)
				attr.blksize_column_width = w;
		}
		else if (attr.si_units == 1)
		{
			if ((w = scale_units_round_up(buf + pos, CMDBUFSIZE - pos, (ullong)attr.statbuf->st_blksize, 1000, attr.blksize_column_width, 1)) > attr.blksize_column_width)
				attr.blksize_column_width = w;
		}
		else if (attr.si_units > 1)
		{
			if ((w = scale_units_round_half_up(buf + pos, CMDBUFSIZE - pos, (ullong)attr.statbuf->st_blksize, 1000, attr.blksize_column_width, 1)) > attr.blksize_column_width)
				attr.blksize_column_width = w;
		}
		else
		{
			if ((w = ssnprintf(buf + pos, CMDBUFSIZE - pos, "%*llu", (ullong)attr.blksize_column_width, (ullong)attr.statbuf->st_blksize)) > attr.blksize_column_width)
				attr.blksize_column_width = w;
		}

		pos += w;
		++ncols;
	}

	/* The blocks column */

	if (attr.blocks_column || attr.verbose)
	{
		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "%s", (ncols) ? " " : "");

		if ((w = ssnprintf(buf + pos, CMDBUFSIZE - pos, "%*llu", attr.blocks_column_width, (ullong)attr.statbuf->st_blocks)) > attr.blocks_column_width)
			attr.blocks_column_width = w;

		pos += w;
		++ncols;
	}

	/* The space column */

	if (attr.space_column || attr.verbose)
	{
		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "%s", (ncols) ? " " : "");

		if (attr.human_units == 1)
		{
			if ((w = scale_units_round_up(buf + pos, CMDBUFSIZE - pos, (ullong)attr.statbuf->st_blocks * 512, 1024, attr.space_column_width, 1)) > attr.space_column_width)
				attr.space_column_width = w;
		}
		else if (attr.human_units > 1)
		{
			if ((w = scale_units_round_half_up(buf + pos, CMDBUFSIZE - pos, (ullong)attr.statbuf->st_blocks * 512, 1024, attr.space_column_width, 1)) > attr.space_column_width)
				attr.space_column_width = w;
		}
		else if (attr.si_units == 1)
		{
			if ((w = scale_units_round_up(buf + pos, CMDBUFSIZE - pos, (ullong)attr.statbuf->st_blocks * 512, 1000, attr.space_column_width, 1)) > attr.space_column_width)
				attr.space_column_width = w;
		}
		else if (attr.si_units > 1)
		{
			if ((w = scale_units_round_half_up(buf + pos, CMDBUFSIZE - pos, (ullong)attr.statbuf->st_blocks * 512, 1000, attr.space_column_width, 1)) > attr.space_column_width)
				attr.space_column_width = w;
		}
		else
		{
			if ((w = ssnprintf(buf + pos, CMDBUFSIZE - pos, "%*llu", attr.space_column_width, (ullong)attr.statbuf->st_blocks * 512)) > attr.space_column_width)
				attr.space_column_width = w;
		}

		pos += w;
		++ncols;
	}

	/* The mode (type/perm) column */

	pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "%s%s", (ncols) ? " " : "", modestr(attr.statbuf));
	++ncols;

	/* The ACL/EA indicator */

	pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "%s", aclea());

	/* The nlink column */

	pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "%s", (ncols) ? " " : "");

	if ((w = ssnprintf(buf + pos, CMDBUFSIZE - pos, "%*llu", attr.nlink_column_width, (ullong)attr.statbuf->st_nlink)) > attr.nlink_column_width)
		attr.nlink_column_width = w;

	pos += w;
	++ncols;

	/* The user/owner column */

	if (!attr.no_owner_column)
	{
		struct passwd *pwd = getpwuid(attr.statbuf->st_uid);

		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "%s", (ncols) ? " " : "");

		if (attr.numeric_ids || !pwd)
		{
			if ((w = ssnprintf(buf + pos, CMDBUFSIZE - pos, "%*llu", attr.user_column_width, (ullong)attr.statbuf->st_uid)) > attr.user_column_width)
				attr.user_column_width = w;
		}
		else
		{
			if ((w = ssnprintf(buf + pos, CMDBUFSIZE - pos, "%-*s", attr.user_column_width, pwd->pw_name)) > attr.user_column_width)
				attr.user_column_width = w;
		}

		pos += w;
		++ncols;
	}

	/* The group column */

	if (!attr.no_group_column)
	{
		struct group *grp = getgrgid(attr.statbuf->st_gid);

		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "%s", (ncols) ? " " : "");

		if (attr.numeric_ids || !grp)
		{
			if ((w = ssnprintf(buf + pos, CMDBUFSIZE - pos, "%*llu", attr.group_column_width, (ullong)attr.statbuf->st_gid)) > attr.group_column_width)
				attr.group_column_width = w;
		}
		else
		{
			if ((w = ssnprintf(buf + pos, CMDBUFSIZE - pos, "%-*s", attr.group_column_width, grp->gr_name)) > attr.group_column_width)
				attr.group_column_width = w;
		}

		pos += w;
		++ncols;
	}

	/* The size/rdev column */

	if (ischr(attr.statbuf) || isblk(attr.statbuf))
	{
		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "%s", (ncols) ? " " : "");

		if ((w = ssnprintf(buf + pos, CMDBUFSIZE - pos, "%*llu", attr.rdev_major_column_width, (ullong)major(attr.statbuf->st_rdev))) > attr.rdev_major_column_width)
			attr.rdev_major_column_width = w;

		pos += w;

		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, ",");

		if ((w = ssnprintf(buf + pos, CMDBUFSIZE - pos, "%*llu", attr.rdev_minor_column_width, (ullong)minor(attr.statbuf->st_rdev))) > attr.rdev_minor_column_width)
			attr.rdev_minor_column_width = w;

		if (attr.rdev_major_column_width + 1 + attr.rdev_minor_column_width > attr.size_column_width)
			attr.size_column_width = attr.rdev_major_column_width + 1 + attr.rdev_minor_column_width;
	}
	else
	{
		set_dirsize(); /* Make st_size for directories more useful */

		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "%s", (ncols) ? " " : "");

		if (attr.human_units == 1)
		{
			if ((w = scale_units_round_up(buf + pos, CMDBUFSIZE - pos, (ullong)attr.statbuf->st_size, 1024, attr.size_column_width, 0)) > attr.size_column_width)
				attr.size_column_width = w;
		}
		else if (attr.human_units > 1)
		{
			if ((w = scale_units_round_half_up(buf + pos, CMDBUFSIZE - pos, (ullong)attr.statbuf->st_size, 1024, attr.size_column_width, 0)) > attr.size_column_width)
				attr.size_column_width = w;
		}
		else if (attr.si_units == 1)
		{
			if ((w = scale_units_round_up(buf + pos, CMDBUFSIZE - pos, (ullong)attr.statbuf->st_size, 1000, attr.size_column_width, 0)) > attr.size_column_width)
				attr.size_column_width = w;
		}
		else if (attr.si_units > 1)
		{
			if ((w = scale_units_round_half_up(buf + pos, CMDBUFSIZE - pos, (ullong)attr.statbuf->st_size, 1000, attr.size_column_width, 0)) > attr.size_column_width)
				attr.size_column_width = w;
		}
		else
		{
			if ((w = ssnprintf(buf + pos, CMDBUFSIZE - pos, "%*llu", attr.size_column_width, (ullong)attr.statbuf->st_size)) > attr.size_column_width)
				attr.size_column_width = w;
		}

		if (attr.size_column_width > attr.rdev_major_column_width + 1 + attr.rdev_minor_column_width)
			attr.rdev_minor_column_width = attr.size_column_width - 1 - attr.rdev_major_column_width;
	}

	pos += w;
	++ncols;

	/* The mtime column */

	if ((!attr.atime_column && !attr.ctime_column) || attr.verbose)
	{
		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "%s", (ncols) ? " " : "");
		pos += strftime(buf + pos, CMDBUFSIZE - pos, (attr.iso_time) ? "%Y-%m-%d %H:%M:%S %z" : "%b %e %H:%M:%S %Y", localtime(&attr.statbuf->st_mtime));
		++ncols;
	}

	/* The atime column */

	if (attr.atime_column || attr.verbose)
	{
		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "%s", (ncols) ? " " : "");
		pos += strftime(buf + pos, CMDBUFSIZE - pos, (attr.iso_time) ? "%Y-%m-%d %H:%M:%S %z" : "%b %e %H:%M:%S %Y", localtime(&attr.statbuf->st_atime));
		++ncols;
	}

	/* The ctime column */

	if (attr.ctime_column || attr.verbose)
	{
		pos += ssnprintf(buf + pos, CMDBUFSIZE - pos, "%s", (ncols) ? " " : "");
		pos += strftime(buf + pos, CMDBUFSIZE - pos, (attr.iso_time) ? "%Y-%m-%d %H:%M:%S %z" : "%b %e %H:%M:%S %Y", localtime(&attr.statbuf->st_ctime));
		++ncols;
	}

	/* The path column */

	printf("%s ", buf);
	visitf_default();
}

/*

int interpolate_command(const char *srccmd, char *command, int cmdbufsize);

Interpolate the current file path/name into attr.command, storing the result
in the given command buffer of size cmdbufsize. Return 0 on success, or -1
on error after emitting an error message and setting attr.exit_status to
EXIT_FAILURE.

*/

int interpolate_command(const char *srccmd, char *command, int cmdbufsize)
{
	const char *src;
	char *dst, *f;

	src = srccmd;
	dst = command;
	cmdbufsize = env_int("RAWHIDE_TEST_CMD_MAX", 1, cmdbufsize, cmdbufsize);

	while (*src)
	{
		if (*src != '%')
		{
			if (dst + 1 - command >= cmdbufsize)
			{
				error("command is too big: %s (%s)", ok(srccmd), ok2(attr.fpath));
				attr.exit_status = EXIT_FAILURE;

				return -1;
			}

			*dst++ = *src++;
		}
		else
		{
			++src;

			if (*src == 's')
			{
				/* Interpolate the file path into the command */

				f = attr.fpath;

				/* While there's room for a nul or a byte followed by a nul... */

				while (dst + ((*f) ? 1 : 0) - command < cmdbufsize)
				{
					/* Quote shell meta-characters in the path */

					if (*f && !isalnum((int)(unsigned char)*f) && !strchr("-+,./@_", *f) && !(*f & 0x80))
					{
						*dst++ = '\\';

						/* Is there still room for a byte followed by a nul? */

						if (dst + 1 - command >= cmdbufsize)
						{
							error("command is too big: %s (%s)", ok(srccmd), ok2(attr.fpath));
							attr.exit_status = EXIT_FAILURE;

							return -1;
						}
					}

					/* Copy the (possibly quoted meta) character, stopping after nul */

					if (!(*dst++ = *f++))
					{
						/* Point dst and f back to the nul */

						--dst;
						--f;

						break;
					}
				}

				/* Were we able to completely interpolate the file path? */

				if (*f)
				{
					error("command is too big: %s (%s)", ok(srccmd), ok2(attr.fpath));
					attr.exit_status = EXIT_FAILURE;

					return -1;
				}
			}
			else if (*src == 'S')
			{
				/* Interpolate the file base name (or / for /) into the command */

				f = strrchr(attr.fpath, '/');
				f = (f == attr.fpath && !f[1]) ? attr.fpath : (f) ? f + 1 : attr.fpath;

				/* While there's room for a null or a byte followed by a nul... */

				while (dst + ((*f) ? 1 : 0) - command < cmdbufsize)
				{
					/* Quote shell meta-characters in the path */

					if (*f && !isalnum((int)(unsigned char)*f) && !strchr("-+,./@_", *f) && !(*f & 0x80))
					{
						*dst++ = '\\';

						/* Is there still room for a byte followed by a nul? */

						if (dst + 1 - command >= cmdbufsize)
						{
							error("command is too big: %s (%s)", ok(srccmd), ok2(attr.fpath));
							attr.exit_status = EXIT_FAILURE;

							return -1;
						}
					}

					/* Copy the (possibly quoted meta) character, stopping after nul */

					if (!(*dst++ = *f++))
					{
						/* Point dst and f back to the nul */

						--dst;
						--f;

						break;
					}
				}

				/* Were we able to completely interpolate the file name? */

				if (*f)
				{
					error("command is too big: %s (%s)", ok(srccmd), ok2(attr.fpath));
					attr.exit_status = EXIT_FAILURE;

					return -1;
				}
			}
			else if (*src == '%')
			{
				/* Is there room for a byte followed by a nul? */

				if (dst + 1 - command >= cmdbufsize)
				{
					error("command is too big: %s (%s)", ok(srccmd), ok2(attr.fpath));
					attr.exit_status = EXIT_FAILURE;

					return -1;
				}

				/* Copy the literal % */

				*dst++ = *src;
			}
			else
			{
				fatal("invalid shell command: %s (after '%%' expected 's', 'S' or '%%')", ok(srccmd));
			}

			++src;
		}
	}

	*dst = '\0';

	return 0;
}

/*

void visitf_execute(void);

The -x action for matching files. It executes a shell command. Occurrences
of %s in the command are replaced with the matching file's full path
relative to the search directory. Occurrences of %S in the command are
replaced with the base name of the matching file. In both cases, all shell
meta-characters are backslash-quoted to prevent shell command injection.
Occurrences of %% in the command are replaced with a single %. A % that
isn't followed by s, S, or % is invalid.

*/

void visitf_execute(void)
{
	static char command[CMDBUFSIZE];

	if (interpolate_command(attr.command, command, CMDBUFSIZE) == -1)
		return;

	debug(("command: %s", command));

	if (attr.verbose)
	{
		printf_sanitized("%s", command);
		putchar('\n');
		fflush(stdout);
	}

	if (syscmd(command))
		attr.exit_status = EXIT_FAILURE;
}

/*

int chdir_local(int do_debug);

Perform the first chdir (before fchdir starts happening).
Returns dot_fd which must be closed later unless it is -1.
If do_debug is true, emit (traversal) error messages.
Leave this off for c_sh().
There is fault injection for testing.

*/

int chdir_local(int do_debug)
{
	char *slash_posp, *dir_path;
	int dot_fd = -1;
	char *env;

	/* If in a sub-directory (has parent_id and basename) */

	if (attr.parent_fd != AT_FDCWD && attr.parent_fd != -1)
	{
		if (do_debug)
			debug(("fchdir(cwd=%s parent_fd=%d) [%.*s]", getcwd(cwdbuf, CMDBUFSIZE), attr.parent_fd, attr.basename - attr.fpath - 1, attr.fpath));

		if ((env = getenv("RAWHIDE_TEST_FCHDIR_FAILURE")))
		{
			debug(("RAWHIDE_TEST_FCHDIR_FAILURE=%s", env));
			errno = EPERM;
		}

		if ((env && *env == '1') || fchdir(attr.parent_fd) == -1)
			fatalsys("fchdir %.*s", attr.basename - attr.fpath - 1, attr.fpath);
	}
	else /* Starting search directory (no parent_id or basename) */
	{
		/* Only chdir if the initial search directory is not in cwd */

		if ((slash_posp = strrchr(attr.fpath, '/')))
		{
			/* We'll need to cd back afterwards, so that the first openat can work later */

			if ((dot_fd = open(".", O_RDONLY)) == -1)
				fatalsys("open .");

			/* Prevent leaking this file descriptor into child processes */

			fcntl_set_fdflag(dot_fd, FD_CLOEXEC);

			/* Change directory */

			if (!(dir_path = malloc(attr.fpath_size)))
				fatalsys("out of memory");

			strlcpy(dir_path, attr.fpath, slash_posp - attr.fpath + 1 + (slash_posp == attr.fpath));

			if (do_debug)
				debug(("initial chdir(cwd=%s path=%s) fpath=%s", getcwd(cwdbuf, CMDBUFSIZE), dir_path, attr.fpath));

			if ((env = getenv("RAWHIDE_TEST_CHDIR_FAILURE")))
			{
				if (do_debug)
					debug(("RAWHIDE_TEST_CHDIR_FAILURE=%s", env));

				errno = EPERM;
			}

			if ((env && *env == '1') || chdir(dir_path) == -1)
				fatalsys("chdir %s", ok(dir_path));

			free(dir_path);
		}
		else
		{
			if (do_debug)
				debug(("initial search directory is local, so no chdir"));
		}
	}

	return dot_fd;
}

/*

void visitf_execute_local(void);

The -X action for matching files. It executes a shell command. This is like
the -x option, except that each command is executed from the directory
containing the matching file. It avoids race conditions that make the -x
option insecure.

Occurrences of %s in the command are replaced with the matching entry's
full path (relative to the starting search directory). Occurrences of %S
in the command are replaced with the base name of the matching entry (or
with "/" when the matching entry is the root directory which has no base
name). In both cases, all shell meta-characters are backslash-quoted to
prevent shell command injection. Occurrences of %% in the command are
replaced with a single %. A % that isn't followed by s, S, or % is
invalid.

*/

void visitf_execute_local(void)
{
	static char command[CMDBUFSIZE];
	int dot_fd = -1;

	/* Prepare the command for this entry */

	if (interpolate_command(attr.command, command, CMDBUFSIZE) == -1)
		return;

	debug(("command: %s", command));

	/* Announce the command if verbose */

	if (attr.verbose)
	{
		printf_sanitized("%s", command);
		putchar('\n');
		fflush(stdout);
	}

	/* Change cwd to the file's directory (to avoid path-based race conditions) */

	dot_fd = chdir_local(1);

	/* Execute the command and record any failure */

	if (syscmd(command))
		attr.exit_status = EXIT_FAILURE;

	/* Change cwd back */

	if (dot_fd != -1)
	{
		debug(("fchdir(back)"));

		if (fchdir(dot_fd) == -1)
			fatalsys("fchdir back");

		close(dot_fd);
	}
}

/* 

int syscmd(const char *cmd);

Like system(), but propagate SIGINT/SIGQUIT to rh.

*/

int syscmd(const char *cmd)
{
	int rc = system(cmd);

	if (rc != -1 && WIFSIGNALED(rc) && (WTERMSIG(rc) == SIGINT || WTERMSIG(rc) == SIGQUIT))
		kill(getpid(), WTERMSIG(rc));

	return rc;
}

/*

int remove_danger_from_path(void);

Replace $PATH with a version excluding the current working directory
(i.e. "." or ""), and any non-absolute entries (e.g. "a/b").
This is for better security when the -X option or "cmd".sh is used.
When changing to each matching file's directory, and then executing
a shell command, we don't want those directories in $PATH.

If rh is setuid or setgid for some silly reason, $PATH is set to
"/bin:/usr/bin:/sbin:/usr/sbin".

*/

int remove_danger_from_path(void)
{
	char *oldpath, *newpath, *src, *dst, *d;
	int rc = 0;

	#define DEFAULT_PATH "/bin:/usr/bin:/sbin:/usr/sbin"

	if (geteuid() != getuid() || getegid() != getgid())
	{
		debug_extra(("path not safe, setting default path"));

		return setenv("PATH", DEFAULT_PATH, 1);
	}

	if (!(oldpath = getenv("PATH")))
	{
		debug_extra(("no path, setting default path"));

		return setenv("PATH", DEFAULT_PATH, 1);
	}

	if (!(newpath = malloc(strlen(oldpath) + 1)))
		return -1;

	debug_extra(("path %s", oldpath));

	src = oldpath;
	dst = newpath;

	strlcpy(dst, src, strlen(oldpath) + 1);

	for (;;)
	{
		int modified = 0;
		char *s;

		/* Replace /^:/ with // (empty entry means cwd) */

		if (dst[0] == ':')
		{
			memmove(dst, dst + 1, strlen(dst + 1) + 1);
			debug_extra(("path %s", newpath));

			continue;
		}

		/* Replace /^rel/ with // (includes /^./) */

		if (dst[0] && dst[0] != '/')
		{
			if (!(s = strchr(dst + 1, ':')))
				s = dst + 1 + strlen(dst + 1);

			memmove(dst, s, strlen(s) + 1);
			debug_extra(("path %s", newpath));

			continue;
		}

		/* Replace /:rel/ with // (includes /:./) */
		/* Replace /:$/ with // */
		/* Replace /::/ with /:/ */

		for (d = strchr(dst, ':'); d; d = strchr(d + 1, ':'))
		{
			if (d[1] != '/')
			{
				if (!(s = strchr(d + 1, ':')))
					s = d + 1 + strlen(d + 1);

				memmove(d, s, strlen(s) + 1);
				debug_extra(("path %s", newpath));
				modified = 1;

				break;
			}
		}

		if (modified)
			continue;

		break;
	}

	/* If we no longer have a path, use the default */

	if (!*newpath)
	{
		debug(("old path = %s", oldpath));
		debug(("new path = %s", DEFAULT_PATH));
		free(newpath);

		return setenv("PATH", DEFAULT_PATH, 1);
	}

	/* If we've modified the path, set it */

	if (strcmp(oldpath, newpath))
	{
		debug(("old path = %s", oldpath));
		debug(("new path = %s", newpath));
		rc = setenv("PATH", newpath, 1);
	}

	free(newpath);

	return rc;
}

/*

void visitf_unlink(void);

The -U action for unlinking matching files.

*/

void visitf_unlink(void)
{
	int flags = (isdir(attr.statbuf) && !attr.followed) ? AT_REMOVEDIR : 0;

	if (attr.verbose)
	{
		printf_sanitized("%s", attr.fpath);
		putchar('\n');
	}

	/* If in a sub-directory (has parent_id and basename) */

	if (attr.parent_fd != AT_FDCWD && attr.parent_fd != -1)
	{
		debug(("unlinkat(parent_fd=%d basename=%s flags=0x%x) path=%s", attr.parent_fd, attr.basename, flags, attr.fpath));

		if (unlinkat(attr.parent_fd, attr.basename, flags) == -1)
		{
			errorsys("unlinkat %s", ok(attr.fpath));
			attr.exit_status = EXIT_FAILURE;
		}
	}
	else /* Starting search directory (no parent_id or basename) */
	{
		if (flags)
		{
			debug(("rmdir(cwd %s, path=%s)", getcwd(cwdbuf, CMDBUFSIZE), attr.fpath));

			if (rmdir(attr.fpath) == -1)
			{
				errorsys("rmdir %s", ok(attr.fpath));
				attr.exit_status = EXIT_FAILURE;
			}
		}
		else
		{
			debug(("unlink(cwd %s, path=%s)", getcwd(cwdbuf, CMDBUFSIZE), attr.fpath));

			if (unlink(attr.fpath) == -1)
			{
				errorsys("unlink %s", ok(attr.fpath));
				attr.exit_status = EXIT_FAILURE;
			}
		}
	}
}

/*

static int add_field(char *buf, ssize_t sz, const char *name, const char *value);

Add a JSON string field to buf (of sz bytes).
Encode the value with C-like escape sequences (but without "\a").
Return the length in bytes (excluding nul).

*/

static int add_field(char *buf, ssize_t sz, const char *name, const char *value)
{
	int pos = 0;

	pos += ssnprintf(buf + pos, sz - pos, "\"%s\":\"", name);
	pos += cescape(buf + pos, sz - pos, value, -1, CESCAPE_JSON);
	pos += ssnprintf(buf + pos, sz - pos, "\", ");

	return pos;
}

/*

static char *attributes(void);

Return the ext2-style file attributes as a space-separated
list of attribute names.

*/

static char *attributes(void)
{
	#define ATTR_BUFSIZE 256
	static char buf[ATTR_BUFSIZE];
	unsigned long flags = get_attr();
	int pos = 0;
	char *env;

	buf[pos] = '\0';

	if ((env = getenv("RAWHIDE_TEST_ATTR_FORMAT")) && *env == '1')
		flags = (unsigned long)-1;

	if (!flags)
		return buf;

	#ifdef EXT2_SECRM_FL
	if (flags & EXT2_SECRM_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "secrm");
	#endif

	#ifdef EXT2_UNRM_FL
	if (flags & EXT2_UNRM_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "unrm");
	#endif

	#ifdef EXT2_COMPR_FL
	if (flags & EXT2_COMPR_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "compr");
	#endif

	#ifdef EXT2_SYNC_FL
	if (flags & EXT2_SYNC_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "sync");
	#endif

	#ifdef EXT2_IMMUTABLE_FL
	if (flags & EXT2_IMMUTABLE_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "immutable");
	#endif

	#ifdef EXT2_APPEND_FL
	if (flags & EXT2_APPEND_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "append");
	#endif

	#ifdef EXT2_NODUMP_FL
	if (flags & EXT2_NODUMP_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "nodump");
	#endif

	#ifdef EXT2_NOATIME_FL
	if (flags & EXT2_NOATIME_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "noatime");
	#endif

	#ifdef EXT2_DIRTY_FL
	if (flags & EXT2_DIRTY_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "dirty");
	#endif

	#ifdef EXT2_COMPRBLK_FL
	if (flags & EXT2_COMPRBLK_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "comprblk");
	#endif

	#ifdef EXT2_NOCOMPR_FL
	if (flags & EXT2_NOCOMPR_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "nocompr");
	#endif

	#ifdef EXT4_ENCRYPT_FL
	if (flags & EXT4_ENCRYPT_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "encrypt");
	#endif

	#ifdef EXT2_INDEX_FL
	if (flags & EXT2_INDEX_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "index");
	#endif

	#ifdef EXT2_IMAGIC_FL
	if (flags & EXT2_IMAGIC_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "imagic");
	#endif

	#ifdef EXT3_JOURNAL_DATA_FL
	if (flags & EXT3_JOURNAL_DATA_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "journal_data");
	#endif

	#ifdef EXT2_NOTAIL_FL
	if (flags & EXT2_NOTAIL_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "notail");
	#endif

	#ifdef EXT2_DIRSYNC_FL
	if (flags & EXT2_DIRSYNC_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "dirsync");
	#endif

	#ifdef EXT2_TOPDIR_FL
	if (flags & EXT2_TOPDIR_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "topdir");
	#endif

	#ifdef EXT4_HUGE_FILE_FL
	if (flags & EXT4_HUGE_FILE_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "huge_file");
	#endif

	#ifdef EXT4_EXTENTS_FL
	if (flags & EXT4_EXTENTS_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "extents");
	#endif

	#ifdef EXT4_VERITY_FL
	if (flags & EXT4_VERITY_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "verity");
	#endif

	#ifdef EXT4_EA_INODE_FL
	if (flags & EXT4_EA_INODE_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "ea_inode");
	#endif

	#ifdef FS_NOCOW_FL
	if (flags & FS_NOCOW_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "nocow");
	#endif

	#ifdef EXT4_SNAPFILE_FL
	if (flags & EXT4_SNAPFILE_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "snapfile");
	#endif

	#ifdef FS_DAX_FL
	if (flags & FS_DAX_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "dax");
	#endif

	#ifdef EXT4_SNAPFILE_DELETED_FL
	if (flags & EXT4_SNAPFILE_DELETED_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "snapfile_deleted");
	#endif

	#ifdef EXT4_SNAPFILE_SHRUNK_FL
	if (flags & EXT4_SNAPFILE_SHRUNK_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "snapfile_shrunk");
	#endif

	#ifdef EXT4_INLINE_DATA_FL
	if (flags & EXT4_INLINE_DATA_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "inline_data");
	#endif

	#ifdef EXT4_PROJINHERIT_FL
	if (flags & EXT4_PROJINHERIT_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "projinherit");
	#endif

	#ifdef EXT4_CASEFOLD_FL
	if (flags & EXT4_CASEFOLD_FL)
		pos += ssnprintf(buf + pos, ATTR_BUFSIZE - pos, "%s%s", (pos) ? " " : "", "casefold");
	#endif

	return buf;
}

/*

static char *json(void);

Return all file information in JSON format. The information
included is path, name, target path, starting path, depth,
and all the inode metadata (stat(2) structure fields).

*/

static char *json(void)
{
	#define JSON_BUFSIZE 262144
	static char buf[JSON_BUFSIZE];
	struct passwd *pwd;
	struct group *grp;
	char *base, *selinux, *acl, *ea;
	int pos = 0;

	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "{");

	pos += add_field(buf + pos, JSON_BUFSIZE - pos, "path", attr.fpath);

	base = ((base = strrchr(attr.fpath, '/')) && base[1]) ? base + 1 : attr.fpath;
	pos += add_field(buf + pos, JSON_BUFSIZE - pos, "name", base);

	if (islink(attr.statbuf))
		pos += add_field(buf + pos, JSON_BUFSIZE - pos, "target", read_symlink());

	pos += add_field(buf + pos, JSON_BUFSIZE - pos, "start", attr.search_path);
	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"depth\":%lld, ", (llong)attr.depth);

	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"dev\":%lld, ", (llong)attr.statbuf->st_dev);
	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"major\":%lld, ", (llong)major(attr.statbuf->st_dev));
	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"minor\":%lld, ", (llong)minor(attr.statbuf->st_dev));
	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"ino\":%lld, ", (llong)attr.statbuf->st_ino);

	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"mode\":%lld, ", (llong)attr.statbuf->st_mode);
	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"modestr\":\"%s\", ", modestr(attr.statbuf));
	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"type\":\"%s\", ", ytypecode(attr.statbuf));
	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"perm\":%d, ", (int)attr.statbuf->st_mode & ~S_IFMT);
	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"nlink\":%lld, ", (llong)attr.statbuf->st_nlink);

	if ((pwd = getpwuid(attr.statbuf->st_uid)))
		pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"user\":\"%s\", ", pwd->pw_name);

	if ((grp = getgrgid(attr.statbuf->st_gid)))
		pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"group\":\"%s\", ", grp->gr_name);

	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"uid\":%lld, ", (llong)attr.statbuf->st_uid);
	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"gid\":%lld, ", (llong)attr.statbuf->st_uid);

	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"rdev\":%lld, ", (llong)attr.statbuf->st_rdev);
	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"rmajor\":%lld, ", (llong)major(attr.statbuf->st_rdev));
	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"rminor\":%lld, ", (llong)minor(attr.statbuf->st_rdev));

	set_dirsize();
	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"size\":%lld, ", (llong)attr.statbuf->st_size);
	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"blksize\":%lld, ", (llong)attr.statbuf->st_blksize);
	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"blocks\":%lld, ", (llong)attr.statbuf->st_blocks);

	pos += strftime(buf + pos, JSON_BUFSIZE - pos, "\"atime\":\"%Y-%m-%d %H:%M:%S %z\", ", localtime(&attr.statbuf->st_atime));
	pos += strftime(buf + pos, JSON_BUFSIZE - pos, "\"mtime\":\"%Y-%m-%d %H:%M:%S %z\", ", localtime(&attr.statbuf->st_mtime));
	pos += strftime(buf + pos, JSON_BUFSIZE - pos, "\"ctime\":\"%Y-%m-%d %H:%M:%S %z\", ", localtime(&attr.statbuf->st_ctime));

	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"atime_unix\":%lld, ", (llong)attr.statbuf->st_atime);
	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"mtime_unix\":%lld, ", (llong)attr.statbuf->st_mtime);
	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"ctime_unix\":%lld, ", (llong)attr.statbuf->st_ctime);

	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"attributes\":\"%s\", ", attributes());

	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"project\":%lu, ", get_proj());

	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"generation\":%lu, ", get_gen());

	if ((acl = get_acl(1)))
	{
		pos += add_field(buf + pos, JSON_BUFSIZE - pos, "access_control_list", acl);

		if (attr.facl_verbose)
			pos += add_field(buf + pos, JSON_BUFSIZE - pos, "access_control_list_verbose", acl);
	}

	if ((ea = get_ea(1)) && attr.fea_ok)
		pos += add_field(buf + pos, JSON_BUFSIZE - pos, "extended_attributes", ea);

	if ((selinux = (get_ea(1) && attr.fea_ok && attr.fea_selinux) ? attr.fea_selinux : "") && *selinux)
		pos += add_field(buf + pos, JSON_BUFSIZE - pos, "selinux_context", selinux);

	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "\"acl_ea_indicator\":\"%s\"", aclea());

	pos += ssnprintf(buf + pos, JSON_BUFSIZE - pos, "}");

	return buf;
}

/*

void visitf_format(void);

The -L action for outputting match information in a user-supplied format.

*/

void visitf_format(void)
{
	char *f;

	for (f = attr.format; *f; ++f)
	{
		switch (*f)
		{
			case '\\': /* Backslash escapes */
			{
				switch (*++f)
				{
					case 'a': putchar('\a'); break;
					case 'b': putchar('\b'); break;
					case 'c': fflush(stdout); return;
					case 'f': putchar('\f'); break;
					case 'n': putchar('\n'); break;
					case 'r': putchar('\r'); break;
					case 't': putchar('\t'); break;
					case 'v': putchar('\v'); break;
					case '\\': putchar('\\'); break;
					case '0':
					case '1':
					case '2':
					case '3':
					{
						char *start;
						int ch;

						for (start = f, ch = 0; f - start < 3 && *f >= '0' && *f <= '7'; ++f)
							ch <<= 3, ch |= *f - '0';

						putchar(ch);
						--f;

						break;
					}

					default: putchar('\\'); putchar(*f); break;
				}

				break;
			}

			case '%': /* Conversion */
			{
				/* Construct an output format (flags, width, precision) and strftime format */

				#define OFMTSIZE 33
				#define BUFSIZE 128
				char ofmt[OFMTSIZE], *o = ofmt;
				char buf[BUFSIZE];
				char tfmt[3];

				*o++ = '%';
				tfmt[0] = '%';
				tfmt[2] = '\0';

				#define ofmt_space() (o - ofmt < OFMTSIZE - 1)
				#define ofmt_add(c) if (ofmt_space()) { *o++ = (c); *o = '\0'; } else fatal("invalid -L argument: %s (conversion flags/width/precision too long)", ok(attr.format))
				#define ofmt_add_lld() ofmt_add('l'); ofmt_add('l'); ofmt_add('d')

				while (ofmt_space() && f[1] && strchr(" -+#0", f[1]))
					*o++ = *++f;

				while (ofmt_space() && isdigit((int)(unsigned char)f[1]))
					*o++ = *++f;

				if (ofmt_space() && f[1] == '.')
				{
					*o++ = *++f;

					while (ofmt_space() && isdigit((int)(unsigned char)f[1]))
						*o++ = *++f;
				}

				/* Process conversion: finish ofmt then use it */

				switch (*++f)
				{
					case '%': /* Literal per cent sign */
					{
						debug_extra(("fmt %%%%"));
						putchar('%');

						break;
					}

					case 'a': /* Accessed time in ctime format */
					{
						ofmt_add('s');

						if (strftime(buf, BUFSIZE, "%a %b %e %H:%M:%S %Y", localtime(&attr.statbuf->st_atime)) == 0)
							*buf = '\0';

						debug_extra(("fmt %%a \"%s\", \"%s\"", ofmt, buf));
						printf(ofmt, buf);

						break;
					}

					case 'A': /* Accessed time in strftime format */
					{
						if (*++f == '@')
						{
							ofmt_add_lld();
							debug_extra(("fmt %%A@ \"%s\", %lld", ofmt, (llong)attr.statbuf->st_atime));
							printf(ofmt, (llong)attr.statbuf->st_atime);
						}
						else if (!isalpha((int)(unsigned char)*f) && *f != '+' && *f != '%')
						{
							fatal("invalid %%A conversion: %s", ok(attr.format));
						}
						else
						{
							ofmt_add('s');
							tfmt[1] = *f;

							if (strftime(buf, BUFSIZE, tfmt, localtime(&attr.statbuf->st_atime)) == 0)
								*buf = '\0';

							debug_extra(("fmt %%A%c \"%s\", \"%s\"", tfmt[1], ofmt, buf));
							printf(ofmt, buf);
						}

						break;
					}

					case 'b': /* Blocks */
					{
						ofmt_add_lld();
						debug_extra(("fmt %%b \"%s\", %lld", ofmt, (llong)attr.statbuf->st_blocks));
						printf(ofmt, (llong)attr.statbuf->st_blocks);

						break;
					}

					case 'B': /* Block size */
					{
						ofmt_add_lld();
						debug_extra(("fmt %%B \"%s\", %lld", ofmt, (llong)attr.statbuf->st_blksize));
						printf(ofmt, (llong)attr.statbuf->st_blksize);

						break;
					}

					case 'c': /* Inode changed time in ctime format */
					{
						ofmt_add('s');

						if (strftime(buf, BUFSIZE, "%a %b %e %H:%M:%S %Y", localtime(&attr.statbuf->st_ctime)) == 0)
							*buf = '\0';

						debug_extra(("fmt %%c \"%s\", \"%s\"", ofmt, buf));
						printf(ofmt, buf);

						break;
					}

					case 'C': /* Inode changed time in strftime format */
					{
						if (*++f == '@')
						{
							ofmt_add_lld();
							debug_extra(("fmt %%C@ \"%s\", %lld", ofmt, (llong)attr.statbuf->st_ctime));
							printf(ofmt, (llong)attr.statbuf->st_ctime);
						}
						else if (!isalpha((int)(unsigned char)*f) && *f != '+' && *f != '%')
						{
							fatal("invalid %%C conversion: %s", ok(attr.format));
						}
						else
						{
							ofmt_add('s');
							tfmt[1] = *f;

							if (strftime(buf, BUFSIZE, tfmt, localtime(&attr.statbuf->st_ctime)) == 0)
								*buf = '\0';

							debug_extra(("fmt %%C%c \"%s\", \"%s\"", tfmt[1], ofmt, buf));
							printf(ofmt, buf);
						}

						break;
					}

					case 'd': /* Depth relative to the starting search directory */
					{
						ofmt_add('d');
						debug_extra(("fmt %%d \"%s\", %d", ofmt, attr.depth));
						printf(ofmt, attr.depth);

						break;
					}

					case 'D': /* Device number of the filesystem */
					{
						ofmt_add_lld();
						debug_extra(("fmt %%D \"%s\", %lld", ofmt, (llong)attr.statbuf->st_dev));
						printf(ofmt, (llong)attr.statbuf->st_dev);

						break;
					}

					case 'E': /* Device number */
					{
						ofmt_add_lld();
						debug_extra(("fmt %%E \"%s\", %lld", ofmt, (llong)attr.statbuf->st_rdev));
						printf(ofmt, (llong)attr.statbuf->st_rdev);

						break;
					}

					case 'f': /* Base name or "/" for / */
					{
						char *base;

						ofmt_add('s');
						base = ((base = strrchr(attr.fpath, '/')) && base[1]) ? base + 1 : attr.fpath;
						debug_extra(("fmt %%f \"%s\", \"%s\"", ofmt, base));
						printf_sanitized(ofmt, base);

						break;
					}

					case 'g': /* Group name or ID */
					{
						struct group *grp;

						ofmt_add('s');

						if ((grp = getgrgid(attr.statbuf->st_gid)))
						{
							debug_extra(("fmt %%g \"%s\", \"%s\"", ofmt, grp->gr_name));
							printf(ofmt, grp->gr_name);
						}
						else /* Any flags are %s based, not %d based */
						{
							snprintf(buf, BUFSIZE, "%d", (int)attr.statbuf->st_gid);
							debug_extra(("fmt %%g \"%s\", \"%s\"", ofmt, buf));
							printf(ofmt, buf);
						}

						break;
					}

					case 'G': /* Group ID */
					{
						ofmt_add('d');
						debug_extra(("fmt %%G \"%s\", %d", ofmt, (int)attr.statbuf->st_gid));
						printf(ofmt, (int)attr.statbuf->st_gid);

						break;
					}

					case 'h': /* Directory, or "" for root and children, or "." for local */
					{
						char *s, *e;
						int local;

						ofmt_add('s');

						/* Find the last non-slash character (or "/") */

						e = attr.fpath + strlen(attr.fpath) - 1;

						while (e > attr.fpath && *e == '/')
							--e;

						/* Find the last slash character before that (or "/") */

						while (e > attr.fpath && *e != '/')
							--e;

						/* Local if no '/' in [path, e), unless e == path == "/" */

						local = !(s = strchr(attr.fpath, '/')) || (s >= e && *e != '/'); /* Could also end: || (s > e) */

						/* Output directory, or "." if local, or "" for root dir and its children */

						if (!attr.formatbuf && !(attr.formatbuf = malloc(attr.fpath_size)))
							fatalsys("out of memory");

						snprintf(attr.formatbuf, attr.fpath_size, "%.*s", (local) ? 1 : (int)(e - attr.fpath), (local) ? "." : attr.fpath);
						debug_extra(("fmt %%h \"%s\", \"%s\"", ofmt, attr.formatbuf));
						printf_sanitized(ofmt, attr.formatbuf);

						break;
					}

					case 'H': /* Starting search directory */
					{
						ofmt_add('s');
						debug_extra(("fmt %%H \"%s\", \"%s\"", ofmt, attr.search_path));
						printf_sanitized(ofmt, attr.search_path);

						break;
					}

					case 'i': /* Inode number */
					{
						ofmt_add_lld();
						debug_extra(("fmt %%i \"%s\", %lld", ofmt, (llong)attr.statbuf->st_ino));
						printf(ofmt, (llong)attr.statbuf->st_ino);

						break;
					}

					case 'j': /* JSON */
					{
						char *j = json();

						ofmt_add('s');
						debug_extra(("fmt %%j \"%s\", %s", ofmt, j));
						printf(ofmt, j);

						break;
					}

					case 'k': /* Number of 1KiB blocks */
					{
						llong k = (attr.statbuf->st_blocks + attr.statbuf->st_blocks % 2) >> 1;

						ofmt_add_lld();
						debug_extra(("fmt %%k \"%s\", %lld", ofmt, k));
						printf(ofmt, k);

						break;
					}

					case 'l': /* Symlink target */
					{
						ofmt_add('s');
						debug_extra(("fmt %%l \"%s\", \"%s\"", ofmt, (islink(attr.statbuf)) ? read_symlink() : ""));
						printf_sanitized(ofmt, (islink(attr.statbuf)) ? read_symlink() : "");

						break;
					}

					case 'm': /* Permissions in octal */
					{
						ofmt_add('o');
						debug_extra(("fmt %%m \"%s\", %#o", ofmt, (int)attr.statbuf->st_mode & ~S_IFMT));
						printf(ofmt, (int)attr.statbuf->st_mode & ~S_IFMT);

						break;
					}

					case 'M': /* Permissions in symbolic form */
					{
						ofmt_add('s');
						debug_extra(("fmt %%M \"%s\", \"%s\"", ofmt, modestr(attr.statbuf)));
						printf(ofmt, modestr(attr.statbuf));

						break;
					}

					case 'n': /* Number of hard links */
					{
						ofmt_add_lld();
						debug_extra(("fmt %%n \"%s\", %lld", ofmt, (llong)attr.statbuf->st_nlink));
						printf(ofmt, (llong)attr.statbuf->st_nlink);

						break;
					}

					case 'p': /* Path including the starting search directory */
					{
						ofmt_add('s');
						debug_extra(("fmt %%p \"%s\", \"%s\"", ofmt, attr.fpath));
						printf_sanitized(ofmt, attr.fpath);

						break;
					}

					case 'P': /* Path excluding the starting search directory */
					{
						char *s;

						ofmt_add('s');
						s = attr.fpath + attr.search_path_len;
						s += (*s == '/') ? 1 : 0;
						debug_extra(("fmt %%P \"%s\", \"%s\"", ofmt, s));
						printf_sanitized(ofmt, s);

						break;
					}

					case 'r': /* Minor device number */
					{
						ofmt_add_lld();
						debug_extra(("fmt %%r \"%s\", %lld", ofmt, (llong)minor(attr.statbuf->st_rdev)));
						printf(ofmt, (llong)minor(attr.statbuf->st_rdev));

						break;
					}

					case 'R': /* Major device number */
					{
						ofmt_add_lld();
						debug_extra(("fmt %%R \"%s\", %lld", ofmt, (llong)major(attr.statbuf->st_rdev)));
						printf(ofmt, (llong)major(attr.statbuf->st_rdev));

						break;
					}

					case 's': /* Size */
					{
						ofmt_add_lld();
						set_dirsize(); /* Make st_size for directories more useful */
						debug_extra(("fmt %%s \"%s\", %lld", ofmt, (llong)attr.statbuf->st_size));
						printf(ofmt, (llong)attr.statbuf->st_size);

						break;
					}

					case 'S': /* Sparseness: size ? blocks * 512 / size : 1 */
					{
						ofmt_add('g');
						set_dirsize(); /* Make st_size for directories more useful */
						debug_extra(("fmt %%S \"%s\", %g", ofmt, (attr.statbuf->st_size) ? (double)attr.statbuf->st_blocks * 512 / attr.statbuf->st_size : 1.0));
						printf(ofmt, (attr.statbuf->st_size) ? (double)attr.statbuf->st_blocks * 512 / attr.statbuf->st_size : 1.0);

						break;
					}

					case 't': /* Modification time in ctime format */
					{
						ofmt_add('s');

						if (strftime(buf, BUFSIZE, "%a %b %e %H:%M:%S %Y", localtime(&attr.statbuf->st_mtime)) == 0)
							*buf = '\0';

						debug_extra(("fmt %t \"%s\", \"%s\"", ofmt, buf));
						printf(ofmt, buf);

						break;
					}

					case 'T': /* Modification time in strftime format */
					{
						if (*++f == '@')
						{
							ofmt_add_lld();
							debug_extra(("fmt %%T@ \"%s\", %lld", ofmt, (llong)attr.statbuf->st_mtime));
							printf(ofmt, (llong)attr.statbuf->st_mtime);
						}
						else if (!isalpha((int)(unsigned char)*f) && *f != '+' && *f != '%')
						{
							fatal("invalid %%T conversion: %s", ok(attr.format));
						}
						else
						{
							ofmt_add('s');
							tfmt[1] = *f;

							if (strftime(buf, BUFSIZE, tfmt, localtime(&attr.statbuf->st_mtime)) == 0)
								*buf = '\0';

							debug_extra(("fmt %%T%c \"%s\", \"%s\"", tfmt[1], ofmt, buf));
							printf(ofmt, buf);
						}

						break;
					}

					case 'u': /* User name or ID */
					{
						struct passwd *pwd;

						ofmt_add('s');

						if ((pwd = getpwuid(attr.statbuf->st_uid)))
						{
							debug_extra(("fmt %%u \"%s\", \"%s\"", ofmt, pwd->pw_name));
							printf(ofmt, pwd->pw_name);
						}
						else /* Any flags are %s based, not %d based */
						{
							snprintf(buf, BUFSIZE, "%d", (int)attr.statbuf->st_uid);
							debug_extra(("fmt %%u \"%s\", \"%s\"", ofmt, buf));
							printf(ofmt, buf);
						}

						break;
					}

					case 'U': /* User ID */
					{
						ofmt_add('d');
						debug_extra(("fmt %%U \"%s\", %d", ofmt, (int)attr.statbuf->st_uid));
						printf(ofmt, (int)attr.statbuf->st_uid);

						break;
					}

					case 'v': /* Minor device number of the device the file resides on */
					{
						ofmt_add_lld();
						debug_extra(("fmt %%v \"%s\", %lld", ofmt, (llong)minor(attr.statbuf->st_dev)));
						printf(ofmt, (llong)minor(attr.statbuf->st_dev));

						break;
					}

					case 'V': /* Major device number of the device the file resides on */
					{
						ofmt_add_lld();
						debug_extra(("fmt %%V \"%s\", %lld", ofmt, (llong)major(attr.statbuf->st_dev)));
						printf(ofmt, (llong)major(attr.statbuf->st_dev));

						break;
					}

					case 'x': /* Extended attributes */ 
					{
						char *ea, *s;

						if ((ea = get_ea(1)) && attr.fea_ok)
						{
							/* We have finished searching and can modify the buffer. */
							/* Replace newlines with commas and remove the last one */

							for (s = ea; *s; ++s)
								if (*s == '\n')
									*s = ',';

							if (s > attr.fea && !*s && s[-1] == ',')
								s[-1] = '\0';

							ofmt_add('s');
							debug_extra(("fmt %%x \"%s\", \"%s\"", ofmt, attr.fea));
							printf(ofmt, attr.fea); /* Sanitized earlier by cescape() */
						}
						else
						{
							ofmt_add('s');
							debug_extra(("fmt %%x \"%s\", \"%s\"", ofmt, ""));
							printf(ofmt, "");
						}

						break;
					}

					case 'X': /* ACL/EA indicator */ 
					{
						ofmt_add('s');
						debug_extra(("fmt %%X \"%s\", \"%s\"", ofmt, aclea()));
						printf(ofmt, aclea());

						break;
					}

					case 'y': /* File type */
					{
						ofmt_add('s');
						debug_extra(("fmt %%y \"%s\", \"%s\"", ofmt, ytypecode(attr.statbuf)));
						printf(ofmt, ytypecode(attr.statbuf));

						break;
					}

					case 'Y': /* File type showing symlink targets */
					{
						const char *ttype;

						if (islink(attr.statbuf))
						{
							prepare_target();

							if (attr.linkstat_ok)
								ttype = ytypecode(attr.linkstatbuf);
							else if (attr.linkstat_errno == ENOENT || attr.linkstat_errno == ENOTDIR)
								ttype = "N";
							else if (attr.linkstat_errno == ELOOP)
								ttype = "L";
							else
								ttype = "?";
						}
						else
						{
							ttype = ytypecode(attr.statbuf);
						}

						ofmt_add('s');
						debug_extra(("fmt %%Y \"%s\", \"%s\"", ofmt, ttype));
						printf(ofmt, ttype);

						break;
					}

					case 'z': /* Access control list (comma-separated) */
					{
						char *z, *s;

						if ((z = get_acl(1)))
						{
							/* On FreeBSD/Solaris, use non-compact form if verbose */

							if (attr.verbose && attr.facl_verbose)
								z = attr.facl_verbose;

							if (!attr.formatbuf && !(attr.formatbuf = malloc(attr.fpath_size)))
								fatalsys("out of memory");

							snprintf(attr.formatbuf, attr.fpath_size, "%s", z);

							/* Replace newlines with commas and remove the last one. */
							/* On FreeBSD/Solaris, there are no newlines just commas already */

							for (s = attr.formatbuf; *s; ++s)
								if (*s == '\n')
									*s = ',';

							if (s > attr.formatbuf && !*s && s[-1] == ',')
								s[-1] = '\0';

							ofmt_add('s');
							debug_extra(("fmt %%z \"%s\", \"%s\"", ofmt, attr.formatbuf));
							printf(ofmt, attr.formatbuf);
						}
						else
						{
							ofmt_add('s');
							debug_extra(("fmt %%z \"%s\", \"%s\"", ofmt, ""));
							printf(ofmt, "");
						}

						break;
					}

					case 'Z': /* SELinux security context/label */
					{
						char *selinux = (get_ea(1) && attr.fea_ok && attr.fea_selinux) ? attr.fea_selinux : "";

						ofmt_add('s');
						debug_extra(("fmt %%Z \"%s\", \"%s\"", ofmt, selinux));
						printf(ofmt, selinux);

						break;
					}

					case 'e': /* Ext2-style file attributes */
					{
						char *a = attributes();

						ofmt_add('s');
						debug_extra(("fmt %%e \"%s\", \"%s\"", ofmt, a));
						printf(ofmt, a);

						break;
					}

					case 'J': /* Ext2-style project */
					{
						unsigned long proj = get_proj();

						ofmt_add_lld();
						debug_extra(("fmt %%J \"%s\", %lld", ofmt, (llong)proj));
						printf(ofmt, (llong)proj);

						break;
					}

					case 'I': /* Ext2-style generation */
					{
						unsigned long gen = get_gen();

						ofmt_add_lld();
						debug_extra(("fmt %%I \"%s\", %lld", ofmt, (llong)gen));
						printf(ofmt, (llong)gen);

						break;
					}

					case '\0': /* Invalid % at the end */
					{
						fatal("invalid -L argument: %s (%% at the end)", ok(attr.format));
					}

					default: /* Invalid % */
					{
						fatal("invalid -L argument: %s (invalid conversion: %%%s)", ok(attr.format), oklen(f, 1));
					}
				}

				break;
			}

			default: /* Literal text */
			{
				putchar(*f);

				break;
			}
		}
	}
}

/* vi:set ts=4 sw=4: */
