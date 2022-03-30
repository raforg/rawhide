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

#define _FILE_OFFSET_BITS 64 /* For 64-bit off_t on 32-bit systems (Not AIX) */

#include <stdio.h>
#include <string.h>
#include <time.h>
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

#include "rh.h"
#include "rhjson.h"
#include "rhdir.h"
#include "rhcmds.h"
#include "rhstr.h"

#define islink(statbuf) (((statbuf)->st_mode & S_IFMT) == S_IFLNK)

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

char *json(void);

Return all file information in JSON format. The information
included is path, name, target path, starting path, depth,
and all the inode metadata (stat(2) structure fields).

*/

char *json(void)
{
	#define BUFSIZE 262144
	static char buf[BUFSIZE];
	struct passwd *pwd;
	struct group *grp;
	char *base, *selinux, *acl, *ea;
	int pos = 0;

	pos += ssnprintf(buf + pos, BUFSIZE - pos, "{");

	pos += add_field(buf + pos, BUFSIZE - pos, "path", attr.fpath);

	base = ((base = strrchr(attr.fpath, '/')) && base[1]) ? base + 1 : attr.fpath;
	pos += add_field(buf + pos, BUFSIZE - pos, "name", base);

	if (islink(attr.statbuf))
		pos += add_field(buf + pos, BUFSIZE - pos, "target", read_symlink());

	pos += add_field(buf + pos, BUFSIZE - pos, "start", attr.search_path);
	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"depth\":%lld, ", (llong)attr.depth);

	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"dev\":%lld, ", (llong)attr.statbuf->st_dev);
	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"major\":%lld, ", (llong)major(attr.statbuf->st_dev));
	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"minor\":%lld, ", (llong)minor(attr.statbuf->st_dev));
	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"ino\":%lld, ", (llong)attr.statbuf->st_ino);

	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"mode\":%lld, ", (llong)attr.statbuf->st_mode);
	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"modestr\":\"%s\", ", modestr(attr.statbuf));
	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"type\":\"%s\", ", ytypecode(attr.statbuf));
	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"perm\":%d, ", (int)attr.statbuf->st_mode & ~S_IFMT);
	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"nlink\":%lld, ", (llong)attr.statbuf->st_nlink);

	if ((pwd = getpwuid(attr.statbuf->st_uid)))
		pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"user\":\"%s\", ", pwd->pw_name);

	if ((grp = getgrgid(attr.statbuf->st_gid)))
		pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"group\":\"%s\", ", grp->gr_name);

	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"uid\":%lld, ", (llong)attr.statbuf->st_uid);
	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"gid\":%lld, ", (llong)attr.statbuf->st_uid);

	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"rdev\":%lld, ", (llong)attr.statbuf->st_rdev);
	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"rmajor\":%lld, ", (llong)major(attr.statbuf->st_rdev));
	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"rminor\":%lld, ", (llong)minor(attr.statbuf->st_rdev));

	set_dirsize();
	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"size\":%lld, ", (llong)attr.statbuf->st_size);
	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"blksize\":%lld, ", (llong)attr.statbuf->st_blksize);
	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"blocks\":%lld, ", (llong)attr.statbuf->st_blocks);

	pos += strftime(buf + pos, BUFSIZE - pos, "\"atime\":\"%Y-%m-%d %H:%M:%S %z\", ", localtime(&attr.statbuf->st_atime));
	pos += strftime(buf + pos, BUFSIZE - pos, "\"mtime\":\"%Y-%m-%d %H:%M:%S %z\", ", localtime(&attr.statbuf->st_mtime));
	pos += strftime(buf + pos, BUFSIZE - pos, "\"ctime\":\"%Y-%m-%d %H:%M:%S %z\", ", localtime(&attr.statbuf->st_ctime));

	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"atime_unix\":%lld, ", (llong)attr.statbuf->st_atime);
	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"mtime_unix\":%lld, ", (llong)attr.statbuf->st_mtime);
	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"ctime_unix\":%lld, ", (llong)attr.statbuf->st_ctime);

	if ((acl = get_acl(1)))
	{
		pos += add_field(buf + pos, BUFSIZE - pos, "access_control_list", acl);

		if (attr.facl_verbose)
			pos += add_field(buf + pos, BUFSIZE - pos, "access_control_list_verbose", acl);
	}

	if ((ea = get_ea(1)) && attr.fea_ok)
		pos += add_field(buf + pos, BUFSIZE - pos, "extended_attributes", ea);

	if ((selinux = (get_ea(1) && attr.fea_ok && attr.fea_selinux) ? attr.fea_selinux : "") && *selinux)
		pos += add_field(buf + pos, BUFSIZE - pos, "selinux_context", selinux);

	pos += ssnprintf(buf + pos, BUFSIZE - pos, "\"acl_ea_indicator\":\"%s\"", aclea());

	pos += ssnprintf(buf + pos, BUFSIZE - pos, "}");

	return buf;
}

/* vi:set ts=4 sw=4: */
