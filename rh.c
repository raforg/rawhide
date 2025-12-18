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
#define _FILE_OFFSET_BITS 64 /* For 64-bit off_t on 32-bit systems */
#define _TIME_BITS 64        /* For 64-bit time_t on 32-bit systems */

#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <fnmatch.h>
#include <glob.h>
#include <errno.h>
#include <pwd.h>
#include <sys/stat.h>
#include <langinfo.h>

#include "rh.h"
#include "rhparse.h"
#include "rhdata.h"
#include "rhdir.h"
#include "rhstr.h"
#include "rherr.h"
#include "rhfnmatch.h"
#include "rhgetopt.h"

#ifdef HAVE_ACL
#include <sys/acl.h>
#endif

#ifdef NDEBUG
#define debug(args)
#else
#define debug(args) debugf args
#endif

/*

static void help_message(void);

Output the usage message, then exit.

*/

static void help_message(void)
{
	symbol_t *s;
	int i;

	printf("usage: %s [options] [--] [path...]\n", RAWHIDE_PROG_NAME);
	printf("options:\n");
	printf("  -h --help    - Show this help message, then exit\n");
	printf("  -V --version - Show the version message, then exit\n");
	printf("  -N           - Don't read system-wide config (" RAWHIDE_CONF ")\n");
	printf("  -n           - Don't read user-specific config (~" RAWHIDE_RC ")\n");
	printf("  -f-          - Read functions and/or expression from stdin\n");
	printf("  -f fname     - Read functions and/or expression from a file\n");
	printf(" [-e] 'expr'   - Read functions and/or expression from the cmdline\n");
	printf("\n");
	printf("traversal options:\n");
	printf("  -r           - Only search one level down (same as -m1 -M1)\n");
	printf("  -m #         - Override the default minimum depth (0)\n");
	printf("  -M #         - Override the default maximum depth (system limit)\n");
	printf("  -D           - Depth-first searching (contents before directory)\n");
	printf("  -1           - Single filesystem (don't cross filesystem boundaries)\n");
	printf("  -y           - Follow symlinks on the cmdline and in reference files\n");
	printf("  -Y           - Follow symlinks encountered while searching as well\n");
	printf("\n");
	printf("alternative action options:\n");
	printf("  -x 'cmd %%s'  - Execute a shell command for each match (racy)\n");
	printf("  -X 'cmd %%S'  - Like -x but run from each match's directory (safer)\n");
	printf("  -U -U -U     - Unlink matches (but tell me three times), implies -D\n");
	printf("\n");
	printf("output action options:\n");
	printf("  -l           - Output matching entries like ls -l (but unsorted)\n");
	printf("  -d           - Include device column, implies -l\n");
	printf("  -i           - Include inode column, implies -l\n");
	printf("  -B           - Include block size column, implies -l\n");
	printf("  -s           - Include blocks column, implies -l\n");
	printf("  -S           - Include space column, implies -l\n");
	printf("  -g           - Exclude user/owner column, implies -l\n");
	printf("  -o           - Exclude group column, implies -l\n");
	printf("  -a           - Include atime rather than mtime column, implies -l\n");
	printf("  -u           - Same as -a (like ls(1))\n");
	printf("  -c           - Include ctime rather than mtime column, implies -l\n");
	printf("  -b           - Include btime rather than mtime column, implies -l\n");
	printf("  -v           - Verbose: All columns, implies -ldiBsSacb unless -xXU0Lj\n");
	printf("  -0           - Output null chars instead of newlines (for xargs -0)\n");
	printf("  -L format    - Output matching entries in a user-supplied format\n");
	printf("  -j           - Output matching entries as JSON (same as -L \"%%j\\n\")\n");
	printf("\n");
	printf("path format options:\n");
	printf("  -Q           - Enclose paths in double quotes\n");
	printf("  -E           - Output C-style escapes for control characters\n");
	printf("  -q           - Output ? for control characters (default if tty)\n");
	printf("  -p           - Append / indicator to directories\n");
	printf("  -t           - Append most type indicators (one of / @ = | > %%)\n");
	printf("  -F           - Append all type indicators (one of * / @ = | > %%)\n");
	printf("\n");
	printf("                   * executable\n");
	printf("                   / directory\n");
	printf("                   @ symlink\n");
	printf("                   = socket\n");
	printf("                   | fifo\n");
	printf("                   > door (Solaris only)\n");
	printf("                   %% whiteout (macOS only)\n");
	printf("\n");
	printf("other column format options:\n");
	printf("  -H or -HH    - Output sizes like 1.2K 34M 5.6G etc., implies -l\n");
	printf("  -I or -II    - Like -H but with units of 1000, not 1024, implies -l\n");
	printf("  -T           - Output mtime/atime/ctime in ISO format, implies -l\n");
	printf(" '-#'          - Output numeric user/group IDs (not names), implies -l\n");
	printf("\n");
	printf("debug option:\n");
	printf(" '-?' spec     - Output debug messages: spec can include any of:\n");
	printf("                   cmdline, parser, traversal, exec, all, extra\n");

	printf("\nrh (rawhide) finds files using pretty C expressions.\n");
	printf("See the rh(1) and rawhide.conf(5) manual entries for more information.\n");

	printf("\nC operators:\n");
	printf("  - ~ !  * / %%  + -  << >>  < > <= >=  == !=  &  ^  |  &&  ||  ?:  ,\n");

	printf("\nRawhide tokens:\n");
	printf("  \"pattern\"  \"pattern\".modifier  \"/path\".field  \"cmd\".sh  \"cmd\".ush\n");
	printf("  {pattern}  {pattern}.modifier  {/path}.field  {cmd}.sh  {cmd}.ush\n");
	printf("  123 0777 0xffff  1K 2M 3G  1k 2m 3g  $user @group  $$ @@\n");
	printf("  [yyyy/mm/dd] [yyyy/mm/dd hh:mm:ss.nnnnnnnnn]\n");

	printf("\nGlob pattern notation:\n");
	printf("  ? * [abc] [!abc] [a-c] [!a-c]\n");
	#ifdef FNM_EXTMATCH
	if (!attr.internal_fnmatch)
	{
		printf("  ?(a|b|c) *(a|b|c) +(a|b|c) @(a|b|c) !(a|b|c)\n");
		printf("  Some ksh extended glob patterns are available here (see fnmatch(3))\n");
	}
	#endif

	printf("\nPattern modifiers:\n");

	/* One column for each method of pattern matching */
	int columns = 1;
	#ifdef FNM_CASEFOLD
	columns += 1; /* .i */
	#endif
	#ifdef HAVE_PCRE2
	columns += 2; /* .re .rei */
	#endif

	if (columns > 1)
		printf("  %-12s", "");

	for (i = 1, s = symbols; s; s = s->next)
		if (s->type == PATMOD)
			printf("  %-12s%s", s->name, (i++ % columns == columns - 1 || !s->next) ? "\n" : "");

	printf("\n");

	#ifdef FNM_CASEFOLD
	printf("  Case-insensitive glob pattern matching is available here (i)\n");
	#endif
	#ifdef HAVE_PCRE2
	printf("  Perl-compatible regular expressions are available here (re)\n");
	#else
	printf("  Perl-compatible regular expressions are not available here\n");
	#endif
	#ifdef HAVE_MAGIC
	printf("  File types and MIME types are available here (what, mime)\n");
	#endif
	#ifdef HAVE_ACL
	#if defined(HAVE_POSIX_ACL) && defined(ACL_TYPE_DEFAULT)
	printf("  Access control lists are available here (acl, dacl)\n");
	#else
	printf("  Access control lists are available here (acl)\n");
	#endif
	#endif
	#ifdef HAVE_EA
	printf("  Extended attributes are available here (ea)\n");
	#endif

	printf("\nBuilt-in symbols:\n");

	for (i = 0, s = symbols; s; s = s->next)
		if (s->type == FIELD || s->type == NUMBER)
			printf("  %-12s%s", s->name, (i++ % 5 == 4 || !s->next || s->next->type == REFFILE) ? "\n" : "");

	printf("\nReference file fields:\n");

	for (i = 0, s = symbols; s; s = s->next)
		if (s->type == REFFILE)
			printf("  %-12s%s", s->name, (i++ % 5 == 4 || !s->next || s->next->type == PATMOD) ? "\n" : "");

	printf("\nSystem-wide and user-specific functions can be defined here:\n");
	printf("  %s          ~%s\n", RAWHIDE_CONF, RAWHIDE_RC);
	printf("  %s.d/*      ~%s.d/*\n", RAWHIDE_CONF, RAWHIDE_RC);

	printf("\n");
	printf("Name: %s (%s)\n", RAWHIDE_PROG_NAME, RAWHIDE_NAME);
	printf("Version: %s\n", RAWHIDE_VERSION);
	printf("Date: %s\n", RAWHIDE_DATE);
	printf("Authors: Ken Stauffer, raf <raf@raf.org>\n");
	printf("URL: https://raf.org/rawhide\n");
	printf("GIT: https://github.org/raforg/rawhide\n");
	printf("GIT: https://codeberg.org/raforg/rawhide\n");
	printf("\n");
	printf("Copyright (C) 1990 Ken Stauffer, 2022-2023 raf <raf@raf.org>\n");
	printf("\n");
	printf("This is free software released under the terms of the GPLv3+:\n");
	printf("\n");
	printf("    https://www.gnu.org/licenses\n");
	printf("\n");
	printf("There is no warranty; not even for merchantability or fitness\n");
	printf("for a particular purpose.\n");
	printf("\n");
	printf("Report bugs to raf <raf@raf.org>\n");

	exit(EXIT_SUCCESS);
}

