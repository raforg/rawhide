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

#define _GNU_SOURCE /* For FNM_CASEFOLD in <fnmatch.h> */
#define _FILE_OFFSET_BITS 64 /* For 64-bit off_t on 32-bit systems (Not AIX) */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fnmatch.h>
#include <time.h>
#include <sys/stat.h>

#define DATA
#include "rh.h"
#include "rhdata.h"
#include "rhcmds.h"
#include "rherr.h"

char *prog_name;                     /* Name of this program for messages */

symbol_t *symbols;                   /* Symbol table */
symbol_t *tokensym;                  /* Current token symbol */
llong tokenval;                      /* Current token value */
llong token;                         /* Current token code */

instr_t Program[MAX_PROGRAM_SIZE];   /* Program instructions */
llong PC;                            /* Program counter */
llong startPC;                       /* Initial program counter */

char Strbuf[MAX_DATA_SIZE];          /* Storage for string literals */
llong strfree = 0;                   /* Index into Strbuf for the next string literal */

reffile_t RefFile[MAX_REFFILE_SIZE]; /* Storage for reference files */
llong reffree = 0;                   /* Index into RefFile for the next reference file */

llong Stack[MAX_STACK_SIZE + 3];     /* Stack */
llong SP;                            /* Stack pointer */
llong FP;                            /* Frame pointer */

runtime_t attr;                      /* Configuration and runtime state */

char *expstr;   /* The -e option argument (search criteria expression) */
char *expfname; /* The -f option argument (filename) or current config file */
FILE *expfile;  /* FILE handle corresponding to the -f option argument or current config file */

#ifndef S_IFDOOR
#define S_IFDOOR 0150000
#endif

/* Initial built-in symbol table */

