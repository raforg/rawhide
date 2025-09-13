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

#ifndef RAWHIDE_RH_H
#define RAWHIDE_RH_H

/* Configuration file locations (system-wide and user-specific) */

#ifndef ETCDIR
#define ETCDIR "/etc"
#endif

#define RAWHIDE_CONF ETCDIR "/rawhide.conf" /* System-wide config file */
#define RAWHIDE_RC   "/.rhrc"               /* User-specific config file */
#define DOTDDIR      ".d"                   /* Directory suffix */

/* Tokens (plus single-character operators) */

#define OR          256 /* || */
#define AND         257 /* && */
#define LE          258 /* <= */
#define GE          259 /* >= */
#define NE          260 /* != */
#define EQ          261 /* == */
#define SHIFTL      262 /* << */
#define SHIFTR      263 /* >> */
#define NOP         264 /* No-operation - unused */
#define NUMBER      265 /* Literal numbers and symbolic constants */
#define STRING      266 /* Glob pattern strings */
#define FIELD       267 /* File fields (size, mode, nlink, ...) */
#define FUNCTION    268 /* User defined function */
#define RETURN      269 /* Return keyword */
#define PARAM       270 /* Function parameter */
#define IDENTIFIER  271 /* New function name */
#define REFFILE     272 /* Reference file field */
#define PATMOD      273 /* Pattern modifier */

/* Static sizes for the VM */

#ifndef MAX_PROGRAM_SIZE
#define MAX_PROGRAM_SIZE 2000000 /* Size of the program */
#endif
#ifndef MAX_STACK_SIZE
#define MAX_STACK_SIZE 1000000 /* Size of the stack */
#endif
#ifndef MAX_DATA_SIZE
#define MAX_DATA_SIZE 200000 /* Size of the data (patterns, reference file paths, shell commands) */
#endif
#ifndef MAX_REFFILE_SIZE
#define MAX_REFFILE_SIZE 10000 /* Number of available reference files (e.g., "/path".mtime) */
#endif
#ifndef MAX_IDENT_LENGTH
#define MAX_IDENT_LENGTH 200 /* Length limit of an identifier, username, or groupname */
#endif

/* Debug subjects */

#define DEBUG_EXTRA     0x01
#define DEBUG_CMDLINE   0x02
#define DEBUG_PARSER    0x04
#define DEBUG_TRAVERSAL 0x08
#define DEBUG_EXEC      0x10

/* Define type llong for 64 bits on 32-bit systems */

typedef long long int llong;
typedef long long unsigned int ullong;

/* Define macros for file types */

#define isreg(statbuf)  (((statbuf)->st_mode & S_IFMT) == S_IFREG)
#define isdir(statbuf)  (((statbuf)->st_mode & S_IFMT) == S_IFDIR)
#define isblk(statbuf)  (((statbuf)->st_mode & S_IFMT) == S_IFBLK)
#define ischr(statbuf)  (((statbuf)->st_mode & S_IFMT) == S_IFCHR)
#define islink(statbuf) (((statbuf)->st_mode & S_IFMT) == S_IFLNK)
#define issock(statbuf) (((statbuf)->st_mode & S_IFMT) == S_IFSOCK)
#define isfifo(statbuf) (((statbuf)->st_mode & S_IFMT) == S_IFIFO)
#ifdef S_IFDOOR /* Solaris only */
#define isdoor(statbuf) (((statbuf)->st_mode & S_IFMT) == S_IFDOOR)
#else
#define isdoor(statbuf) (0)
#endif

/* Define macros for user shell */

#define USER_SHELLV_SIZE 11
#define FOR_SH  0
#define FOR_USH 1

/* Structure of a rawhide assembly instruction */

typedef struct instr_t instr_t;
struct instr_t
{
	void (*func)(llong); /* The function that implements the instruction */
	llong value;         /* The argument to the function */
};

/* Structure of a symbol */