/*

static void version_message(void);

Output the version message, then exit.

*/

static void version_message(void)
{
	printf("%s-%s\n", RAWHIDE_PROG_NAME, RAWHIDE_VERSION);

	exit(EXIT_SUCCESS);
}

#ifndef NDEBUG
/*

static void debugf(const char *format, ...);

Output a cmdline debug message to stderr, if requested.

*/

static void debugf(const char *format, ...)
{
	va_list args;

	if (!(attr.debug_flags & DEBUG_CMDLINE))
		return;

	va_start(args, format);
	fprintf(stderr, "%s: ", "cmdline");
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
	va_end(args);
}
#endif

/*

static void load_program_file(char *fname);

Compile the given file. I/O errors are non-fatal.

*/

static void load_program_file(char *fname)
{
	struct stat statbuf[1];

	if (stat(fname, statbuf) == -1 || !isreg(statbuf))
		error("%s is not a file", ok(fname));
	else if (!(expfile = fopen(fname, "r")))
		errorsys("%s", ok(fname));
	else
	{
		expfname = fname;
		parse_program();
		fclose(expfile);
		expfile = NULL;
	}
}

/*

static void load_program_str(char *str);

Compile the given string.

*/

static void load_program_str(char *str)
{
	expstr = str;
	parse_program();
	expstr = NULL;
}

