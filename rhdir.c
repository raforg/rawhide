
/* ----------------------------------------------------------------------
 * FILE: rhdir.c
 * VERSION: 2
 * Written by: Ken Stauffer
 * This file contains the "non portable" stuff dealing with
 * directories.
 * printentry(), ftrw(), fwt1()
 *
 *
 * ---------------------------------------------------------------------- */

#include "rh.h"

#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#define user_index(b)	((000777 & (b)) >> 6) + ((b) & S_ISUID ? 8 : 0) 
#define group_index(b)	((000077 & b) >> 3) + ((b) & S_ISGID ? 8 : 0)
#define all_index(b)	((000007 & (b)) + (((b) & S_ISVTX) ? 8 : 0))
#define ftype_index(b)	((b) >> 13)

#define isdirect(b)	(((b) & S_IFMT) == S_IFDIR)
#define isblk(b)	(((b) & S_IFMT) == S_IFBLK)
#define ischr(b)	(((b) & S_IFMT) == S_IFCHR)

#ifndef major
#define major(b)	((b) >> 8)
#endif
#ifndef minor
#define minor(b)	((b) & 0xff)
#endif

#define isdot(s)	((s)[1] == '\0' && (s)[0] == '.')
#define isdotdot(s)	((s)[2] == '\0' && (s)[1] == '.' && (s)[0] == '.')

#ifdef S_IFLNK
#define islink(b)	(((b) & S_IFMT) == S_IFLNK)
#else
#define islink(b)	(0)
#define lstat	stat
#endif

#define isproper(m)	(isdirect(m) && !islink(m) && !attr.prune)

/* ----------------------------------------------------------------------
 * printentry:
 *	Display filename,permissions and size in a '/bin/ls' like
 *	format. If verbose is non-zero then more information is
 *	displayed.
 * uses the macros:
 *	user_index(b)
 *	group_index(b)
 *	all_index(b)
 *	ftype_index(b)
 *
 */

void printentry(int verbose, struct stat *buf, char *name)
{
	char *t;

	static char *ftype[] =
	{
		"p", "c" , 
		"d" , "b" ,  
		"-" , "l" , 
		"s" , "t"
	};
 
	static char *perm[] =
	{
		"---", "--x", "-w-", "-wx" ,
		"r--", "r-x", "rw-", "rwx" ,
		"--S", "--s", "-wS", "-ws" ,
		"r-S", "r-s", "rwS", "rws"
	};

	static char *perm2[] =
	{
		"---", "--x", "-w-", "-wx" ,
		"r--", "r-x", "rw-", "rwx" ,
		"--T", "--t", "-wT", "-wt" ,
		"r-T", "r-t", "rwT", "rwt"
	};
 
	if (verbose)
	{
		t = ctime(&buf->st_mtime);
		t[24] = '\0';

		if (ischr(buf->st_mode) || isblk(buf->st_mode))
		{
			printf("%s%s%s%s %4d %4d %3lu,%3lu %s %-s\n",
				ftype[ftype_index(buf->st_mode)],
				perm[user_index(buf->st_mode)],
				perm[group_index(buf->st_mode)],
				perm2[all_index(buf->st_mode)],
				buf->st_uid,
				buf->st_gid,
				(long unsigned int)major(buf->st_rdev),
				(long unsigned int)minor(buf->st_rdev),
				t + 4,
				name
			);
		}
		else
		{
			printf("%s%s%s%s %4d %4d %6lu %s %-s\n",
				ftype[ftype_index(buf->st_mode)],
				perm[user_index(buf->st_mode)],
				perm[group_index(buf->st_mode)],
				perm2[all_index(buf->st_mode)],
				buf->st_uid,
				buf->st_gid,
				(long unsigned int)buf->st_size,
				t + 4,
				name
			);
		}
	}
	else
	{
		if (ischr(buf->st_mode) || isblk(buf->st_mode))
		{
			printf("%s%s%s%s %3lu,%3lu %-s\n",
				ftype[ftype_index(buf->st_mode)],
				perm[user_index(buf->st_mode)],
				perm[group_index(buf->st_mode)],
				perm2[all_index(buf->st_mode)],
				(long unsigned int)major(buf->st_rdev),
				(long unsigned int)minor(buf->st_rdev),
				name
			);
		}
		else
		{
			printf("%s%s%s%s %9lu %-s\n",
				ftype[ftype_index(buf->st_mode)],
				perm[user_index(buf->st_mode)],
				perm[group_index(buf->st_mode)],
				perm2[all_index(buf->st_mode)],
				(long unsigned int)buf->st_size,
				name
			);
		}
	}
}

/* ----------------------------------------------------------------------
 * fwt1:
 *	'p' points to the end of the string in attr.fname
 *
 */

static void fwt1(int depth, char *p)
{
	char *q, *s;
	DIR *dirp;
	struct dirent *dp;

	if (!depth)
		return;

	attr.depth += 1;

	dirp = opendir(attr.fname);
	if (dirp == NULL)
		return;

	for (dp = readdir(dirp); dp; dp = readdir(dirp))
	{
		if (isdot(dp->d_name) || isdotdot(dp->d_name))
			continue;

		s = p;
		q = dp->d_name;

		while ((*s++ = *q++)) /* XXX limit this */
			;

		s -= 1;

		if (lstat(attr.fname, attr.buf) < 0)
			continue;

		(*(attr.func))();

		if (isproper(attr.buf->st_mode))
		{
			*s++ = '/';
			*s = '\0';
			fwt1(depth - 1, s);
		}
	}

	closedir(dirp);
	attr.depth -= 1;
	attr.prune = 0;
	*p = '\0';
}

/* ----------------------------------------------------------------------
 * ftrw:
 *	Entry point to do the search, ftrw is a front end
 *	to the recursive fwt1.
 *	ftrw() initializes some global variables and
 *	builds the initial filename string which is passed to
 *	ftw1().
 */

int ftrw(char *f, void (*fn)(void), int depth)
{
	char *p, *filebuf;
	struct stat statbuf;
	int last;
	long max_pathlen;

	max_pathlen = pathconf(f, _PC_PATH_MAX);
	if (max_pathlen == -1)
		max_pathlen = 1024;
	else
		max_pathlen += 2;

	filebuf = (char *)malloc(max_pathlen);
	attr.prune = 0;
	attr.depth = 0;
	attr.func = fn;
	attr.fname = filebuf;
	attr.buf = &statbuf;

	if (strlen(f) > max_pathlen)
	{
		fprintf(stderr, "path is too long: %s\n", f);
		exit(1);
	}

	strncpy(attr.fname, f, max_pathlen + 1);
	last = 0;

	for (p = attr.fname; *p; p++)
		last = (*p == '/') ? 1 : 0;

	if (!last)
	{
		*p++ = '/';
		*p = '\0';
	}

	if (lstat(attr.fname, attr.buf) < 0)
	{
		free(filebuf);
		return -1;
	}

	(*(attr.func))();

	if (isproper(attr.buf->st_mode))
		fwt1(depth, p);

	free(filebuf);

	return 0;
}

/* vi:set ts=4 sw=4: */