typedef struct symbol_t symbol_t;
struct symbol_t
{
	char *name;          /* The symbol name: built-ins, functions, parameters */
	int type;            /* Token type: NUMBER, STRING, FIELD, FUNCTION, RETURN, PARAM, IDENTIFIER, REFFILE, PATMOD */
	llong value;         /* Argument to func() */
	void (*func)(llong); /* Instruction action */
	symbol_t *next;      /* Pointer to the next symbol entry */
};

/* Structure defining a point in the traversal (for loop detection) */

typedef struct point_t point_t;
struct point_t
{
	dev_t dev; /* The search directory device */
	ino_t ino; /* The search directory inode */
};

/* Structure defining the runtime environment */

#ifdef HAVE_MAGIC
#include <magic.h>
#endif

typedef struct runtime_t runtime_t;
struct runtime_t
{
	struct stat statbuf[1]; /* Stat info of the current candidate file */
	int dirsize_done;       /* Have we counted a directory's contents yet? */

	char *search_path;      /* Starting search directory (For -L) */
	size_t search_path_len; /* Length of the starting search directory */

	char *fpath;            /* Path to the current candidate file */
	llong fpath_size;       /* Size of the dynamic fpath buffer */

	char *defused_path;           /* Copy of fpath with leading ./ for %s */
	size_t defused_path_size;     /* Allocated size of defused_fpath */
	char *defused_basename;       /* Copy of fpath with leading ./ for %S */
	size_t defused_basename_size; /* Allocated size of defused_basename */

	int ftarget_done;       /* Have we read the current candidate symlink's target path yet? */
	char *ftarget;          /* Target path of the current candidate symlink (long-lived, on-demand) */

	int facl_done;          /* Have we loaded the current candidate's access control lists yet? */
	char *facl;             /* ACL as lines of text ("POSIX"/FreeBSD) or comma-separated (Solaris) */
	char *facl_verbose;     /* ACL as non-compact form (FreeBSD/Solaris) */
	int facl_solaris_no_trivial; /* Suppress trivial ACLs on Solaris? */

	int fea_done;           /* Have we loaded the extended attributes yet? */
	int fea_ok;             /* Have we loaded the extended attributes successfully? */
	char *fea;              /* Extended attributes as lines of text (long-lived, on-demand) */
	char *fea_selinux;      /* "security.selinux" extended attribute, if any */
	int fea_real;           /* Are there any real EAs (i.e. non-ACL/non-selinux ones on Linux)? */
	llong fea_size;         /* Non-default size to allocate for extended attributes? */
	int fea_solaris_no_sunwattr; /* Suppress ubiquitous SUNWattr_ro/SUNWattr_rw EAs on Solaris? */
	int fea_solaris_no_statinfo; /* Suppress artificial stat(2) info EAs on Solaris? */
	int no_implicit_expr_heuristic; /* Suppress implicit search criteria expression heuristic */

	char *user_shell;        /* The value of $RAWHIDE_USER_SHELL to override the user's login shell, for .ush */
	int user_shell_like_csh; /* The value of $RAWHIDE_USER_SHELL_LIKE_CSH (i.e., shell doesn't take $0 argument, or --) */
	int user_shell_init_done;/* Have we obtained the user shell details? */
	char *user_shell_copy;   /* A dynamic mutable copy of the user_shell, for splitting purposes */
	char *user_shellv[USER_SHELLV_SIZE]; /* The user shell, possibly split into path and options */
	char *ush_basename;        /* The user shell's basename, for $0 */
	int ush_command_is_optarg; /* The user shell's -c option takes command string as its argument (so -- after command) */
	int ush_like_csh;          /* Is the user shell like csh? (No -- at all, and no $0) */
	int ush_like_fish;         /* Is the user shell like fish? (-- after command, no $0, %s=$argv[1] %S=$argv[2] */

	#ifdef HAVE_MAGIC
	magic_t what_cookie;        /* Libmagic data for file type searches */
	magic_t mime_cookie;        /* Libmagic data for mime type searches */
	magic_t what_follow_cookie; /* Libmagic data for file type searches when following symlinks */
	magic_t mime_follow_cookie; /* Libmagic data for mime type searches when following symlinks */
	#endif
	int what_done;          /* Have we loaded the file type yet? */
	int mime_done;          /* Have we loaded the mime type yet? */
	const char *what;       /* The file type (libmagic-managed data) */
	const char *mime;       /* The mime type (libmagic-managed data) */