/*

static int env_flag(char *envname);

Return 1 if the environment variable is "1". Return 0 otherwise.

*/

static int env_flag(char *envname)
{
	char *env;

	return (env = getenv(envname)) && !strcmp(env, "1");
}

/*

static void load_program_dir(const char *initdir, char *initpattern, llong pathbufsize);

Compile the files (in lexicographic order) that are in the
directory named in initdir. The initpattern buffer will be
used to construct the glob pattern. The pathbufsize is the
size of the two buffers.

*/

static void load_program_dir(const char *initdir, char *initpattern, llong pathbufsize)
{
	strlcpy(initpattern, initdir, pathbufsize);

	if (strlcat(initpattern, "/*", pathbufsize) >= pathbufsize)
		error("path is too long: %s/*", ok(initdir));
	else
	{
		glob_t glob_state[1];
		int i;

		if (glob(initpattern, 0, NULL, glob_state) == 0)
			for (i = 0; i < glob_state->gl_pathc; i++)
				load_program_file(glob_state->gl_pathv[i]);

		globfree(glob_state);
	}
}

/*

static void load_config_dir(const char *initfile, char *initdir, char *initpattern, llong pathbufsize);

Compile the files (in lexicographic order) that are in the
directory corresponding to the file named in initfile. The
initdir and initpattern buffers will be used to construct
the directory name (by appending ".d") and the glob pattern.
The pathbufsize is the size of the buffers.

*/

static int load_config_dir(const char *initfile, char *initdir, char *initpattern, llong pathbufsize)
{
	struct stat statbuf[1];

	strlcpy(initdir, initfile, pathbufsize);

	if (strlcat(initdir, DOTDDIR, pathbufsize) >= pathbufsize)
		error("path is too long: %s%s", ok(initfile), ok2(DOTDDIR));
	else if (stat(initdir, statbuf) != -1)
	{
		if (!isdir(statbuf))
			error("%s is not a directory", ok(initdir));
		else
		{
			load_program_dir(initdir, initpattern, pathbufsize);

			return 1;
		}
	}

	return 0;
}

/*

static void magic_cleanup(void);

Release any libmagic resources that have been allocated.
To be run at exit.

*/

#ifdef HAVE_MAGIC
static void magic_cleanup(void)
{
	if (attr.what_cookie)
	{
		magic_close(attr.what_cookie);
		attr.what_cookie = NULL;
	}

	if (attr.mime_cookie)
	{
		magic_close(attr.mime_cookie);
		attr.mime_cookie = NULL;
	}

	if (attr.what_follow_cookie)
	{
		magic_close(attr.what_follow_cookie);
		attr.what_follow_cookie = NULL;
	}

	if (attr.mime_follow_cookie)
	{
		magic_close(attr.mime_follow_cookie);
		attr.mime_follow_cookie = NULL;
	}
}
#endif

/*

int main(int argc, char *argv[]);

Parse the command line:

The --help and --version options are supported (if first).
The -e, -x, -X, -L, and -j options can only appear once.
The -l, -0, -L, -j, -x, -X, and -U options are mutually exclusive.

Read /etc/rawhide.conf and /etc/rawhide.conf.d (unless -N).
Read ~/.rhrc and ~/.rhrc.d (unless -n).
Read -f file/dir/stdin (if present).
Read -e expr (if present).

If RAWHIDE_ALLOW_IMPLICIT_EXPR_HEURISTIC=1 (and -f not supplied):
If no explicit -e expression is supplied, look among any
remaining command line arguments for one that is not a
file or directory (and doesn't look like it's supposed
to be a file or directory), and assume that it is the
file condition expression (an implicit -e option).

If, after all that, a condition expression is still not
found, then assume "1" to match all filesystem entries.

Find matching files:

Find matching files in directories specified by the
remaining arguments (or in the current directory).
The -rmMD1yY options alter traversal behaviour.
By default, matching entry names are output.
The -l option and other options include more details.
There are many options to affect the formatting.
The -0 option outputs nul characters rather than newlines.
The -L option outputs information in a user-defined format.
The -j option outputs information as JSON (same as -L "%j\n").
The -x option executes a shell command for each
matching file instead.
The -X option is the same, but executes each command
from the matching entry's parent directory so as to
minimize race conditions.
The -U option unlinks/removes/deletes matching entries instead,
and must be supplied three times in order to take effect.

*/

