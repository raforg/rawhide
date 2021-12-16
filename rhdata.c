
/* ----------------------------------------------------------------------
 * FILE: rhdata.c
 * VERSION: 2
 * Written by: Ken Stauffer
 * This file contains the predefined symbol table, and related data
 * structures.
 *
 * ---------------------------------------------------------------------- */

#define DATA
#include "rh.h"
#include "rhcmds.h"
#include "rhparse.h"

#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

struct symbol	*symbols;

struct symbol	*tokensym;
long			tokenval;
long			token;

char			Strbuf[MAX_STRBUF_SIZE + 1];
int				strfree = 0;

struct instr	StackProgram[MAX_PROGRAM_SIZE];
int				PC;
int				startPC;

long			Stack[MAX_STACK_SIZE + 3];
int				SP;
int				FP;

struct runtime	attr;

/*
 * The following variables specify where the input is comming from.
 * If expstr == NULL then the input is certainly not from there, and
 * instead is taken from expfile.
 * else expstr is used as input.
 *
 */

char		*expstr;
FILE		*expfile;
char		*expfname;

static struct symbol init_syms[] =
{
	{ "NOW",	NUMBER,	0,			c_number,	NULL },
	{ "IFBLK",	NUMBER,	S_IFBLK,	c_number,	NULL },
	{ "IFCHR",	NUMBER,	S_IFCHR,	c_number,	NULL },
	{ "IFDIR",	NUMBER,	S_IFDIR,	c_number,	NULL },
	{ "IFMT",	NUMBER,	S_IFMT,		c_number,	NULL },
	{ "IFREG",	NUMBER,	S_IFREG,	c_number,	NULL },
	{ "ISGID",	NUMBER,	S_ISGID,	c_number,	NULL },
	{ "ISUID",	NUMBER,	S_ISUID,	c_number,	NULL },
	{ "ISVTX",	NUMBER,	S_ISVTX,	c_number,	NULL },
#ifdef S_IFLNK
	{ "IFLNK",	NUMBER,	S_IFLNK,	c_number,	NULL },
#endif
#ifdef S_IFSOCK
	{ "IFSOCK",	NUMBER,	S_IFSOCK,	c_number,	NULL },
#endif
#ifdef S_IFIFO
	{ "IFIFO",	NUMBER,	S_IFIFO,	c_number,	NULL },
#endif
	{ "atime",	FIELD,	0,			c_atime,	NULL },
	{ "ctime",	FIELD,	0,			c_ctime,	NULL },
	{ "dev",	FIELD,	0,			c_dev,		NULL },
	{ "gid",	FIELD,	0,			c_gid,		NULL },
	{ "ino",	FIELD,	0,			c_ino,		NULL },
	{ "mode",	FIELD,	0,			c_mode,		NULL },
	{ "mtime",	FIELD,	0,			c_mtime,	NULL },
	{ "nlink",	FIELD,	0,			c_nlink,	NULL },
	{ "rdev",	FIELD,	0,			c_rdev,		NULL },
	{ "size",	FIELD,	0,			c_size,		NULL },
	{ "uid",	FIELD,	0,			c_uid,		NULL },
	{ "depth",	FIELD,	0,			c_depth,	NULL },
	{ "prune",	FIELD,	0,			c_prune,	NULL },
	{ "days",	NUMBER,	24 * 3600,		c_number,	NULL },
	{ "weeks",	NUMBER,	24 * 3600 * 7,	c_number,	NULL },
	{ "hours",	NUMBER, 3600,		c_number,	NULL },
	{ "strlen",	FIELD,  0,			c_baselen,	NULL },
	{ "return",	RETURN,	0,			c_return,	NULL }
};

void rhinit(void)
{
	int i;
	struct symbol *s;

	symbols = &init_syms[0];

	for (i = 0; i < sizeof(init_syms) / sizeof(struct symbol) - 1; i++)
		init_syms[i].next = &init_syms[i + 1];

	/* Initialize the NOW variable to the time right now */

	s = locatename("NOW");
	s->value = time(NULL);
}

void rhfinish(void)
{
	struct symbol *s;

	while (symbols->type == PARAM || symbols->type == FUNCTION)
	{
		s = symbols;
		symbols = symbols->next;
		free(s->name);
		free(s);
	}
}

/* vi:set ts=4 sw=4: */