	int body_done;          /* Have we read the content yet? */
	char *body;             /* The content of the file */
	size_t body_size;       /* The size of the file content buffer (file size + 1 nul byte) */
	size_t body_length;     /* The length of the file content */

	int attr_done;          /* Have we loaded the Linux ext2-style attributes/BSD flags yet? */
	unsigned long attr;     /* Linux ext2-style attributes/BSD flags */
	int proj_done;          /* Have we loaded the Linux ext2-style project yet? */
	unsigned long proj;     /* Linux ext2-style project */
	int gen_done;           /* Have we loaded the Linux ext2-style generation yet? */
	unsigned long gen;      /* Linux ext2-style generation */

	char *ttybuf;           /* Temp space for printf_sanitized() (long-lived, on-demand) */
	char *formatbuf;        /* Temp space for visitf_format() (long-lived, on-demand) */

	int parent_fd;          /* File descriptor of parent directory */
	char *basename;         /* Base name relative to parent_fd */
	llong depth;            /* Relative depth of the current candidate file */
	void (*visitf)(void);   /* Visitor/action function for matching entries */
	point_t *search_stack;  /* Stack of search points (for loop detection) */

	int debug_flags;        /* Debug bit mask to indicate debugging */
	int prune;              /* Flag to indicate pruning */
	int pruned;             /* Flag to prevent matching of pruned entry */
	int exit;               /* Flag to indicate exiting */
	int exit_status;        /* Exit status for rh */

	llong min_depth;        /* Flag for the -m option: Minimum depth to report or act on */
	llong max_depth;        /* Flag for the -M option: Maximum depth to search */
	llong depth_limit;      /* System imposed depth limit (max open files - 5) */
	int depth_first;        /* Flag for the -D option: depth-first search */
	int single_filesystem;  /* Flag for the -1 option: single filesystem */
	dev_t fs_dev;           /* The filesystem's dev (for the -1 option) */
	int follow_symlinks;    /* Flag for the -y and -Y options: 0=No 1=y 2=Y */
	int followed;           /* Have we followed a symlink for this candidate? */
	int tty;                /* Is stdout a tty? (to default to -q) */
	int ttyerr;             /* Is stderr a tty? */
	int utf;                /* Does the user not want default utf8 suppressed in pcre? */
	int dotall_always;      /* Does the user always want /s by default in pcre? */
	int multiline_always;   /* Does the user always want /m by default in pcre? */
	int report_broken_symlinks; /* Does the user want to report broken symlinks? */
	int report_cycles;      /* Does the user want to report filesystem cycles (default)? */

	int linkstat_done;      /* Have we attempted to stat the current candidate symlink target yet? */
	int linkstat_ok;        /* Did statting the current candidate symlink target work? */
	int linkstat_errno;     /* If not, what was the error? (For -L) */
	int linkstat_baselen;   /* Length of the current candidate symlink's base name? */
	struct stat linkstatbuf[1]; /* Stat info of the current candidate symlink target */
	int linkdirsize_done;   /* Have we counted the target directory's contents yet? */

	char *command;          /* Command to execute for matching entries: -x or -X */
	int local;              /* Commands are executed locally: -X */
	int unlink;             /* Flag for the -U option: unlink */

	int dev_column;         /* Flag for the -d option: Include device column */
	int ino_column;         /* Flag for the -i option: Include inode column */
	int blksize_column;     /* Flag for the -B option: Include blksize column */
	int blocks_column;      /* Flag for the -s option: Include blocks column */
	int space_column;       /* Flag for the -S option: Include space column (blocks * 512) */
	int no_owner_column;    /* Flag for the -g option: Exclude user/owner column */
	int no_group_column;    /* Flag for the -o option: Exclude group column */
	int atime_column;       /* Flag for the -au option: Show atime rather than mtime */
	int ctime_column;       /* Flag for the -c option: Show ctime rather than mtime */
	int verbose;            /* Flag for the -v option: Include all columns */
	int nul;                /* Flag for the -0 option: nul byte rather than newline */
	char *format;           /* Format for the -L option */