static symbol_t init_syms[] =
{
	/* stat structure fields */

	{ "dev",     FIELD, 0, c_dev,     NULL },
	{ "major",   FIELD, 0, c_major,   NULL },
	{ "minor",   FIELD, 0, c_minor,   NULL },
	{ "ino",     FIELD, 0, c_ino,     NULL },
	{ "mode",    FIELD, 0, c_mode,    NULL },
	{ "nlink",   FIELD, 0, c_nlink,   NULL },
	{ "uid",     FIELD, 0, c_uid,     NULL },
	{ "gid",     FIELD, 0, c_gid,     NULL },
	{ "rdev",    FIELD, 0, c_rdev,    NULL },
	{ "rmajor",  FIELD, 0, c_rmajor,  NULL },
	{ "rminor",  FIELD, 0, c_rminor,  NULL },
	{ "size",    FIELD, 0, c_size,    NULL },
	{ "blksize", FIELD, 0, c_blksize, NULL },
	{ "blocks",  FIELD, 0, c_blocks,  NULL },
	{ "atime",   FIELD, 0, c_atime,   NULL },
	{ "mtime",   FIELD, 0, c_mtime,   NULL },
	{ "ctime",   FIELD, 0, c_ctime,   NULL },

	/* Linux ext2-style file attributes and BSD file flags */

#if HAVE_ATTR || HAVE_FLAGS
	{ "attr",    FIELD, 0, c_attr,   NULL },
#endif
#if HAVE_ATTR
	{ "proj",    FIELD, 0, c_proj,   NULL },
	{ "gen",     FIELD, 0, c_gen,    NULL },
#endif

	/* Miscellaneous functions and control flow */

	{ "nouser",     FIELD, 0, c_nouser,     NULL },
	{ "nogroup",    FIELD, 0, c_nogroup,    NULL },
	{ "readable",   FIELD, 0, c_readable,   NULL },
	{ "writable",   FIELD, 0, c_writable,   NULL },
	{ "executable", FIELD, 0, c_executable, NULL },
	{ "strlen",     FIELD, 0, c_strlen,     NULL },
	{ "depth",      FIELD, 0, c_depth,      NULL },
	{ "prune",      FIELD, 0, c_prune,      NULL },
	{ "trim",       FIELD, 0, c_trim,       NULL },
	{ "exit",       FIELD, 0, c_exit,       NULL },

	/* Time */

	{ "now",    NUMBER, 0,                            c_number, NULL }, /* Set by rawhide_init() */
	{ "today",  NUMBER, 0,                            c_number, NULL }, /* Set by rawhide_init() */
	{ "second", NUMBER, 1,                            c_number, NULL },
	{ "minute", NUMBER, 60,                           c_number, NULL },
	{ "hour",   NUMBER, 60 * 60,                      c_number, NULL },
	{ "day",    NUMBER, 60 * 60 * 24,                 c_number, NULL },
	{ "week",   NUMBER, 60 * 60 * 24 * 7,             c_number, NULL },
	{ "month",  NUMBER, 60 * 60 * 24 * 365.2425 / 12, c_number, NULL },
	{ "year",   NUMBER, 60 * 60 * 24 * 365.2425,      c_number, NULL },

	/* Constants from <sys/stat.h> */

	{ "IFREG",  NUMBER, S_IFREG,  c_number, NULL },
	{ "IFDIR",  NUMBER, S_IFDIR,  c_number, NULL },
	{ "IFLNK",  NUMBER, S_IFLNK,  c_number, NULL },
	{ "IFCHR",  NUMBER, S_IFCHR,  c_number, NULL },
	{ "IFBLK",  NUMBER, S_IFBLK,  c_number, NULL },
	{ "IFSOCK", NUMBER, S_IFSOCK, c_number, NULL },
	{ "IFIFO",  NUMBER, S_IFIFO,  c_number, NULL },
	{ "IFDOOR", NUMBER, S_IFDOOR, c_number, NULL },
	{ "IFMT",   NUMBER, S_IFMT,   c_number, NULL },
	{ "ISUID",  NUMBER, S_ISUID,  c_number, NULL },
	{ "ISGID",  NUMBER, S_ISGID,  c_number, NULL },
	{ "ISVTX",  NUMBER, S_ISVTX,  c_number, NULL },
	{ "IRWXU",  NUMBER, S_IRWXU,  c_number, NULL },
	{ "IRUSR",  NUMBER, S_IRUSR,  c_number, NULL },
	{ "IWUSR",  NUMBER, S_IWUSR,  c_number, NULL },
	{ "IXUSR",  NUMBER, S_IXUSR,  c_number, NULL },
	{ "IRWXG",  NUMBER, S_IRWXG,  c_number, NULL },
	{ "IRGRP",  NUMBER, S_IRGRP,  c_number, NULL },
	{ "IWGRP",  NUMBER, S_IWGRP,  c_number, NULL },
	{ "IXGRP",  NUMBER, S_IXGRP,  c_number, NULL },
	{ "IRWXO",  NUMBER, S_IRWXO,  c_number, NULL },
	{ "IROTH",  NUMBER, S_IROTH,  c_number, NULL },
	{ "IWOTH",  NUMBER, S_IWOTH,  c_number, NULL },
	{ "IXOTH",  NUMBER, S_IXOTH,  c_number, NULL },

	/* Symlink target stat structure fields */

	{ "texists",   FIELD, 0, t_exists,  NULL },
	{ "tdev",      FIELD, 0, t_dev,     NULL },
	{ "tmajor",    FIELD, 0, t_major,   NULL },
	{ "tminor",    FIELD, 0, t_minor,   NULL },
	{ "tino",      FIELD, 0, t_ino,     NULL },
	{ "tmode",     FIELD, 0, t_mode,    NULL },
	{ "tnlink",    FIELD, 0, t_nlink,   NULL },
	{ "tuid",      FIELD, 0, t_uid,     NULL },
	{ "tgid",      FIELD, 0, t_gid,     NULL },
	{ "trdev",     FIELD, 0, t_rdev,    NULL },
	{ "trmajor",   FIELD, 0, t_rmajor,  NULL },
	{ "trminor",   FIELD, 0, t_rminor,  NULL },
	{ "tsize",     FIELD, 0, t_size,    NULL },
	{ "tblksize",  FIELD, 0, t_blksize, NULL },
	{ "tblocks",   FIELD, 0, t_blocks,  NULL },
	{ "tatime",    FIELD, 0, t_atime,   NULL },
	{ "tmtime",    FIELD, 0, t_mtime,   NULL },
	{ "tctime",    FIELD, 0, t_ctime,   NULL },
	{ "tstrlen",   FIELD, 0, t_strlen,  NULL },

	/* Reference file stat structure fields */

	{ ".exists",   REFFILE, 0, r_exists,  NULL },
	{ ".dev",      REFFILE, 0, r_dev,     NULL },
	{ ".major",    REFFILE, 0, r_major,   NULL },
	{ ".minor",    REFFILE, 0, r_minor,   NULL },
	{ ".ino",      REFFILE, 0, r_ino,     NULL },
	{ ".mode",     REFFILE, 0, r_mode,    NULL },
	{ ".type",     REFFILE, 0, r_type,    NULL },
	{ ".perm",     REFFILE, 0, r_perm,    NULL },
	{ ".nlink",    REFFILE, 0, r_nlink,   NULL },
	{ ".uid",      REFFILE, 0, r_uid,     NULL },
	{ ".gid",      REFFILE, 0, r_gid,     NULL },
	{ ".rdev",     REFFILE, 0, r_rdev,    NULL },
	{ ".rmajor",   REFFILE, 0, r_rmajor,  NULL },
	{ ".rminor",   REFFILE, 0, r_rminor,  NULL },
	{ ".size",     REFFILE, 0, r_size,    NULL },
	{ ".blksize",  REFFILE, 0, r_blksize, NULL },
	{ ".blocks",   REFFILE, 0, r_blocks,  NULL },
	{ ".atime",    REFFILE, 0, r_atime,   NULL },
	{ ".mtime",    REFFILE, 0, r_mtime,   NULL },
	{ ".ctime",    REFFILE, 0, r_ctime,   NULL },
#if HAVE_ATTR || HAVE_FLAGS
	{ ".attr",     REFFILE, 0, r_attr,    NULL },
#endif
#if HAVE_ATTR
	{ ".proj",     REFFILE, 0, r_proj,    NULL },
	{ ".gen",      REFFILE, 0, r_gen,     NULL },
#endif
	{ ".strlen",   REFFILE, 0, r_strlen,  NULL },

	{ ".inode",    REFFILE, 0, r_ino,     NULL },
	{ ".nlinks",   REFFILE, 0, r_nlink,   NULL },
	{ ".user",     REFFILE, 0, r_uid,     NULL },
	{ ".group",    REFFILE, 0, r_gid,     NULL },
	{ ".sz",       REFFILE, 0, r_size,    NULL },
	{ ".accessed", REFFILE, 0, r_atime,   NULL },
	{ ".modified", REFFILE, 0, r_mtime,   NULL },
	{ ".changed",  REFFILE, 0, r_ctime,   NULL },
#if HAVE_ATTR || HAVE_FLAGS
	{ ".attribute",  REFFILE, 0, r_attr,  NULL },
#endif
#if HAVE_ATTR
	{ ".project",    REFFILE, 0, r_proj,  NULL },
	{ ".generation", REFFILE, 0, r_gen,   NULL },
#endif
	{ ".len",      REFFILE, 0, r_strlen,  NULL },

	/* Pattern modifiers */

#ifdef FNM_CASEFOLD
	{ ".i",        PATMOD, 0, c_i,       NULL },
#endif
#ifdef HAVE_PCRE2
	{ ".re",       PATMOD, 0, c_re,      NULL },
	{ ".rei",      PATMOD, 0, c_rei,     NULL },
#endif

	{ ".path",     PATMOD, 0, c_path,    NULL },
#ifdef FNM_CASEFOLD
	{ ".ipath",    PATMOD, 0, c_ipath,   NULL },
#endif
#ifdef HAVE_PCRE2
	{ ".repath",   PATMOD, 0, c_repath,  NULL },
	{ ".reipath",  PATMOD, 0, c_reipath, NULL },
#endif

	{ ".link",     PATMOD, 0, c_link,    NULL },
#ifdef FNM_CASEFOLD
	{ ".ilink",    PATMOD, 0, c_ilink,   NULL },
#endif
#ifdef HAVE_PCRE2
	{ ".relink",   PATMOD, 0, c_relink,  NULL },
	{ ".reilink",  PATMOD, 0, c_reilink, NULL },
#endif

	{ ".body",     PATMOD, 0, c_body,    NULL },
#ifdef FNM_CASEFOLD
	{ ".ibody",    PATMOD, 0, c_ibody,   NULL },
#endif
#ifdef HAVE_PCRE2
	{ ".rebody",   PATMOD, 0, c_rebody,  NULL },
	{ ".reibody",  PATMOD, 0, c_reibody, NULL },
#endif

#ifdef HAVE_MAGIC
	{ ".what",     PATMOD, 0, c_what,     NULL },
#ifdef FNM_CASEFOLD
	{ ".iwhat",    PATMOD, 0, c_iwhat,    NULL },
#endif
#ifdef HAVE_PCRE2
	{ ".rewhat",   PATMOD, 0, c_rewhat,   NULL },
	{ ".reiwhat",  PATMOD, 0, c_reiwhat,  NULL },
#endif
#endif

#ifdef HAVE_MAGIC
	{ ".mime",     PATMOD, 0, c_mime,     NULL },
#ifdef FNM_CASEFOLD
	{ ".imime",    PATMOD, 0, c_imime,    NULL },
#endif
#ifdef HAVE_PCRE2
	{ ".remime",   PATMOD, 0, c_remime,   NULL },
	{ ".reimime",  PATMOD, 0, c_reimime,  NULL },
#endif
#endif

#ifdef HAVE_ACL
	{ ".acl",      PATMOD, 0, c_acl,     NULL },
#ifdef FNM_CASEFOLD
	{ ".iacl",     PATMOD, 0, c_iacl,    NULL },
#endif
#ifdef HAVE_PCRE2
	{ ".reacl",    PATMOD, 0, c_reacl,   NULL },
	{ ".reiacl",   PATMOD, 0, c_reiacl,  NULL },
#endif
#endif

#ifdef HAVE_EA
	{ ".ea",       PATMOD, 0, c_ea,      NULL },
#ifdef FNM_CASEFOLD
	{ ".iea",      PATMOD, 0, c_iea,     NULL },
#endif
#ifdef HAVE_PCRE2
	{ ".reea",     PATMOD, 0, c_reea,    NULL },
	{ ".reiea",    PATMOD, 0, c_reiea,   NULL },
#endif
#endif
	{ ".sh",       PATMOD, 0, c_sh,      NULL },
	{ ".ush",      PATMOD, 0, c_ush,     NULL },
};