int main(int argc, char *argv[])
{
	char *opt_e, *initfile, *initdir, *initpattern, *endptr, **opt_f_list;
	int opt_f, opt_h, opt_V, opt_l, opt_r, opt_N, opt_n, opt_U;
	llong opt_m, opt_M, optarg_int, opt_e_expr = 0;
	llong max_pathlen, pathbufsize;
	struct stat statbuf[1];
	int o, i, expr_index = 0, any = 0, stdin_read = 0;
	uid_t uid;
	gid_t gid;

	setlocale(LC_ALL, "");
	prog_name = argv[0];

	/* Revoke any unexpected privileges */

	if (getegid() != (gid = getgid()))
		if (setgid(gid) == -1 || getegid() != getgid() || env_flag("RAWHIDE_TEST_SETGID_FAILURE"))
			fatalsys("failed to revoke unexpected setgid privileges");

	if (geteuid() != (uid = getuid()))
		if (setuid(uid) == -1 || geteuid() != getuid() || env_flag("RAWHIDE_TEST_SETUID_FAILURE"))
			fatalsys("failed to revoke unexpected setuid privileges");

	/* Defaults */

	opt_h = 0;                  /* -h (help) */
	opt_V = 0;                  /* -V (version) */
	opt_N = 0;                  /* -N (suppress /etc/rawhide.conf and .d) */
	opt_n = 0;                  /* -n (suppress ~/.rhrc and .d) */
	opt_f = 0;                  /* Number of -f option arguments */
	opt_f_list = NULL;          /* List of -f option arguments (file/dir/stdin) */
	opt_e = NULL;               /* -e expr (read code from arg) */

	opt_r = 0;                  /* -r (only search 1 level down) */
	opt_m = -1;                 /* -m (min depth) */
	opt_M = -1;                 /* -M (max depth) */
	attr.depth_first = 0;       /* -D (depth-first) */
	attr.single_filesystem = 0; /* -1 (single filesystem) */
	attr.follow_symlinks = 0;   /* -y -Y (follow symlinks: 1=supplied 2=encountered) */

	attr.command = NULL;        /* -x/-X cmd (execute cmd) */
	attr.local = 0;             /* cmd executed locally (-X) */
	opt_U = 0;                  /* -U (unlink - but tell me three times) */
	attr.unlink = 0;            /* -UUU (unlink - I've been told three times) */

	opt_l = 0;                  /* -l */
	attr.dev_column = 0;        /* -d */
	attr.ino_column = 0;        /* -i */
	attr.blksize_column = 0;    /* -B */
	attr.blocks_column = 0;     /* -s */
	attr.space_column = 0;      /* -S */
	attr.no_owner_column = 0;   /* -g */
	attr.no_group_column = 0;   /* -o */
	attr.atime_column = 0;      /* -a/-u */
	attr.ctime_column = 0;      /* -c */
	attr.btime_column = 0;      /* -b */
	attr.verbose = 0;           /* -v */
	attr.nul = 0;               /* -0 */
	attr.format = NULL;         /* -L/-j */

	attr.quote_name = 0;        /* -Q */
	attr.escape_name = 0;       /* -E */
	attr.mask_name = 0;         /* -q */
	attr.dir_indicator = 0;     /* -p */
	attr.most_indicators = 0;   /* -t */
	attr.all_indicators = 0;    /* -F */

	attr.human_units = 0;       /* -H (1=roundup 2=roundhalfup) */
	attr.si_units = 0;          /* -I (1=roundup 2=roundhalfup) */
	attr.iso_time = 0;          /* -T */
	attr.numeric_ids = 0;       /* -# */

	attr.visitf = visitf_default;
	attr.exit_status = EXIT_SUCCESS;
	attr.debug_flags = 0;

	attr.tty = isatty(STDOUT_FILENO) || env_flag("RAWHIDE_TEST_TTY");
	attr.ttyerr = isatty(STDERR_FILENO) || env_flag("RAWHIDE_TEST_TTY");

	if (!strcmp(nl_langinfo(CODESET), "UTF-8"))
		attr.utf = !env_flag("RAWHIDE_PCRE2_NOT_UTF8_DEFAULT");
	else
		attr.utf = env_flag("RAWHIDE_PCRE2_UTF8_DEFAULT");

	attr.dotall_always = env_flag("RAWHIDE_PCRE2_DOTALL_ALWAYS");
	attr.multiline_always = env_flag("RAWHIDE_PCRE2_MULTILINE_ALWAYS");

	attr.report_broken_symlinks = env_flag("RAWHIDE_REPORT_BROKEN_SYMLINKS");
	attr.report_cycles = !env_flag("RAWHIDE_DONT_REPORT_CYCLES");
	attr.facl_solaris_no_trivial = env_flag("RAWHIDE_SOLARIS_ACL_NO_TRIVIAL");
	attr.fea_solaris_no_sunwattr = env_flag("RAWHIDE_SOLARIS_EA_NO_SUNWATTR");
	attr.fea_solaris_no_statinfo = env_flag("RAWHIDE_SOLARIS_EA_NO_STATINFO");
	attr.fea_size = env_int("RAWHIDE_EA_SIZE", 1, -1, 0);
	attr.implicit_expr_heuristic = env_flag("RAWHIDE_ALLOW_IMPLICIT_EXPR_HEURISTIC");
	attr.user_shell = getenv("RAWHIDE_USER_SHELL");
	attr.user_shell_like_csh = env_flag("RAWHIDE_USER_SHELL_LIKE_CSH");
	attr.internal_fnmatch = env_flag("RAWHIDE_INTERNAL_GLOB");
	attr.fnmatch = (attr.internal_fnmatch) ? rhfnmatch : fnmatch;
	attr.no_implicit_path = env_flag("RAWHIDE_NO_IMPLICIT_PATH_MODIFIER");

	attr.test_cmd_max = env_int("RAWHIDE_TEST_CMD_MAX", 1, -1, -1);
	attr.test_attr_format = env_flag("RAWHIDE_TEST_ATTR_FORMAT");
	attr.test_chdir_failure = env_flag("RAWHIDE_TEST_CHDIR_FAILURE");
	attr.test_fchdir_failure = env_flag("RAWHIDE_TEST_FCHDIR_FAILURE");
	attr.test_fdopendir_failure = env_flag("RAWHIDE_TEST_FDOPENDIR_FAILURE");
	attr.test_fstatat_failure = getenv("RAWHIDE_TEST_FSTATAT_FAILURE");
	attr.test_invalid_date = env_flag("RAWHIDE_TEST_INVALID_DATE");
	attr.test_openat_failure = env_flag("RAWHIDE_TEST_OPENAT_FAILURE");
	attr.test_readlinkat_failure = env_flag("RAWHIDE_TEST_READLINKAT_FAILURE");
	attr.test_readlinkat_too_long_failure = env_flag("RAWHIDE_TEST_READLINKAT_TOO_LONG_FAILURE");
	attr.test_implicit_expr_heuristic_despite_f_option = env_flag("RAWHIDE_TEST_IMPLICIT_EXPR_HEURISTIC_DESPITE_F_OPTION");
	attr.test_malloc_fatalsys = env_flag("RAWHIDE_TEST_MALLOC_FATALSYS");
	attr.test_malloc_errorsys = env_flag("RAWHIDE_TEST_MALLOC_ERRORSYS");
	attr.test_realloc_fatalsys = env_flag("RAWHIDE_TEST_REALLOC_FATALSYS");

	/* Parse cmdline options */

	rawhide_init();

	if (argc >= 2 && !strcmp(argv[1], "--help"))
		help_message();

	if (argc >= 2 && !strcmp(argv[1], "--version"))
		version_message();

	while ((o = getopt(argc, argv, ":hVNnf:e:rm:M:D1yYx:X:UldiBsSgoaucv0L:jQEbqptFHIT#?:")) != -1)
	{
		switch (o)
		{
			case 'h':
			{
				opt_h = 1;

				break;
			}

			case 'V':
			{
				opt_V = 1;

				break;
			}

			case 'N':
			{
				opt_N = 1;

				break;
			}

			case 'n':
			{
				opt_n = 1;

				break;
			}

			case 'f':
			{
				if (!optarg || !*optarg)
					fatal("missing -f option argument (see %s -h for help)", prog_name);

				opt_f_list = realloc_or_fatalsys(opt_f_list, ++opt_f * sizeof(*opt_f_list));
				opt_f_list[opt_f - 1] = optarg;

				if (!attr.test_implicit_expr_heuristic_despite_f_option)
					attr.implicit_expr_heuristic = 0;

				break;
			}

			case 'e':
			{
				#if 0 /* Matched by case ':' */
				if (!optarg) /* The argument may be empty (valid syntax) */
					fatal("missing -e option argument (see %s -h for help)", prog_name);
				#endif

				if (opt_e)
					fatal("too many -e options");

				opt_e = optarg;

				break;
			}

			case 'r':
			{
				opt_r = 1;

				break;
			}

			case 'm':
			{
				if (!optarg || !*optarg)
					fatal("missing -m option argument (see %s -h for help)", prog_name);

				errno = 0;
				optarg_int = strtoll(optarg, &endptr, 10);

				if ((endptr && *endptr) || optarg_int < 0 || errno == ERANGE)
					fatal("invalid -m option argument: %s (must be a non-negative integer)", ok(optarg));

				opt_m = optarg_int;

				break;
			}

			case 'M':
			{
				if (!optarg || !*optarg)
					fatal("missing -M option argument (see %s -h for help)", prog_name);

				errno = 0;
				optarg_int = strtoll(optarg, &endptr, 10);

				if ((endptr && *endptr) || optarg_int < 0 || errno == ERANGE)
					fatal("invalid -M option argument: %s (must be a non-negative integer)", ok(optarg));

				opt_M = optarg_int;

				break;
			}

			case 'D':
			{
				attr.depth_first = 1;

				break;
			}

			case '1':
			{
				attr.single_filesystem = 1;

				break;
			}

			case 'y':
			{
				attr.follow_symlinks = 1;

				break;
			}

			case 'Y':
			{
				attr.follow_symlinks = 2;

				break;
			}

			case 'x':
			case 'X':
			{
				if (!optarg || !*optarg)
					fatal("missing -%c option argument (see %s -h for help)", o, prog_name);

				if (attr.command)
				{
					if (o == 'x' && !attr.local)
						fatal("too many -x options");

					if (o == 'X' && attr.local)
						fatal("too many -X options");

					if ((o == 'x' && attr.local) || (o == 'X' && !attr.local))
						fatal("-x and -X options are mutually exclusive");
				}

				attr.command = optarg;
				attr.local = (o == 'X');
				attr.visitf = (attr.local) ? visitf_execute_local : visitf_execute;

				break;
			}

			case 'U':
			{
				if (++opt_U == 3)
				{
					attr.depth_first = attr.unlink = 1;
					attr.visitf = visitf_unlink;
				}

				break;
			}

			case 'l':
			{
				opt_l = 1;

				break;
			}

			case 'd':
			{
				attr.dev_column = opt_l = 1;

				break;
			}

			case 'i':
			{
				attr.ino_column = opt_l = 1;

				break;
			}

			case 'B':
			{
				attr.blksize_column = opt_l = 1;

				break;
			}

			case 's':
			{
				attr.blocks_column = opt_l = 1;

				break;
			}

			case 'S':
			{
				attr.space_column = opt_l = 1;

				break;
			}

			case 'g':
			{
				attr.no_owner_column = opt_l = 1;

				break;
			}

			case 'o':
			{
				attr.no_group_column = opt_l = 1;

				break;
			}

			case 'a':
			case 'u':
			{
				attr.atime_column = opt_l = 1;

				break;
			}

			case 'c':
			{
				attr.ctime_column = opt_l = 1;

				break;
			}

			case 'b':
			{
				attr.btime_column = opt_l = 1;

				break;
			}

			case 'v':
			{
				attr.verbose = 1;

				break;
			}

			case '0':
			{
				attr.nul = 1;

				break;
			}

			case 'L':
			{
				#if 0 /* Matched by case ':' */
				if (!optarg) /* The argument can be empty (no output) */
					fatal("missing -L option argument (see %s -h for help)", prog_name);
				#endif

				if (attr.format)
					fatal("too many -L or -j options");

				attr.format = optarg;
				attr.visitf = visitf_format;

				break;
			}

			case 'j':
			{
				if (attr.format)
					fatal("too many -L or -j options");

				attr.format = "%j\n";
				attr.visitf = visitf_format;

				break;
			}

			case 'Q':
			{
				attr.quote_name = 1;

				break;
			}

			case 'E':
			{
				attr.escape_name = 1;

				break;
			}

			case 'q':
			{
				attr.mask_name = 1;

				break;
			}

			case 'p':
			{
				attr.dir_indicator = 1;

				break;
			}

			case 't':
			{
				attr.most_indicators = 1;

				break;
			}

			case 'F':
			{
				attr.all_indicators = 1;

				break;
			}

			case 'H':
			{
				++attr.human_units;
				opt_l = 1;

				break;
			}

			case 'I':
			{
				++attr.si_units;
				opt_l = 1;

				break;
			}

			case 'T':
			{
				attr.iso_time = 1;
				opt_l = 1;

				break;
			}

			case '#':
			{
				attr.numeric_ids = 1;
				opt_l = 1;

				break;
			}

			case '?':
			{
				/* Invalid options are reported with '?' and optopt set */

				if (optopt && optopt != '?') /* Default optopt can be ? but we use ? so that's ok */
					fatal("invalid option: -%c (see %s -h for help)", optopt, prog_name);

				/* Otherwise, the ? option was supplied */

				if (!optarg || !*optarg)
					fatal("missing -? option argument (see %s -h for help)", prog_name);

				#ifndef NDEBUG
				if (strstr(optarg, "all") || strstr(optarg, "cmdline"))
					attr.debug_flags |= DEBUG_CMDLINE;

				if (strstr(optarg, "all") || strstr(optarg, "parser"))
					attr.debug_flags |= DEBUG_PARSER;

				if (strstr(optarg, "all") || strstr(optarg, "traversal"))
					attr.debug_flags |= DEBUG_TRAVERSAL;

				if (strstr(optarg, "all") || strstr(optarg, "exec"))
					attr.debug_flags |= DEBUG_EXEC;

				if (strstr(optarg, "extra"))
					attr.debug_flags |= DEBUG_EXTRA;

				if (!attr.debug_flags)
					fatal("invalid -? option argument: %s (needs: cmdline parser traversal exec all extra)", ok(optarg));

				debug(("attr.debug_flags = 0x%x", attr.debug_flags));
				#endif

				break;
			}

			case ':':
			{
				fatal("missing -%c option argument (see %s -h for help)", optopt, prog_name);
			}
		}
	}

	if (attr.verbose && !opt_l && !attr.nul && !attr.command && !attr.unlink && !attr.format)
		opt_l = 1;

	if (attr.command && !attr.local && opt_l)
		fatal("-x and -l options are mutually exclusive");

	if (attr.command && !attr.local && attr.nul)
		fatal("-x and -0 options are mutually exclusive");

	if (attr.command && !attr.local && attr.format)
		fatal("-x and -L/-j options are mutually exclusive");

	if (attr.command && attr.local && opt_l)
		fatal("-X and -l options are mutually exclusive");

	if (attr.command && attr.local && attr.nul)
		fatal("-X and -0 options are mutually exclusive");

	if (attr.command && attr.local && attr.format)
		fatal("-X and -L/-j options are mutually exclusive");

	if (opt_l && attr.nul)
		fatal("-l and -0 options are mutually exclusive");

	if (opt_l && attr.format)
		fatal("-l and -L/-j options are mutually exclusive");

	if (attr.unlink && attr.command && !attr.local)
		fatal("-x and -U options are mutually exclusive");

	if (attr.unlink && attr.command && attr.local)
		fatal("-X and -U options are mutually exclusive");

	if (attr.unlink && opt_l)
		fatal("-l and -U options are mutually exclusive");

	if (attr.unlink && attr.nul)
		fatal("-0 and -U options are mutually exclusive");

	if (attr.nul && attr.format)
		fatal("-0 and -L/-j options are mutually exclusive");

	if (attr.unlink && attr.format)
		fatal("-U and -L/-j options are mutually exclusive");

	if (attr.escape_name && attr.mask_name)
		fatal("-E and -q options are mutually exclusive");

	if (attr.human_units && attr.si_units)
		fatal("-H and -I options are mutually exclusive");

	if (opt_r != 0 && opt_M != -1)
		fatal("-r and -M options are mutually exclusive");

	if (opt_r != 0 && opt_m != -1)
		fatal("-r and -m options are mutually exclusive");

	if (opt_U && !attr.unlink)
		fatal("-U option only works if supplied three times");

	if (opt_l)
		attr.visitf = visitf_long;

	if (opt_h)
		help_message();

	if (opt_V)
		version_message();

	/* For -X, "cmd".sh, and "cmd".ush, remove cwd from $PATH (after debug config), affects -x as well */

	if (remove_danger_from_path() == -1)
		fatalsys("failed to remove cwd from path");

	/* Create path buffers */

	max_pathlen = pathconf("/", _PC_PATH_MAX);
	max_pathlen = (max_pathlen == -1) ? 1024 : max_pathlen + 2;
	max_pathlen = env_int("RAWHIDE_TEST_PATHLEN_MAX", 1, max_pathlen, max_pathlen);
	pathbufsize = max_pathlen + 1;

	initfile = malloc_or_fatalsys(pathbufsize);
	initdir = malloc_or_fatalsys(pathbufsize);
	initpattern = malloc_or_fatalsys(pathbufsize);

	/* Load /etc/rawhide.conf and /etc/rawhide.conf.d (unless -N) */

	if (opt_N == 0)
	{
		char *conf, *env;

		conf = (geteuid() && (env = getenv("RAWHIDE_CONFIG")) && *env) ? env : RAWHIDE_CONF;

		if (stat(conf, statbuf) != -1)
			load_program_file(conf);

		load_config_dir(conf, initdir, initpattern, pathbufsize);
	}

	/* Load ~/.rhrc and ~/.rhrc.d (unless -n) */

	if (opt_n == 0)
	{
		char *env, *rc = NULL, *home = NULL;
		struct passwd *pwd;

		if (geteuid() && (env = getenv("RAWHIDE_RC")) && *env)
			rc = env;
		else if ((pwd = getpwuid(getuid())))
			home = pwd->pw_dir;

		*initfile = '\0';

		if (rc && strlcpy(initfile, rc, pathbufsize) >= pathbufsize)
			error("path is too long: %s", ok(rc));
		else if (home && (strlcpy(initfile, home, pathbufsize) >= pathbufsize || strlcat(initfile, RAWHIDE_RC, pathbufsize) >= pathbufsize))
			error("path is too long: %s%s", ok(home), ok2(RAWHIDE_RC));
		else
		{
			if (stat(initfile, statbuf) != -1)
				load_program_file(initfile);

			load_config_dir(initfile, initdir, initpattern, pathbufsize);
		}
	}

	/* Load the -f file-and-or-dir ("-" is stdin) */

	for (i = 0; i < opt_f; ++i)
	{
		debug(("-f option: %s", opt_f_list[i]));

		if (!strcmp(opt_f_list[i], "-"))
		{
			if (stdin_read)
				fatal("invalid -f option argument: - (stdin can only be read once)");

			stdin_read = 1;

			expfname = "stdin";
			expfile = stdin;
			parse_program();
			expfile = NULL;
		}
		else
		{
			if (stat(opt_f_list[i], statbuf) == -1)
				fatalsys("invalid -f option argument: %s", ok(opt_f_list[i]));

			if (isreg(statbuf) || ischr(statbuf) || isfifo(statbuf))
			{
				if (!(expfile = fopen(opt_f_list[i], "r")))
					fatalsys("%s", ok(opt_f_list[i]));

				expfname = opt_f_list[i];
				parse_program();
				fclose(expfile);
				expfile = NULL;

				load_config_dir(opt_f_list[i], initdir, initpattern, pathbufsize);
			}
			else if (isdir(statbuf))
				load_program_dir(opt_f_list[i], initpattern, pathbufsize);
			else
				fatal("invalid -f option argument: %s (not a file or directory)", ok(opt_f_list[i]));
		}
	}

	/* Clean up */

	if (opt_f_list)
	{
		free(opt_f_list);
		opt_f_list = NULL;
	}

	free(initfile);
	initfile = NULL;

	free(initdir);
	initdir = NULL;

	free(initpattern);
	initpattern = NULL;

	/* Parse the -e argument */

	if (opt_e)
	{
		debug(("explicit -e option: %s", opt_e));

		opt_e_expr = startPC;
		load_program_str(opt_e);
		opt_e_expr = (startPC != opt_e_expr);
	}

	/* If no explicit -e expr, search remaining arguments for an implicit expr (if allowed) */

	if (!opt_e_expr && attr.implicit_expr_heuristic)
	{
		char *posp;
		int posi;

		debug(("checking remaining arguments for an expression"));

		for (i = optind; i < argc; i++)
		{
			/* If it exists in the filesystem, it's a path */

			if (lstat(argv[i], statbuf) != -1)
			{
				debug(("argv[%d] = %s (path, it exists)", i, argv[i]));

				continue;
			}

			/* If it contains characters unlikely to be in a path, it's probably an expression */

			if (!(posi = strcspn(argv[i], "?:|&^=!<>*%$\"\\[]{};\n")) || argv[i][posi] != '\0')
			{
				debug(("argv[%d] = %s (unlikely chars for a path, probably an expression)", i, argv[i]));

				expr_index = i;

				break;
			}

			/* If part of it exists in the filesystem (ancestors), it's probably an attempt at a path */

			if ((posp = strrchr(argv[i], '/')))
			{
				char *tmp = strdup(argv[i]);

				tmp[posp - argv[i]] = '\0';

				debug(("argv[%d] = %s (checking if %s exists)", i, argv[i], tmp));

				if (lstat(tmp, statbuf) != -1)
				{
					debug(("argv[%d] = %s (%s exists, it's an attempt at a path)", i, argv[i], tmp));
					free(tmp);

					continue;
				}

				if ((posp = strrchr(tmp, '/')) && posp > tmp)
				{
					*posp = '\0';

					debug(("argv[%d] = %s (checking if %s exists)", i, argv[i], tmp));

					if (lstat(tmp, statbuf) != -1)
					{
						debug(("argv[%d] = %s (%s exists, it's an attempt at a path)", i, argv[i], tmp));
						free(tmp);

						continue;
					}

					if ((posp = strrchr(tmp, '/')) && posp > tmp)
					{
						*posp = '\0';

						debug(("argv[%d] = %s (checking if %s exists)", i, argv[i], tmp));

						if (lstat(tmp, statbuf) != -1)
						{
							debug(("argv[%d] = %s (%s exists, it's an attempt at a path)", i, argv[i], tmp));
							free(tmp);

							continue;
						}
					}
				}

				free(tmp);
			}

			/* Assume it's an expression */

			debug(("argv[%d] = %s (assuming it's an expression)", i, argv[i]));
			expr_index = i;

			break;
		}
	}

	if (expr_index)
	{
		debug(("implicit -e option: argv[%d] = %s", expr_index, argv[expr_index]));

		load_program_str(argv[expr_index]);
	}

	/* Default to "1" if no file test expression has been specified */

	if (startPC == -1)
	{
		debug(("default expression: 1"));

		load_program_str("1");
	}

	/* Deallocate symbols */

	rawhide_finish();

	/* Initialize depth limits in the global attr runtime state */

	attr.depth_limit = sysconf(_SC_OPEN_MAX) - 5; /* stdin, stdout, stderr, dot_fd/dirsize/attropen+1 */
	attr.depth_limit = env_int("RAWHIDE_TEST_DEPTH_LIMIT", 1, attr.depth_limit, attr.depth_limit);

	if (opt_m > attr.depth_limit + 1)
		fatal("invalid -m option argument: %lld (must be no more than %lld)", opt_m, attr.depth_limit + 1);

	if (opt_M > attr.depth_limit + 1)
		fatal("invalid -M option argument: %lld (must be no more than %lld)", opt_M, attr.depth_limit + 1);

	attr.min_depth = (opt_m != -1) ? opt_m : (opt_r) ? 1 : 0;
	attr.max_depth = (opt_M != -1) ? opt_M : (opt_r) ? 1 : attr.depth_limit + 1; /* +1 so we're told when we reach it */

	debug(("min_depth = %lld, max_depth = %lld, depth_limit = %lld", attr.min_depth, attr.max_depth, attr.depth_limit));

	/* Initialize a search stack for filesystem cycle detection */

	attr.search_stack = (point_t *)malloc_or_fatalsys((attr.max_depth + 1) * sizeof(point_t));

	/* Prepare to deallocate libmagic resources */

	#ifdef HAVE_MAGIC
	atexit(magic_cleanup);
	#endif

	/* Find matches in the given directories (or the current working directory) */

	for (; optind < argc; optind++)
	{
		if (optind == expr_index) /* Skip implicit expression */
			continue;

		if (rawhide_search(argv[optind]) == -1)
			attr.exit_status = EXIT_FAILURE;

		++any;
	}

	if (!any)
	{
		if (rawhide_search(".") == -1)
			attr.exit_status = EXIT_FAILURE;
	}

	free(attr.search_stack);
	if (attr.user_shell_copy)
		free(attr.user_shell_copy);
	if (attr.defused_path)
		free(attr.defused_path);
	if (attr.defused_basename)
		free(attr.defused_basename);

	exit(attr.exit_status);
}

/* vi:set ts=4 sw=4: */
