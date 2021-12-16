#ifndef RH_H
#define RH_H

/* ----------------------------------------------------------------------
 * FILE: rh.h
 * VERSION: 2
 * Written by: Ken Stauffer
 * This header contains the #define's for the tokens.
 * It also contains structure definitions.
 * 
 * ---------------------------------------------------------------------- */

#include <stdio.h>

#define RHRC		"/.rhrc"	/* start up file */
#define RHENV		"RH"		/* rh environment variable */
#define HOMEENV		"HOME"		/* path to your home directory */

/* Definition of returned tokens from the lexical analyzer */

#define OR			256	/* || */
#define AND			257	/* && */
#define LE			258	/* <= */
#define GE			259	/* >= */
#define NE			260	/* != */
#define EQ			261	/* == */
#define SHIFTL		262	/* << */
#define SHIFTR		263	/* >> */
#define NOP			264	/* no-operation */
#define NUMBER		265	/* literal numbers and symbolic constants */
#define STR			266	/* regular expression strings */
#define FIELD		267	/* file fields (size,mode,nlinks) */
#define FUNCTION	268	/* user defined function */
#define RETURN		269	/* return keyword */
#define PARAM		270	/* parameter to functions */
#define IDENTIFIER	271

#ifndef MAX_PROGRAM_SIZE
#define MAX_PROGRAM_SIZE	20000	/* size of stack program */
#endif
#ifndef MAX_STACK_SIZE
#define MAX_STACK_SIZE		10000	/* size of stack */
#endif
#ifndef MAX_IDENT_LENGTH
#define MAX_IDENT_LENGTH	200		/* length of an identifier */
#endif
#ifndef MAX_STRBUF_SIZE
#define MAX_STRBUF_SIZE		2000	/* total chars available for strings */
#endif

/*
 * Structure of a "rh-assembly" instruction.
 *
 */

struct instr
{
	void	(*func)(long);
	long	value;
};

/*
 * Structure of a symbol.
 *
 */

struct symbol
{
	char			*name;
	int				type;
	long			value;
	void			(*func)(long);
	struct symbol	*next;
};

/*
 * Structure defining the rh runtime environment.
 *
 */

struct runtime
{
	struct stat	*buf;			/* stat info of current file */
	char		*fname;			/* file name of current file */
	int			depth;			/* relative depth of current file */
	void		(*func)(void);	/* examination function */
	int			prune;			/* flag to indicate prunning */
	int			verbose;		/* used by the (*func)() routine */
	char		*command;		/* command to exec for current file */
};

#ifndef DATA
extern struct symbol	*symbols;

extern struct symbol	*tokensym;
extern long				tokenval;
extern long				token;

extern struct instr		StackProgram[];
extern int				PC;
extern int				startPC;
extern long				Stack[];
extern int				SP;
extern int				FP;	/* frame pointer */

extern struct runtime	attr;

extern char				Strbuf[];
extern int				strfree;

extern char				*expstr;
extern char				*expfname;
extern FILE				*expfile;
#endif

#endif

/* vi:set ts=4 sw=4: */