	int dev_major_column_width;  /* Maximum width of the dev major column so far */
	int dev_minor_column_width;  /* Maximum width of the dev minor column so far */
	int ino_column_width;        /* Maximum width of the inode column so far */
	int blksize_column_width;    /* Maximum width of the blksize column so far */
	int blocks_column_width;     /* Maximum width of the blocks column so far */
	int space_column_width;      /* Maximum width of the space column so far */
	int nlink_column_width;      /* Maximum width of the nlink column so far */
	int user_column_width;       /* Maximum width of the user/owner column so far */
	int group_column_width;      /* Maximum width of the group column so far */
	int size_column_width;       /* Maximum width of the size column so far */
	int rdev_major_column_width; /* Maximum width of the rdev major column so far */
	int rdev_minor_column_width; /* Maximum width of the rdev minor column so far */

	int quote_name;         /* Flag for the -Q option */
	int escape_name;        /* Flag for the -E/-b option */
	int mask_name;          /* Flag for the -q option */
	int dir_indicator;      /* Flag for the -p option */
	int most_indicators;    /* Flag for the -t option */
	int all_indicators;     /* Flag for the -F option */

	int human_units;        /* Flag for the -H option: "human readable" sizes (1=roundup 2=roundhalfup)) */
	int si_units;           /* Flag for the -I option: SI prefixes for sizes (1=roundup 2=roundhalfup) */
	int iso_time;           /* Flag for the -T option: ISO time format */
	int numeric_ids;        /* Flag for the -# option: numeric uid/gid */

	int test_cmd_max;       /* Cache test-related getenv values (e.g., fault injection) */
	int test_attr_format;
	int test_chdir_failure;
	int test_fchdir_failure;
	int test_fdopendir_failure;
	char *test_fstatat_failure;
	int test_invalid_date;
	int test_openat_failure;
	int test_readlinkat_failure;
	int test_readlinkat_too_long_failure;
};

/* Structure of a reference file */

typedef struct reffile_t reffile_t;
struct reffile_t
{
	int exists;             /* Whether or not lstat(2) succeeded for this reference file */
	llong fpathi;           /* Index into Strbuf of the reference file path (e.g., "fpath".mtime) */
	llong baselen;          /* The length of the base name */
	struct stat statbuf[1]; /* The stat structure for the referenced file */
	int dirsize_done;       /* Have we counted a reference directory's contents yet? */
	int attr_done;          /* Have we loaded the Linux ext2-style attributes/BSD flags yet? */
	unsigned long attr;     /* Linux ext2-style attributes */
	int proj_done;          /* Have we loaded the Linux ext2-style project yet? */
	unsigned long proj;     /* Linux ext2-style project */
	int gen_done;           /* Have we loaded the Linux ext2-style generation yet? */
	unsigned long gen;      /* Linux ext2-style generation */
};

/* Global variables */

#ifndef DATA
extern char *prog_name;     /* Name of this program for messages */

extern symbol_t *symbols;   /* Symbol table */
extern symbol_t *tokensym;  /* Current token symbol */
extern llong tokenval;      /* Current token value */
extern llong token;         /* Current token code */

extern instr_t Program[];   /* Program instructions */
extern llong PC;            /* Program counter */
extern llong startPC;       /* Initial program counter */

extern char Strbuf[];       /* Storage for string literals */
extern llong strfree;       /* Index into Strbuf for the next string literal */

extern reffile_t RefFile[]; /* Storage for reference files (e.g., "fpath".mtime) */
extern llong reffree;       /* Index into RefFile for the next reference file */

extern llong Stack[];       /* Stack */
extern llong SP;            /* Stack pointer */
extern llong FP;            /* Frame pointer */

extern runtime_t attr;      /* Configuration and runtime state */

extern char *expstr;        /* The -e option argument (search criteria expression) */
extern char *expfname;      /* The -f option argument (filename) or current config file */
extern FILE *expfile;       /* FILE handle corresponding to the -f option argument or current config file */
#endif

#endif

/* vi:set ts=4 sw=4: */