/*

void rawhide_init(void);

Prepare the symbol table with built-in symbols.
Set the values for "now" and "today".
Must be called before the -h option is processed.

*/

void rawhide_init(void)
{
	symbol_t *sym;
	struct tm *tm;
	time_t t;
	int i;

	symbols = &init_syms[0];

	for (i = 0; i < sizeof(init_syms) / sizeof(symbol_t) - 1; i++)
		init_syms[i].next = &init_syms[i + 1];

	/* Initialize "now" to the time right now */

	sym = locate_symbol("now");
	sym->value = (llong)time(&t);

	/* Initialize "today" to the time last midnight (local time) */

	tm = localtime(&t);
	tm->tm_hour = tm->tm_min = tm->tm_sec = 0;
	tm->tm_isdst = -1;
	t = mktime(tm);

	sym = locate_symbol("today");
	sym->value = (llong)t;

	/* Initialize Program */

	PC = 0;
	startPC = -1;
}

/*

void rawhide_finish(void);

Free non-built-in symbols.
Must be called after parsing.
Can be called before searching.

*/

void rawhide_finish(void)
{
	symbol_t *s;

	while (symbols->type == PARAM || symbols->type == FUNCTION || symbols->type == IDENTIFIER)
	{
		s = symbols;
		symbols = symbols->next;
		free(s->name);
		free(s);
	}
}

/*

symbol_t *insert_symbol(char *name, int toktype, llong val);

Insert a new symbol into the symbol table.
The name parameter is its name.
The toktype parameter is its type.
The val parameter is its value, or zero.

Return a pointer to the symbol table entry.
The symbol is inserted at the head of the linked list.
This behaviour is relied upon elsewhere.

*/

symbol_t *insert_symbol(char *name, int toktype, llong val)
{
	symbol_t *sym;

	if (!(sym = malloc(sizeof(symbol_t))))
		return NULL;

	if (!(sym->name = strdup(name)))
		return free(sym), (void *)NULL;

	sym->type = toktype;
	sym->value = val;
	sym->next = symbols;
	symbols = sym;

	return sym;
}

/*

symbol_t *locate_symbol(char *name);

Search for a symbol in the symbol table.
The name parameter is the name of the symbol to search for.

*/

symbol_t *locate_symbol(char *name)
{
	symbol_t *s;

	for (s = symbols; s; s = s->next)
		if (!strcmp(name, s->name))
			return s;

	return NULL;
}

/*

symbol_t *locate_patmod_prefix(char *name);

Search for a pattern modifier symbol in the symbol table.
The name parameter is hopefully a unique prefix of a symbol,
but it is not a complete symbol.

*/

symbol_t *locate_patmod_prefix(char *name)
{
	symbol_t *s, *match = NULL;

	for (s = symbols; s; s = s->next)
	{
		if (s->type != PATMOD)
			continue;

		if (!strncmp(name, s->name, strlen(name)))
		{
			if (match)
				return NULL; /* Not a unique prefix */

			match = s;
		}
	}

	return match;
}

/*

int rawhide_instruction(void (*func)(llong), llong value);

Append a rawhide instruction to the Program array.
The func parameter is the instruction's implementation.
The value parameter is the argument to that function.

*/

int rawhide_instruction(void (*func)(llong), llong value)
{
	if (PC >= MAX_PROGRAM_SIZE)
		return -1;

	Program[PC].func = func;
	Program[PC++].value = value;

	return 0;
}

/*

llong rawhide_execute(void);

Execute the program stored in Program.
Each element of Program contains a pointer to a function.
The program is NULL-terminated.
Returns the value of the expression.

*/

llong rawhide_execute(void)
{
	for (SP = 0, PC = startPC; Program[PC].func; PC++)
	{
		(*Program[PC].func)(Program[PC].value);

		if (SP >= MAX_STACK_SIZE)
			fatal("stack overflow");
	}

	return Stack[0];
}

/* vi:set ts=4 sw=4: */
